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

#ifndef APRINTER_IP_STACK_NETWORK_H
#define APRINTER_IP_STACK_NETWORK_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include <aprinter/meta/WrapFunction.h>
#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/Callback.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Preprocessor.h>
#include <aprinter/structure/DoubleEndedList.h>
#include <aprinter/hal/common/EthernetCommon.h>
#include <aipstack/misc/Struct.h>
#include <aipstack/misc/Buf.h>
#include <aipstack/misc/Allocator.h>
#include <aipstack/proto/IpAddr.h>
#include <aipstack/ip/IpStack.h>
#include <aipstack/tcp/IpTcpProto.h>
#include <aipstack/eth/EthIpIface.h>

#include <aprinter/BeginNamespace.h>

template <typename Arg>
class IpStackNetwork {
    APRINTER_USE_TYPES1(Arg, (Context, ParentObject, Params))
    
    APRINTER_USE_TYPES2(AIpStack, (EthHeader, Ip4Header, Tcp4Header, StackBufAllocator,
                                   IpBufRef, IpBufNode, MacAddr, IpErr, Ip4Addr))
    
    APRINTER_USE_TYPES1(Params, (EthernetService, PcbIndexService))
    APRINTER_USE_VALS(Params, (NumArpEntries, ArpProtectCount, NumTcpPcbs, NumOosSegs,
                               LinkWithArrayIndices))
    
    static_assert(NumArpEntries >= 4, "");
    static_assert(ArpProtectCount >= 2, "");
    static_assert(NumTcpPcbs >= 2, "");
    static_assert(NumOosSegs >= 2 && NumOosSegs <= 255, "");
    
    static size_t const EthMTU = 1514;
    static size_t const TcpMaxMSS = EthMTU - EthHeader::Size - Ip4Header::Size - Tcp4Header::Size;
    static_assert(TcpMaxMSS == 1460, "");
    
public:
    struct Object;
    
private:
    static uint8_t const IpTTL = 64;
    
    using TheBufAllocator = StackBufAllocator;
    
    using TheIpStackService = AIpStack::IpStackService<
        EthHeader::Size, // HeaderBeforeIp
        IpTTL,           // IcmpTTL
        Arg::Params::MaxReassPackets,
        Arg::Params::MaxReassSize,
        typename Arg::Params::PathMtuParams
    >;
    APRINTER_MAKE_INSTANCE(TheIpStack, (TheIpStackService::template Compose<Context, TheBufAllocator>))
    
    using Iface = typename TheIpStack::Iface;
    
    using TheIpTcpProtoService = AIpStack::IpTcpProtoService<
        IpTTL,
        NumTcpPcbs,
        NumOosSegs,
        49152, // EphemeralPortFirst
        65535, // EphemeralPortLast
        PcbIndexService,
        LinkWithArrayIndices
    >;
    APRINTER_MAKE_INSTANCE(TheIpTcpProto, (TheIpTcpProtoService::template Compose<Context, TheBufAllocator, TheIpStack>))
    
    struct EthernetActivateHandler;
    struct EthernetLinkHandler;
    struct EthernetReceiveHandler;
    using TheEthernetClientParams = EthernetClientParams<EthernetActivateHandler, EthernetLinkHandler, EthernetReceiveHandler, IpBufRef>;
    APRINTER_MAKE_INSTANCE(TheEthernet, (EthernetService::template Ethernet<Context, Object, TheEthernetClientParams>))
    
    using TheEthIpIfaceService = AIpStack::EthIpIfaceService<
        NumArpEntries,
        ArpProtectCount,
        0 // HeaderBeforeEth
    >;
    APRINTER_MAKE_INSTANCE(TheEthIpIface, (TheEthIpIfaceService::template Compose<Context, TheBufAllocator, typename Iface::CallbackImpl>))
    
public:
    using TcpProto = TheIpTcpProto;
    
    enum EthActivateState {NOT_ACTIVATED, ACTIVATING, ACTIVATE_FAILED, ACTIVATED};
    
