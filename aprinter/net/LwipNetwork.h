/*
 * Copyright (c) 2015 Ambroz Bizjak
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

#ifndef APRINTER_LWIP_NETWORK_H
#define APRINTER_LWIP_NETWORK_H

//#define APRINTER_DEBUG_NETWORK

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>

#include <lwip/init.h>
#include <lwip/timers.h>
#include <lwip/netif.h>
#include <lwip/ip_addr.h>
#include <lwip/err.h>
#include <lwip/pbuf.h>
#include <lwip/snmp.h>
#include <lwip/stats.h>
#include <lwip/dhcp.h>
#include <netif/etharp.h>

#include <aprinter/meta/WrapFunction.h>
#include <aprinter/meta/TypeListUtils.h>
#include <aprinter/meta/MemberType.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/Callback.h>
#include <aprinter/base/Assert.h>
#include <aprinter/hal/common/EthernetCommon.h>
#include <aprinter/printer/Console.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, typename EthernetService>
class LwipNetwork {
public:
    struct Object;
    
private:
    using TimeType = typename Context::Clock::TimeType;
    
    struct EthernetActivateHandler;
    struct EthernetLinkHandler;
    struct EthernetReceiveHandler;
    class EthernetSendBuffer;
    class EthernetRecvBuffer;
    using TheEthernetClientParams = EthernetClientParams<EthernetActivateHandler, EthernetLinkHandler, EthernetReceiveHandler, EthernetSendBuffer, EthernetRecvBuffer>;
    using TheEthernet = typename EthernetService::template Ethernet<Context, Object, TheEthernetClientParams>;
    
    static TimeType const TimeoutsIntervalTicks = 0.1 * Context::Clock::time_freq;
    
    APRINTER_DEFINE_MEMBER_TYPE(MemberType_EventLoopFastEvents, EventLoopFastEvents)
    
public:
    struct ActivateParams {
        uint8_t const *mac_addr;
    };
    
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        TheEthernet::init(c);
        o->timeouts_event.init(c, APRINTER_CB_STATFUNC_T(&LwipNetwork::timeouts_event_handler));
        o->net_activated = false;
        o->eth_activated = false;
    }
    
    // Note, deinit doesn't really work due to lwIP.
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        
        o->timeouts_event.deinit(c);
        TheEthernet::deinit(c);
    }
    
    static void activate (Context c, ActivateParams params)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(!o->net_activated)
        
        init_lwip(c, params);
        o->net_activated = true;
        TheEthernet::activate(c, o->netif.hwaddr);
        o->timeouts_event.appendNowNotAlready(c);
    }
    
    static bool isActivated (Context c)
    {
        auto *o = Object::self(c);
        return o->net_activated;
    }
    
    /*
    class TcpListener {
    public:
        using AcceptHandler = Callback<Context>;
        
        void init (Context c, AcceptHandler accept_handler)
        {
            m_accept_handler = accept_handler;
        }
        
        void deinit (Context c)
        {
            
        }
        
        void reset (Context c)
        {
        }
        
        bool start_listening (Context c, uint16_t port)
        {
            return false;
        }
        
    private:
        AcceptHandler m_accept_handler;
    };
    
    class TcpSocket {
    public:
        using RecvHandler = Callback<Context>;
        using SendHandler = Callback<Context>;
        
        void init (Context c, RecvHandler recv_handler, SendHandler send_handler)
        {
            m_recv_handler = recv_handler;
            m_send_handler = send_handler;
        }
        
        bool accept_connection (Context c, TcpListener *listener)
        {
            return false;
        }
        
        void deinit (Context c)
        {
        }
        
        void reset (Context c)
        {
        }
        
        bool receive (Context c, char *buffer, size_t *out_length)
        {
            return false;
        }
        
    private:
        RecvHandler m_recv_handler;
        SendHandler m_send_handler;
    };
    */
    
