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
#include <aprinter/meta/ChooseInt.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Callback.h>
#include <aprinter/base/OneOf.h>
#include <aprinter/base/LoopUtils.h>
#include <aprinter/base/Preprocessor.h>
#include <aprinter/ipstack/misc/Struct.h>
#include <aprinter/ipstack/misc/Buf.h>
#include <aprinter/ipstack/proto/IpAddr.h>
#include <aprinter/ipstack/proto/EthernetProto.h>
#include <aprinter/ipstack/proto/ArpProto.h>
#include <aprinter/ipstack/ip/IpIfaceDriver.h>
#include <aprinter/ipstack/eth/EthIfaceDriver.h>

#include <aprinter/BeginNamespace.h>

template <typename Arg>
class EthIpIface : public IpIfaceDriver,
    private EthIfaceDriverCallback
{
    APRINTER_USE_VALS(Arg::Params, (NumArpEntries, ArpProtectCount, HeaderBeforeEth))
    APRINTER_USE_TYPES1(Arg, (Context, BufAllocator))
    
    APRINTER_USE_TYPE1(Context, Clock)
    APRINTER_USE_TYPE1(Clock, TimeType)
    
    static size_t const EthArpPktSize = EthHeader::Size + ArpIp4Header::Size;
    
    static_assert(NumArpEntries > 0, "");
    static_assert(ArpProtectCount >= 0, "");
    static_assert(ArpProtectCount <= NumArpEntries, "");
    
    static int const ArpNonProtectCount = NumArpEntries - ArpProtectCount;
    
    using ArpEntryIndexType = ChooseIntForMax<NumArpEntries, true>;
    
    static TimeType const ArpTimerTicks = 1.0 * Clock::time_freq;
    
    static uint8_t const ArpQueryTimeout = 3;
    static uint8_t const ArpValidTimeout = 60;
    static uint8_t const ArpRefreshTimeout = 3;
    
public:
    void init (EthIfaceDriver *driver)
    {
        m_arp_timer.init(Context(), APRINTER_CB_OBJFUNC_T(&EthIpIface::arp_timer_handler, this));
        m_driver = driver;
        m_callback = nullptr;
        
        m_driver->setCallback(this);
        m_mac_addr = m_driver->getMacAddr();
        
        m_first_arp_entry = 0;
        
        for (int i : LoopRangeAuto(NumArpEntries)) {
            m_arp_entries[i].next = (i < NumArpEntries-1) ? i+1 : -1;
            m_arp_entries[i].state = ArpEntryState::FREE;
            m_arp_entries[i].weak = true;
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
        ArpEntryIndexType next;
        uint8_t state : 2;
        bool weak : 1;
        uint8_t time_left;
        MacAddr mac_addr;
        Ip4Addr ip_addr;
    };
    
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
        
        ArpEntry *entry = get_arp_entry(ip_addr, false);
        
        if (entry->state == ArpEntryState::FREE) {
            entry->state = ArpEntryState::QUERY;
            entry->time_left = ArpQueryTimeout;
            send_arp_packet(ArpOpTypeRequest, MacAddr::BroadcastAddr(), ip_addr);
        }
        
        if (entry->state == ArpEntryState::QUERY) {
            return IpErr::ARP_QUERY;
        }
        
        if (entry->state == ArpEntryState::VALID && entry->time_left == 0) {
            entry->state = ArpEntryState::REFRESHING;
            entry->time_left = ArpRefreshTimeout;
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
            ArpEntry *entry = get_arp_entry(ip_addr, true);
            entry->state = ArpEntryState::VALID;
            entry->time_left = ArpValidTimeout;
            entry->mac_addr = mac_addr;
        }
    }
    
    ArpEntry * get_arp_entry (Ip4Addr ip_addr, bool weak)
    {
        ArpEntry *e;
        
        int index = m_first_arp_entry;
        int prev_index = -1;
        
        int num_hard = 0;
        int last_weak_index = -1;
        int last_weak_prev_index;
        int last_hard_index = -1;
        int last_hard_prev_index;
        
        while (index >= 0) {
            AMBRO_ASSERT(index < NumArpEntries)
            e = &m_arp_entries[index];
            
            if (e->state != ArpEntryState::FREE && e->ip_addr == ip_addr) {
                break;
            }
            
            if (e->weak) {
                last_weak_index = index;
                last_weak_prev_index = prev_index;
            } else {
                num_hard++;
                last_hard_index = index;
                last_hard_prev_index = prev_index;
            }
            
            prev_index = index;
            index = e->next;
        }
        
        if (index >= 0) {
            if (!weak) {
                e->weak = false;
            }
        } else {
            bool use_weak;
            if (last_weak_index >= 0 && m_arp_entries[last_weak_index].state == ArpEntryState::FREE) {
                use_weak = true;
            } else {
                if (weak) {
                    use_weak = !(num_hard > ArpProtectCount || last_weak_index < 0);
                } else {
                    int num_weak = NumArpEntries - num_hard;
                    use_weak = (num_weak > ArpNonProtectCount || last_hard_index < 0);
                }
            }
            
            if (use_weak) {
                index = last_weak_index;
                prev_index = last_weak_prev_index;
            } else {
                index = last_hard_index;
                prev_index = last_hard_prev_index;
            }
            
            AMBRO_ASSERT(index >= 0)
            e = &m_arp_entries[index];
            
            e->state = ArpEntryState::FREE;
            e->ip_addr = ip_addr;
            e->weak = weak;
        }
        
        if (prev_index >= 0) {
            m_arp_entries[prev_index].next = e->next;
            e->next = m_first_arp_entry;
            m_first_arp_entry = index;
        }
        
        return e;
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
            switch (e.state) {
                case ArpEntryState::QUERY: {
                    e.time_left--;
                    if (e.time_left == 0) {
                        e.state = ArpEntryState::FREE;
                    } else {
                        send_arp_packet(ArpOpTypeRequest, MacAddr::BroadcastAddr(), e.ip_addr);
                    }
                } break;
                
                case ArpEntryState::VALID: {
                    if (e.time_left > 0) {
                        e.time_left--;
                    }
                } break;
                
                case ArpEntryState::REFRESHING: {
                    e.time_left--;
                    if (e.time_left == 0) {
                        e.state = ArpEntryState::QUERY;
                        e.time_left = ArpQueryTimeout;
                        send_arp_packet(ArpOpTypeRequest, MacAddr::BroadcastAddr(), e.ip_addr);
                    } else {
                        send_arp_packet(ArpOpTypeRequest, e.mac_addr, e.ip_addr);
                    }
                } break;
            }
        }
    }
    
private:
    typename Context::EventLoop::TimedEvent m_arp_timer;
    EthIfaceDriver *m_driver;
    IpIfaceDriverCallback *m_callback;
    MacAddr const *m_mac_addr;
    ArpEntryIndexType m_first_arp_entry;
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