    struct NetworkParams {
        EthActivateState activation_state : 2; // for getStatus() only 
        bool link_up : 1; // for getStatus() only 
        bool dhcp_enabled : 1;
        uint8_t mac_addr[6];
        uint8_t ip_addr[4];
        uint8_t ip_netmask[4];
        uint8_t ip_gateway[4];
    };
    
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        TheEthernet::init(c);
        o->event_listeners.init();
        o->ip_stack.init();
        o->ip_tcp_proto.init(&o->ip_stack);
        o->activation_state = NOT_ACTIVATED;
        o->driver_proxy.clear();
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->event_listeners.isEmpty())
        
        if (o->activation_state == ACTIVATED) {
            o->ip_iface.deinit();
            o->eth_ip_iface.deinit();
        }
        o->ip_tcp_proto.deinit();
        o->ip_stack.deinit();
        TheEthernet::deinit(c);
    }
    
    static void activate (Context c, NetworkParams const *params)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->activation_state == NOT_ACTIVATED)
        
        MacAddr mac_addr = AIpStack::ReadSingleField<MacAddr>((char const *)params->mac_addr);
        
        o->activation_state = ACTIVATING;
        o->config = *params;
        TheEthernet::activate(c, mac_addr);
    }
    
    static void deactivate (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->activation_state != NOT_ACTIVATED)
        
        if (o->activation_state == ACTIVATED) {
            o->ip_iface.deinit();
            o->eth_ip_iface.deinit();
        }
        TheEthernet::reset(c);
        o->activation_state = NOT_ACTIVATED;
        o->driver_proxy.clear();
    }
    
    static bool isActivated (Context c)
    {
        auto *o = Object::self(c);
        
        return o->activation_state != NOT_ACTIVATED;
    }
    
    static TcpProto * getTcpProto (Context c)
    {
        auto *o = Object::self(c);
        
        return &o->ip_tcp_proto;
    }
    
    static NetworkParams getConfig (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->activation_state != NOT_ACTIVATED)
        
        return o->config;
    }
    
    static NetworkParams getStatus (Context c)
    {
        auto *o = Object::self(c);
        
        NetworkParams status = {};
        
        status.activation_state = o->activation_state;
        
        if (o->activation_state == ACTIVATED) {
            memcpy(status.mac_addr, TheEthernet::getMacAddr(c)->data, 6);
            status.link_up = TheEthernet::getLinkUp(c);
            
            auto addr_setting = o->ip_iface.getIp4Addr();
            if (addr_setting.present) {
                AIpStack::WriteSingleField<Ip4Addr>((char *)status.ip_addr, addr_setting.addr);
                AIpStack::WriteSingleField<Ip4Addr>((char *)status.ip_netmask, Ip4Addr::PrefixMask(addr_setting.prefix));
            }
            
            auto gw_setting = o->ip_iface.getIp4Gateway();
            if (gw_setting.present) {
                AIpStack::WriteSingleField<Ip4Addr>((char *)status.ip_gateway, gw_setting.addr);
            }
        }
        
        return status;
    }
    
    enum class NetworkEventType : uint8_t {ACTIVATION, LINK, DHCP};
    
    struct NetworkEvent {
        NetworkEventType type;
        union {
            struct {
                bool error;
            } activation;
            struct {
                bool up;
            } link;
            struct {
                bool up;
            } dhcp;
        };
    };
    
    class NetworkEventListener {
        friend IpStackNetwork;
        
    public:
        // WARNING: Don't do any funny calls back to this module directly from the event handler.
        // But it is permitted to reset/deinit this listener.
        using EventHandler = Callback<void(Context, NetworkEvent)>;
        
        void init (Context c, EventHandler event_handler)
        {
            m_event_handler = event_handler;
            m_listening = false;
        }
        
        void deinit (Context c)
        {
            reset(c);
        }
        
        void reset (Context c)
        {
            if (m_listening) {
                auto *o = Object::self(c);
                o->event_listeners.remove(this);
                m_listening = false;
            }
        }
        
        void startListening (Context c)
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(!m_listening)
            
            m_listening = true;
            o->event_listeners.prepend(this);
        }
        
    private:
        EventHandler m_event_handler;
        bool m_listening;
        DoubleEndedListNode<NetworkEventListener> m_node;
    };
    
    // Minimum required send buffer size for TCP.
    static size_t const MinTcpSendBufSize = 2 * TcpMaxMSS;
    
    // Minimum required receive buffer size for TCP.
    static size_t const MinTcpRecvBufSize = 2 * TcpMaxMSS;
    
    // How much less free TX buffer than its size we guarantee to provide to
    // the application eventually. See IpTcpProto::TcpConnection::getSndBufOverhead
    // for an explanation.
    static size_t const MaxTcpSndBufOverhead = TcpMaxMSS - 1;
    
    // Threshold for TCP window updates as a divisor of buffer size
    // desired to be used (to be configured by application code).
    static int const TcpWndUpdThrDiv = Params::TcpWndUpdThrDiv;
    static_assert(TcpWndUpdThrDiv >= 2, "");
    