private:
    static void init_lwip (Context c, ActivateParams params)
    {
        auto *o = Object::self(c);
        
        lwip_init();
        
        ip_addr_t the_ipaddr;
        ip_addr_t the_netmask;
        ip_addr_t the_gw;
        ip_addr_set_zero(&the_ipaddr);
        ip_addr_set_zero(&the_netmask);
        ip_addr_set_zero(&the_gw);
        
        memset(&o->netif, 0, sizeof(o->netif));
        
        netif_add(&o->netif, &the_ipaddr, &the_netmask, &the_gw, &params, &LwipNetwork::netif_if_init, ethernet_input);
        netif_set_up(&o->netif);
        netif_set_default(&o->netif);
        dhcp_start(&o->netif);
    }
    
    static err_t netif_if_init (struct netif *netif)
    {
        ActivateParams const *params = (ActivateParams const *)netif->state;
        
        netif->hostname = (char *)"aprinter";
        
        uint32_t link_speed_for_mib2 = UINT32_C(100000000);
        MIB2_INIT_NETIF(netif, snmp_ifType_ethernet_csmacd, link_speed_for_mib2);
        
        netif->name[0] = 'e';
        netif->name[1] = 'n';
        
        netif->output = etharp_output;
        netif->linkoutput = &LwipNetwork::netif_link_output;
        
        netif->hwaddr_len = ETHARP_HWADDR_LEN;
        memcpy(netif->hwaddr, params->mac_addr, ETHARP_HWADDR_LEN);
        
        netif->mtu = 1500;
        netif->flags = NETIF_FLAG_BROADCAST|NETIF_FLAG_ETHARP;
        netif->state = nullptr;
        
        return ERR_OK;
    }
    
    static void timeouts_event_handler (Context c)
    {
        auto *o = Object::self(c);
        
        o->timeouts_event.appendAfterPrevious(c, TimeoutsIntervalTicks);
        sys_check_timeouts();
    }
    
    static void ethernet_activate_handler (Context c, bool error)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(!o->eth_activated)
        
        if (error) {
            APRINTER_CONSOLE_MSG("//EthActivateErr");
            return;
        }
        o->eth_activated = true;
    }
    struct EthernetActivateHandler : public AMBRO_WFUNC_TD(&LwipNetwork::ethernet_activate_handler) {};
    
    static void ethernet_link_handler (Context c, bool link_status)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->eth_activated)
        
        if (link_status) {
            APRINTER_CONSOLE_MSG("//EthLinkUp");
        } else {
            APRINTER_CONSOLE_MSG("//EthLinkDown");
        }
        
        if (link_status) {
            netif_set_link_up(&o->netif);
        } else {
            netif_set_link_down(&o->netif);
        }
    }
    struct EthernetLinkHandler : public AMBRO_WFUNC_TD(&LwipNetwork::ethernet_link_handler) {};
    
    static err_t netif_link_output (struct netif *netif, struct pbuf *p)
    {
        Context c;
        auto *o = Object::self(c);
        
#if ETH_PAD_SIZE
        pbuf_header(p, -ETH_PAD_SIZE);
#endif
        err_t ret = ERR_BUF;
        
        debug_print_pbuf(c, "Tx", p);
        
        if (!o->eth_activated) {
            goto out;
        }
        
        EthernetSendBuffer send_buf;
        send_buf.m_total_len = p->tot_len;
        send_buf.m_current_pbuf = p;
        
        if (!TheEthernet::sendFrame(c, &send_buf)) {
            goto out;
        }
        
        LINK_STATS_INC(link.xmit);
        ret = ERR_OK;
        
    out:
#if ETH_PAD_SIZE
        pbuf_header(p, ETH_PAD_SIZE);
#endif
        return ret;
    }
    
    class EthernetSendBuffer {
        friend LwipNetwork;
        
    public:
        size_t getTotalLength ()
        {
            return m_total_len;
        }
        
        size_t getChunkLength ()
        {
            AMBRO_ASSERT(m_current_pbuf)
            return m_current_pbuf->len;
        }
        
        char const * getChunkPtr ()
        {
            AMBRO_ASSERT(m_current_pbuf)
            return (char const *)m_current_pbuf->payload;
        }
        
        bool nextChunk ()
        {
            AMBRO_ASSERT(m_current_pbuf)
            m_current_pbuf = m_current_pbuf->next;
            return m_current_pbuf != nullptr;
        }
        
    private:
        size_t m_total_len;
        struct pbuf *m_current_pbuf;
    };
    
    static void ethernet_receive_handler (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->eth_activated)
        
        while (true) {
            EthernetRecvBuffer recv_buf;
            recv_buf.m_first_pbuf = nullptr;
            recv_buf.m_current_pbuf = nullptr;
            recv_buf.m_valid = false;
            
            if (!TheEthernet::recvFrame(c, &recv_buf)) {
                if (recv_buf.m_first_pbuf) {
                    pbuf_free(recv_buf.m_first_pbuf);
                }
                return;
            }
            
            if (!recv_buf.m_valid) {
                if (recv_buf.m_first_pbuf) {
                    pbuf_free(recv_buf.m_first_pbuf);
                }
                LINK_STATS_INC(link.memerr);
                LINK_STATS_INC(link.drop);
                continue;
            }
            
            AMBRO_ASSERT(recv_buf.m_first_pbuf)
            AMBRO_ASSERT(!recv_buf.m_current_pbuf)
            struct pbuf *p = recv_buf.m_first_pbuf;
            
            debug_print_pbuf(c, "Rx", p);
            
#if ETH_PAD_SIZE
            pbuf_header(p, ETH_PAD_SIZE);
#endif
            LINK_STATS_INC(link.recv);
            
            if (o->netif.input(p, &o->netif) != ERR_OK) {
                pbuf_free(p);
            }
        }
    }
    struct EthernetReceiveHandler : public AMBRO_WFUNC_TD(&LwipNetwork::ethernet_receive_handler) {};
    
    class EthernetRecvBuffer {
        friend LwipNetwork;
        
    public:
        bool allocate (size_t length)
        {
            AMBRO_ASSERT(!m_first_pbuf)
            
            struct pbuf *p = pbuf_alloc(PBUF_RAW, ETH_PAD_SIZE + length, PBUF_POOL);
            if (!p) {
                return false;
            }
#if ETH_PAD_SIZE
            pbuf_header(p, -ETH_PAD_SIZE);
#endif
            m_first_pbuf = p;
            m_current_pbuf = p;
            return true;
        }
        
        size_t getChunkLength ()
        {
            AMBRO_ASSERT(m_current_pbuf)
            return m_current_pbuf->len;
        }
        
        char * getChunkPtr ()
        {
            AMBRO_ASSERT(m_current_pbuf)
            return (char *)m_current_pbuf->payload;
        }
        
        bool nextChunk ()
        {
            AMBRO_ASSERT(m_current_pbuf)
            m_current_pbuf = m_current_pbuf->next;
            return m_current_pbuf != nullptr;
        }
        
        void setValid ()
        {
            m_valid = true;
        }
        
    private:
        struct pbuf *m_first_pbuf;
        struct pbuf *m_current_pbuf;
        bool m_valid;
    };
    
    static void debug_print_pbuf (Context c, char const *event, struct pbuf *p)
    {
#ifdef APRINTER_DEBUG_NETWORK
        auto *out = Context::Printer::get_msg_output(c);
        out->reply_append_str(c, "//");
        out->reply_append_str(c, event);
        out->reply_append_str(c, " tot_len=");
        out->reply_append_uint32(c, p->tot_len);
        out->reply_append_str(c, " data=");
        for (struct pbuf *q = p; q; q = q->next) {
            for (size_t i = 0; i < q->len; i++) {
                char s[4];
                sprintf(s, " %02" PRIX8, ((uint8_t *)q->payload)[i]);
                out->reply_append_str(c, s);
            }
        }
        out->reply_append_ch(c, '\n');
        out->reply_poke(c);
#endif
    }
    
public:
    using EventLoopFastEvents = ObjCollect<MakeTypeList<LwipNetwork>, MemberType_EventLoopFastEvents, true>;
    
    using GetEthernet = TheEthernet;
    
    struct Object : public ObjBase<LwipNetwork, ParentObject, MakeTypeList<
        TheEthernet
    >> {
        typename Context::EventLoop::TimedEvent timeouts_event;
        bool net_activated;
        bool eth_activated;
        struct netif netif;
    };
};

#include <aprinter/EndNamespace.h>

#endif
