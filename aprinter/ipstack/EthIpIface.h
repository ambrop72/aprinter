/*
 * Copyright (c) 2016 Ambroz Bizjak
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef APRINTER_IPSTACK_ETH_IP_IFACE_H
#define APRINTER_IPSTACK_ETH_IP_IFACE_H

#include <stddef.h>
#include <stdint.h>

#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Callback.h>
#include <aprinter/base/OneOf.h>
#include <aprinter/ipstack/Struct.h>
#include <aprinter/ipstack/Buf.h>
#include <aprinter/ipstack/IpAddr.h>
#include <aprinter/ipstack/EthIfaceDriver.h>
#include <aprinter/ipstack/IpIfaceDriver.h>
#include <aprinter/ipstack/proto/EthernetProto.h>
#include <aprinter/ipstack/proto/ArpProto.h>

#include <aprinter/BeginNamespace.h>

template <typename Arg>
class EthIpIface : public IpIfaceDriver,
    private EthIfaceDriverCallback
{
    static int    const NumArpEntries   = Arg::Params::NumArpEntries;
    static int    const ArpProtectCount = Arg::Params::ArpProtectCount;
    static size_t const HeaderBeforeEth = Arg::Params::HeaderBeforeEth;
    
    using Context      = typename Arg::Context;
    using BufAllocator = typename Arg::BufAllocator;
    
    using Clock = typename Context::Clock;
    using TimeType = typename Clock::TimeType;
    
    static size_t const EthArpPktSize = EthHeader::Size + ArpIp4Header::Size;
    
    static_assert(NumArpEntries > 0, "");
    static_assert(ArpProtectCount >= 0, "");
    static_assert(ArpProtectCount <= NumArpEntries, "");
    
    static int const ArpNonProtectCount = NumArpEntries - ArpProtectCount;
    
    static TimeType const ArpTimerTicks = 5.0 * Clock::time_freq;
    
    static uint8_t const QueryEntryTimeoutAge = 3;
    static uint8_t const ValidEntryRefreshAge = 12;
    static uint8_t const RefreshingEntryBroadcastAge = 3;
    static uint8_t const RefreshingEntryTimeoutAge = 5;
    
public:
    void init (EthIfaceDriver *driver)
    {
        m_arp_timer.init(Context(), APRINTER_CB_OBJFUNC_T(&EthIpIface::arp_timer_handler, this));
        m_driver = driver;
        m_callback = nullptr;
        
        m_driver->setCallback(this);
        m_mac_addr = m_driver->getMacAddr();
        
        for (auto &e : m_arp_entries) {
            e.state = ArpEntryState::FREE;
        }
        
        m_arp_timer.appendAfter(Context(), ArpTimerTicks);
    }
    
    void deinit ()
    {
        m_driver->setCallback(nullptr);
        m_arp_timer.deinit(Context());
    }
    
public: // IpIfaceDriver
    void setCallback (IpIfaceDriverCallback *callback) override
    {
        m_callback = callback;
    }
    
    size_t getIpMtu () override
    {
        size_t eth_mtu = m_driver->getEthMtu();
        AMBRO_ASSERT(eth_mtu >= EthHeader::Size)
        return eth_mtu - EthHeader::Size;
    }
    
    IpErr sendIp4Packet (IpBufRef pkt, Ip4Addr ip_addr) override
    {
        MacAddr dst_mac;
        IpErr resolve_err = resolve_hw_addr(ip_addr, &dst_mac);
        if (resolve_err != IpErr::SUCCESS) {
            return resolve_err;
        }
        
        IpBufRef frame;
        if (!pkt.revealHeader(EthHeader::Size, &frame)) {
            return IpErr::NO_HEADER_SPACE;
        }
        
        auto eth_header = EthHeader::MakeRef(frame.getChunkPtr());
        eth_header.set(EthHeader::DstMac(),  dst_mac);
        eth_header.set(EthHeader::SrcMac(),  *m_mac_addr);
        eth_header.set(EthHeader::EthType(), EthTypeIpv4);
        
        return m_driver->sendFrame(frame);
    }
    
private: // EthIfaceDriverCallback
    void recvFrame (IpBufRef frame) override
    {
        if (!frame.hasHeader(EthHeader::Size)) {
            return;
        }
        auto eth_header = EthHeader::MakeRef(frame.getChunkPtr());
        MacAddr dst_mac  = eth_header.get(EthHeader::DstMac());
        MacAddr src_mac  = eth_header.get(EthHeader::SrcMac());
        uint16_t ethtype = eth_header.get(EthHeader::EthType());
        
        if (!(dst_mac == *m_mac_addr || dst_mac == MacAddr::BroadcastAddr())) {
            return;
        }
        
        auto pkt = frame.hideHeader(EthHeader::Size);
        
        if (ethtype == EthTypeIpv4) {
            m_callback->recvIp4Packet(pkt);
        }
        else if (ethtype == EthTypeArp) {
            if (!pkt.hasHeader(ArpIp4Header::Size)) {
                return;
            }
            auto arp_header = ArpIp4Header::MakeRef(pkt.getChunkPtr());
            
            if (arp_header.get(ArpIp4Header::HwType())       != ArpHwTypeEth  ||
                arp_header.get(ArpIp4Header::ProtoType())    != EthTypeIpv4   ||
                arp_header.get(ArpIp4Header::HwAddrLen())    != MacAddr::Size ||
                arp_header.get(ArpIp4Header::ProtoAddrLen()) != Ip4Addr::Size ||
                arp_header.get(ArpIp4Header::SrcHwAddr())    != src_mac)
            {
                return;
            }
            
            uint16_t op_type    = arp_header.get(ArpIp4Header::OpType());
            Ip4Addr src_ip_addr = arp_header.get(ArpIp4Header::SrcProtoAddr());
            
            save_hw_addr(src_ip_addr, src_mac);
            
            if (op_type == ArpOpTypeRequest) {
                IpIfaceIp4Addrs const *ifaddr = m_callback->getIp4Addrs();
                
                if (ifaddr != nullptr &&
                    arp_header.get(ArpIp4Header::DstProtoAddr()) == ifaddr->addr)
                {
                    send_arp_packet(ArpOpTypeReply, src_mac, src_ip_addr);
                }
            }
        }
    }
    
private:
    struct ArpEntryState { enum : uint8_t {FREE, QUERY, VALID, REFRESHING}; };
    
    struct ArpEntry {
        uint8_t state : 2;
        bool weak : 1;
        uint8_t age : 5;
        MacAddr mac_addr;
        Ip4Addr ip_addr;
        TimeType last_use_time;
    };
    
    static uint8_t const MaxEntryAge = 31;
    
    IpErr resolve_hw_addr (Ip4Addr ip_addr, MacAddr *mac_addr)
    {
        if (ip_addr == Ip4Addr::AllOnesAddr()) {
            *mac_addr = MacAddr::BroadcastAddr();
            return IpErr::SUCCESS;
        }
        
        IpIfaceIp4Addrs const *ifaddr = m_callback->getIp4Addrs();
        if (ifaddr == nullptr) {
            return IpErr::NO_HW_ROUTE;
        }
        
        if ((ip_addr & ifaddr->netmask) != ifaddr->netaddr) {
            return IpErr::NO_HW_ROUTE;
        }
        
        if (ip_addr == ifaddr->bcastaddr) {
            *mac_addr = MacAddr::BroadcastAddr();
            return IpErr::SUCCESS;
        }
        
        ArpEntry *entry = find_arp_entry(ip_addr);
        
        if (entry == nullptr) {
            entry = alloc_arp_entry(ip_addr, false);
            entry->state = ArpEntryState::QUERY;
            entry->age = 0;
        } else {
            entry->weak = false;
            update_entry_last_use(entry);
        }
        
        if (entry->state == ArpEntryState::QUERY) {
            send_arp_packet(ArpOpTypeRequest, MacAddr::BroadcastAddr(), ip_addr);
            return IpErr::ARP_QUERY;
        }
        
        if (entry->state == ArpEntryState::VALID && entry->age >= ValidEntryRefreshAge) {
            entry->state = ArpEntryState::REFRESHING;
            entry->age = 0;
            send_arp_packet(ArpOpTypeRequest, entry->mac_addr, entry->ip_addr);
        }
        
        *mac_addr = entry->mac_addr;
        return IpErr::SUCCESS;
    }
    
    void save_hw_addr (Ip4Addr ip_addr, MacAddr mac_addr)
    {
        IpIfaceIp4Addrs const *ifaddr = m_callback->getIp4Addrs();
        
        if (ifaddr != nullptr &&
            (ip_addr & ifaddr->netmask) == ifaddr->netaddr &&
            ip_addr != ifaddr->bcastaddr)
        {
            ArpEntry *entry = find_arp_entry(ip_addr);
            if (entry == nullptr) {
                entry = alloc_arp_entry(ip_addr, true);
            } else {
                if (entry->weak) {
                    update_entry_last_use(entry);
                }
            }
            entry->state = ArpEntryState::VALID;
            entry->age = 0;
            entry->mac_addr = mac_addr;
        }
    }
    
    ArpEntry * find_arp_entry (Ip4Addr ip_addr)
    {
        for (auto &e : m_arp_entries) {
            if (e.state != ArpEntryState::FREE && e.ip_addr == ip_addr) {
                return &e;
            }
        }
        return nullptr;
    }
    
    ArpEntry * alloc_arp_entry (Ip4Addr ip_addr, bool weak)
    {
        TimeType now = Clock::getTime(Context());
        
        ArpEntry *entry = nullptr;
        ArpEntry *weak_entry = nullptr;
        ArpEntry *hard_entry = nullptr;
        int num_hard = 0;
        
        for (auto &e : m_arp_entries) {
            if (e.state == ArpEntryState::FREE) {
                entry = &e;
                break;
            }
            
            if (e.weak) {
                if (weak_entry == nullptr || entry_is_older(&e, weak_entry, now)) {
                    weak_entry = &e;
                }
            } else {
                if (hard_entry == nullptr || entry_is_older(&e, hard_entry, now)) {
                    hard_entry = &e;
                }
                num_hard++;
            }
        }
        
        if (entry == nullptr) {
            if (weak) {
                if (num_hard > ArpProtectCount || weak_entry == nullptr) {
                    entry = hard_entry;
                } else {
                    entry = weak_entry;
                }
            } else {
                int num_weak = NumArpEntries - num_hard;
                if (num_weak > ArpNonProtectCount || hard_entry == nullptr) {
                    entry = weak_entry;
                } else {
                    entry = hard_entry;
                }
            }
            AMBRO_ASSERT(entry != nullptr)
        }
        
        entry->ip_addr = ip_addr;
        entry->weak = weak;
        entry->last_use_time = now;
        
        return entry;
    }
    
    void update_entry_last_use (ArpEntry *entry)
    {
        entry->last_use_time = Clock::getTime(Context());
    }
    
    void send_arp_packet (uint16_t op_type, MacAddr dst_mac, Ip4Addr dst_ipaddr)
    {
        TxAllocHelper<BufAllocator, EthArpPktSize, HeaderBeforeEth> frame_alloc(EthArpPktSize);
        
        auto eth_header = EthHeader::MakeRef(frame_alloc.getPtr());
        eth_header.set(EthHeader::DstMac(), dst_mac);
        eth_header.set(EthHeader::SrcMac(), *m_mac_addr);
        eth_header.set(EthHeader::EthType(), EthTypeArp);
        
        IpIfaceIp4Addrs const *ifaddr = m_callback->getIp4Addrs();
        Ip4Addr src_addr = (ifaddr != nullptr) ? ifaddr->addr : Ip4Addr::ZeroAddr();
        
        auto arp_header = ArpIp4Header::MakeRef(frame_alloc.getPtr() + EthHeader::Size);
        arp_header.set(ArpIp4Header::HwType(),       ArpHwTypeEth);
        arp_header.set(ArpIp4Header::ProtoType(),    EthTypeIpv4);
        arp_header.set(ArpIp4Header::HwAddrLen(),    MacAddr::Size);
        arp_header.set(ArpIp4Header::ProtoAddrLen(), Ip4Addr::Size);
        arp_header.set(ArpIp4Header::OpType(),       op_type);
        arp_header.set(ArpIp4Header::SrcHwAddr(),    *m_mac_addr);
        arp_header.set(ArpIp4Header::SrcProtoAddr(), src_addr);
        arp_header.set(ArpIp4Header::DstHwAddr(),    dst_mac);
        arp_header.set(ArpIp4Header::DstProtoAddr(), dst_ipaddr);
        
        m_driver->sendFrame(frame_alloc.getBufRef());
    }
    
    void arp_timer_handler (Context)
    {
        m_arp_timer.appendAfter(Context(), ArpTimerTicks);
        
        for (auto &e : m_arp_entries) {
            if (e.state == ArpEntryState::FREE) {
                continue;
            }
            
            if (e.age < MaxEntryAge) {
                e.age++;
            }
            
            if (e.state == ArpEntryState::QUERY) {
                if (e.age >= QueryEntryTimeoutAge) {
                    e.state = ArpEntryState::FREE;
                }
                else {
                    send_arp_packet(ArpOpTypeRequest, MacAddr::BroadcastAddr(), e.ip_addr);
                }
            }
            else if (e.state == ArpEntryState::REFRESHING) {
                if (e.age >= RefreshingEntryTimeoutAge) {
                    e.state = ArpEntryState::FREE;
                }
                if (e.age >= RefreshingEntryBroadcastAge) {
                    send_arp_packet(ArpOpTypeRequest, MacAddr::BroadcastAddr(), e.ip_addr);
                }
                else {
                    send_arp_packet(ArpOpTypeRequest, e.mac_addr, e.ip_addr);
                }
            }
        }
    }
    
    static bool entry_is_older (ArpEntry *e1, ArpEntry *e2, TimeType now)
    {
        return (TimeType)(now - e1->last_use_time) > (TimeType)(now - e2->last_use_time);
    }
    
private:
    typename Context::EventLoop::TimedEvent m_arp_timer;
    EthIfaceDriver *m_driver;
    IpIfaceDriverCallback *m_callback;
    MacAddr const *m_mac_addr;
    ArpEntry m_arp_entries[NumArpEntries];
};

APRINTER_ALIAS_STRUCT_EXT(EthIpIfaceService, (
    APRINTER_AS_VALUE(int,    NumArpEntries),
    APRINTER_AS_VALUE(int,    ArpProtectCount),
    APRINTER_AS_VALUE(size_t, HeaderBeforeEth)
), (
    APRINTER_ALIAS_STRUCT_EXT(Compose, (
        APRINTER_AS_TYPE(Context),
        APRINTER_AS_TYPE(BufAllocator)
    ), (
        using Params = EthIpIfaceService;
        APRINTER_DEF_INSTANCE(Compose, EthIpIface)
    ))
))

#include <aprinter/EndNamespace.h>

#endif