private:
    using DriverCallbackImpl = typename TheEthIpIface::CallbackImpl;
    
    class EthDriverProxy : public AIpStack::EthIfaceDriver<DriverCallbackImpl> {
        friend IpStackNetwork;
        
        void clear ()
        {
            m_callback = nullptr;
        }
        
    public:
        void setCallback (AIpStack::EthIfaceDriverCallback<DriverCallbackImpl> *callback) override
        {
            m_callback = callback;
        }
        
        MacAddr const * getMacAddr () override
        {
            auto *o = Object::self(Context());
            AMBRO_ASSERT(o->activation_state == ACTIVATED)
            
            return TheEthernet::getMacAddr(Context());
        }
        
        size_t getEthMtu () override
        {
            auto *o = Object::self(Context());
            AMBRO_ASSERT(o->activation_state == ACTIVATED)
            
            return EthMTU;
        }
        
        IpErr sendFrame (IpBufRef frame) override
        {
            auto *o = Object::self(Context());
            AMBRO_ASSERT(o->activation_state == ACTIVATED)
            
            return TheEthernet::sendFrame(Context(), &frame);
        }
        
    private:
        AIpStack::EthIfaceDriverCallback<DriverCallbackImpl> *m_callback;
    };
    
    static void ethernet_activate_handler (Context c, bool error)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->activation_state == ACTIVATING)
        
        if (error) {
            o->activation_state = ACTIVATE_FAILED;
        } else {
            o->activation_state = ACTIVATED;
            
            o->eth_ip_iface.init(&o->driver_proxy);
            o->ip_iface.init(&o->ip_stack, &o->eth_ip_iface);
            
            if (!o->config.dhcp_enabled) {
                Ip4Addr addr    = AIpStack::ReadSingleField<Ip4Addr>((char const *)o->config.ip_addr);
                Ip4Addr netmask = AIpStack::ReadSingleField<Ip4Addr>((char const *)o->config.ip_netmask);
                Ip4Addr gateway = AIpStack::ReadSingleField<Ip4Addr>((char const *)o->config.ip_gateway);
                
                if (addr != Ip4Addr::ZeroAddr()) {
                    o->ip_iface.setIp4Addr(AIpStack::IpIfaceIp4AddrSetting{true, (uint8_t)netmask.countLeadingOnes(), addr});
                }
                if (gateway != Ip4Addr::ZeroAddr()) {
                    o->ip_iface.setIp4Gateway(AIpStack::IpIfaceIp4GatewaySetting{true, gateway});
                }
            }
        }
        
        NetworkEvent event{NetworkEventType::ACTIVATION};
        event.activation.error = error;
        raise_network_event(c, event);
    }
    struct EthernetActivateHandler : public AMBRO_WFUNC_TD(&IpStackNetwork::ethernet_activate_handler) {};
    
    static void ethernet_link_handler (Context c, bool link_status)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->activation_state == ACTIVATED)
        
        // TODO: Inform IP stack about link.
        
        NetworkEvent event{NetworkEventType::LINK};
        event.link.up = link_status;
        raise_network_event(c, event);
    }
    struct EthernetLinkHandler : public AMBRO_WFUNC_TD(&IpStackNetwork::ethernet_link_handler) {};
    
    static void ethernet_receive_handler (Context c, char *data1, char *data2, size_t size1, size_t size2)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->activation_state == ACTIVATED)
        AMBRO_ASSERT(o->driver_proxy.m_callback != nullptr)
        AMBRO_ASSERT(size2 == 0 || size1 > 0)
        
        if (size1 == 0) {
            return;
        }
        
        IpBufNode node2 = {data2, size2, nullptr};
        IpBufNode node1 = {data1, size1, &node2};        
        IpBufRef frame = {&node1, 0, (size_t)(size1 + size2)};
        
        o->driver_proxy.m_callback->recvFrame(frame);
    }
    struct EthernetReceiveHandler : public AMBRO_WFUNC_TD(&IpStackNetwork::ethernet_receive_handler) {};
    
    static void raise_network_event (Context c, NetworkEvent event)
    {
        auto *o = Object::self(c);
        
        NetworkEventListener *nel = o->event_listeners.first();
        while (nel) {
            AMBRO_ASSERT(nel->m_listening)
            auto handler = nel->m_event_handler;
            nel = o->event_listeners.next(nel);
            handler(c, event);
        }
    }
    
public:
    using GetEthernet = TheEthernet;
    
    struct Object : public ObjBase<IpStackNetwork, ParentObject, MakeTypeList<
        TheEthernet
    >> {
        DoubleEndedList<NetworkEventListener, &NetworkEventListener::m_node, false> event_listeners;
        TheIpStack ip_stack;
        TheIpTcpProto ip_tcp_proto;
        EthActivateState activation_state;
        EthDriverProxy driver_proxy;
        TheEthIpIface eth_ip_iface;
        Iface ip_iface;
        NetworkParams config;
    };
};

APRINTER_ALIAS_STRUCT_EXT(IpStackNetworkService, (
    APRINTER_AS_TYPE(EthernetService),
    APRINTER_AS_VALUE(int, NumArpEntries),
    APRINTER_AS_VALUE(int, ArpProtectCount),
    APRINTER_AS_VALUE(int, MaxReassPackets),
    APRINTER_AS_VALUE(uint16_t, MaxReassSize),
    APRINTER_AS_TYPE(PathMtuParams),
    APRINTER_AS_VALUE(int, NumTcpPcbs),
    APRINTER_AS_VALUE(int, NumOosSegs),
    APRINTER_AS_VALUE(int, TcpWndUpdThrDiv),
    APRINTER_AS_TYPE(PcbIndexService),
    APRINTER_AS_VALUE(bool, LinkWithArrayIndices)
), (
    APRINTER_ALIAS_STRUCT_EXT(Compose, (
        APRINTER_AS_TYPE(Context),
        APRINTER_AS_TYPE(ParentObject)
    ), (
        using Params = IpStackNetworkService;
        APRINTER_DEF_INSTANCE(Compose, IpStackNetwork)
    ))
))

#include <aprinter/EndNamespace.h>

#endif
