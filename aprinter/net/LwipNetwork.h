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
#include <lwip/tcp.h>
#include <netif/etharp.h>

#include <aprinter/meta/WrapFunction.h>
#include <aprinter/meta/TypeListUtils.h>
#include <aprinter/meta/MinMax.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/Callback.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/WrapBuffer.h>
#include <aprinter/base/BinaryTools.h>
#include <aprinter/structure/DoubleEndedList.h>
#include <aprinter/hal/common/EthernetCommon.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, typename EthernetService>
class LwipNetwork {
public:
    struct Object;
    class TcpConnection;
    
private:
    struct EthernetActivateHandler;
    struct EthernetLinkHandler;
    struct EthernetReceiveHandler;
    class EthernetSendBuffer;
    using TheEthernetClientParams = EthernetClientParams<EthernetActivateHandler, EthernetLinkHandler, EthernetReceiveHandler, EthernetSendBuffer>;
    using TheEthernet = typename EthernetService::template Ethernet<Context, Object, TheEthernetClientParams>;
    
    using TimeoutsFastEvent = typename Context::EventLoop::template FastEventSpec<LwipNetwork>;
    
    static size_t const RxPbufPayloadSize = PBUF_POOL_BUFSIZE - ETH_PAD_SIZE;
    
public:
    struct NetworkParams {
        uint8_t mac_addr[6];
        bool link_up; // for getStatus() only 
        bool dhcp_enabled;
        uint8_t ip_addr[4];
        uint8_t ip_netmask[4];
        uint8_t ip_gateway[4];
    };
    
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        TheEthernet::init(c);
        o->event_listeners.init();
        Context::EventLoop::template initFastEvent<TimeoutsFastEvent>(c, LwipNetwork::timeouts_event_handler);
        o->net_activated = false;
        o->eth_activated = false;
        lwip_init();
        o->rx_pbuf = nullptr;
        Context::EventLoop::template triggerFastEvent<TimeoutsFastEvent>(c);
    }
    
    // Note, deinit doesn't really work due to lwIP.
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->event_listeners.isEmpty())
        
        Context::EventLoop::template resetFastEvent<TimeoutsFastEvent>(c);
        TheEthernet::deinit(c);
    }
    
    static void activate (Context c, NetworkParams const *params)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(!o->net_activated)
        AMBRO_ASSERT(!o->eth_activated)
        
        o->net_activated = true;
        init_netif(c, params);
        TheEthernet::activate(c, o->netif.hwaddr);
    }
    
    static void deactivate (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->net_activated)
        
        o->net_activated = false;
        o->eth_activated = false;
        deinit_netif(c);
        TheEthernet::reset(c);
    }
    
    static bool isActivated (Context c)
    {
        auto *o = Object::self(c);
        return o->net_activated;
    }
    
    static NetworkParams getStatus (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->net_activated)
        
        NetworkParams status;
        memcpy(status.mac_addr, o->netif.hwaddr, sizeof(status.mac_addr));
        status.link_up = TheEthernet::getLinkUp(c);
        status.dhcp_enabled = (o->netif.dhcp != nullptr);
        memcpy(status.ip_addr,    netif_ip4_addr(&o->netif),    sizeof(status.ip_addr));
        memcpy(status.ip_netmask, netif_ip4_netmask(&o->netif), sizeof(status.ip_netmask));
        memcpy(status.ip_gateway, netif_ip4_gw(&o->netif),      sizeof(status.ip_gateway));
        
        return status;
    }
    
    enum class NetworkEventType {ACTIVATION, LINK, DHCP};
    
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
        friend LwipNetwork;
        
    public:
        // WARNING: Don't do any funny calls back to this module directly from the event handler,
        // especially not deinit/reset the NetworkEventListener.
        using EventHandler = Callback<void(Context, NetworkEvent)>;
        
        void init (Context c, EventHandler event_handler)
        {
            m_event_handler = event_handler;
            m_listening = false;
        }
        
        void deinit (Context c)
        {
            reset_internal(c);
        }
        
        void reset (Context c)
        {
            reset_internal(c);
        }
        
        void startListening (Context c)
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(!m_listening)
            
            m_listening = true;
            o->event_listeners.prepend(this);
        }
        
    private:
        void reset_internal (Context c)
        {
            if (m_listening) {
                auto *o = Object::self(c);
                o->event_listeners.remove(this);
                m_listening = false;
            }
        }
        
        EventHandler m_event_handler;
        bool m_listening;
        DoubleEndedListNode<NetworkEventListener> m_node;
    };
    
    class TcpListener {
        friend class TcpConnection;
        
    public:
        // The user is supposed to call TcpConnection::acceptConnection from within
        // AcceptHandler. You must return true if and only if you have accepted the
        // connection and not closed it.
        // WARNING: Do not call any other network functions from this callback,
        // especially don't close the listener. Though the following is permitted:
        // - Closing the resulting connection (deinit/reset) from within this callback
        //   after having accepted it, as long as you indicate this by returning false.
        // - Closing other connections (perhaps in order to make space for the new one).
        using AcceptHandler = Callback<bool(Context)>;
        
        void init (Context c, AcceptHandler accept_handler)
        {
            m_accept_handler = accept_handler;
            m_pcb = nullptr;
            m_accepted_pcb = nullptr;
        }
        
        void deinit (Context c)
        {
            reset_internal(c);
        }
        
        void reset (Context c)
        {
            reset_internal(c);
        }
        
        bool startListening (Context c, uint16_t port)
        {
            AMBRO_ASSERT(!m_pcb)
            
            do {
                m_pcb = tcp_new();
                if (!m_pcb) {
                    goto fail;
                }
                
                ip_addr_t the_addr;
                ip_addr_set_any(0, &the_addr);
                
                auto err = tcp_bind(m_pcb, &the_addr, port);
                if (err != ERR_OK) {
                    goto fail;
                }
                
                struct tcp_pcb *listen_pcb = tcp_listen(m_pcb);
                if (!listen_pcb) {
                    goto fail;
                }
                m_pcb = listen_pcb;
                
                tcp_arg(m_pcb, this);
                tcp_accept(m_pcb, &TcpListener::pcb_accept_handler_wrapper);
                
                return true;
            } while (false);
            
        fail:
            reset_internal(c);
            return false;
        }
        
    private:
        void reset_internal (Context c)
        {
            AMBRO_ASSERT(!m_accepted_pcb)
            
            if (m_pcb) {
                tcp_arg(m_pcb, nullptr);
                tcp_accept(m_pcb, nullptr);
                auto err = tcp_close(m_pcb);
                AMBRO_ASSERT(err == ERR_OK)
                m_pcb = nullptr;
            }
        }
        
        static err_t pcb_accept_handler_wrapper (void *arg, struct tcp_pcb *newpcb, err_t err)
        {
            TcpListener *obj = (TcpListener *)arg;
            return obj->pcb_accept_handler(newpcb, err);
        }
        
        err_t pcb_accept_handler (struct tcp_pcb *newpcb, err_t err)
        {
            Context c;
            AMBRO_ASSERT(m_pcb)
            AMBRO_ASSERT(newpcb)
            AMBRO_ASSERT(err == ERR_OK)
            AMBRO_ASSERT(!m_accepted_pcb)
            
            tcp_accepted(m_pcb);
            
            m_accepted_pcb = newpcb;
            bool accept_res = m_accept_handler(c);
            
            if (m_accepted_pcb) {
                m_accepted_pcb = nullptr;
                return ERR_BUF;
            }
            
            return accept_res ? ERR_OK : ERR_ABRT;
        }
        
        AcceptHandler m_accept_handler;
        struct tcp_pcb *m_pcb;
        struct tcp_pcb *m_accepted_pcb;
    };
    
    class TcpConnection {
        enum class State : uint8_t {IDLE, RUNNING, ERRORING, ERRORED};
        
    public:
        using ErrorHandler = Callback<void(Context, bool remote_closed)>;
        
        // The user is supposed to call TcpConnection::copyReceivedData from within
        // RecvHandler, one or more times, with the sum of 'length' parameters
        // equal to the 'length' in the callback (or less if not all data is needed).
        // WARNING: Do not call any other network functions from this callback.
        // It is specifically prohibited to close (deinit/reset) this connection.
        using RecvHandler = Callback<void(Context, size_t length)>;
        
        using SendHandler = Callback<void(Context)>;
        
        static size_t const RequiredRxBufSize = TCP_WND;
        static size_t const ProvidedTxBufSize = TCP_SND_BUF;
        
        void init (Context c, ErrorHandler error_handler, RecvHandler recv_handler, SendHandler send_handler)
        {
            m_closed_event.init(c, APRINTER_CB_OBJFUNC_T(&TcpConnection::closed_event_handler, this));
            m_sent_event.init(c, APRINTER_CB_OBJFUNC_T(&TcpConnection::sent_event_handler, this));
            m_error_handler = error_handler;
            m_recv_handler = recv_handler;
            m_send_handler = send_handler;
            m_state = State::IDLE;
            m_pcb = nullptr;
            m_received_pbuf = nullptr;
        }
        
        void deinit (Context c)
        {
            reset_internal(c);
            m_sent_event.deinit(c);
            m_closed_event.deinit(c);
        }
        
        void reset (Context c)
        {
            reset_internal(c);
        }
        
        void acceptConnection (Context c, TcpListener *listener)
        {
            AMBRO_ASSERT(m_state == State::IDLE)
            AMBRO_ASSERT(!m_pcb)
            AMBRO_ASSERT(!m_received_pbuf)
            AMBRO_ASSERT(listener->m_accepted_pcb)
            
            m_pcb = listener->m_accepted_pcb;
            listener->m_accepted_pcb = nullptr;
            
            tcp_arg(m_pcb, this);
            tcp_err(m_pcb, &TcpConnection::pcb_err_handler_wrapper);
            tcp_recv(m_pcb, &TcpConnection::pcb_recv_handler_wrapper);
            tcp_sent(m_pcb, &TcpConnection::pcb_sent_handler_wrapper);
            
            m_state = State::RUNNING;
            m_recv_remote_closed = false;
            m_recv_pending = 0;
            m_send_buf_start = 0;
            m_send_buf_length = 0;
            m_send_buf_passed_length = 0;
        }
        
        void raiseError (Context c)
        {
            AMBRO_ASSERT(m_state == State::RUNNING || m_state == State::ERRORING)
            
            if (m_state == State::RUNNING) {
                go_erroring(c, false);
            }
        }
        
        void copyReceivedData (Context c, char *buffer, size_t length)
        {
            AMBRO_ASSERT(m_state == State::RUNNING)
            AMBRO_ASSERT(m_received_pbuf)
            
            while (length > 0) {
                AMBRO_ASSERT(m_received_offset <= m_received_pbuf->len)
                size_t rem_bytes_in_pbuf = m_received_pbuf->len - m_received_offset;
                if (rem_bytes_in_pbuf == 0) {
                    AMBRO_ASSERT(m_received_pbuf->next)
                    m_received_pbuf = m_received_pbuf->next;
                    m_received_offset = 0;
                    continue;
                }
                
                size_t bytes_to_take = MinValue(length, rem_bytes_in_pbuf);
                
                memcpy(buffer, (char *)m_received_pbuf->payload + m_received_offset, bytes_to_take);
                buffer += bytes_to_take;
                length -= bytes_to_take;
                
                m_received_offset += bytes_to_take;
            }
        }
        
        void acceptReceivedData (Context c, size_t amount)
        {
            AMBRO_ASSERT(m_state == State::RUNNING || m_state == State::ERRORING)
            AMBRO_ASSERT(amount <= m_recv_pending)
            
            m_recv_pending -= amount;
            
            if (m_state == State::RUNNING) {
                tcp_recved(m_pcb, amount);
            }
        }
        
        size_t getSendBufferSpace (Context c)
        {
            AMBRO_ASSERT(m_state == State::RUNNING || m_state == State::ERRORING)
            
            return (ProvidedTxBufSize - m_send_buf_length);
        }
        
        void copySendData (Context c, char const *data, size_t amount)
        {
            AMBRO_ASSERT(m_state == State::RUNNING || m_state == State::ERRORING)
            AMBRO_ASSERT(amount <= ProvidedTxBufSize - m_send_buf_length)
            
            size_t write_offset = send_buf_add(m_send_buf_start, m_send_buf_length);
            WrapBuffer::Make(ProvidedTxBufSize - write_offset, m_send_buf + write_offset, m_send_buf).copyIn(0, amount, data);
            m_send_buf_length += amount;
        }
        
        void pokeSending (Context c)
        {
            AMBRO_ASSERT(m_state == State::RUNNING || m_state == State::ERRORING)
            
            if (m_state == State::RUNNING) {
                do_send(c);
            }
        }
        
    private:
        void reset_internal (Context c)
        {
            AMBRO_ASSERT(!m_received_pbuf)
            
            if (m_pcb) {
                remove_pcb_callbacks(m_pcb);
                close_pcb(m_pcb);
                m_pcb = nullptr;
            }
            m_sent_event.unset(c);
            m_closed_event.unset(c);
            m_state = State::IDLE;
        }
        
        void go_erroring (Context c, bool pcb_gone)
        {
            AMBRO_ASSERT(m_state == State::RUNNING)
            
            remove_pcb_callbacks(m_pcb);
            if (pcb_gone) {
                m_pcb = nullptr;
            }
            m_state = State::ERRORING;
            m_sent_event.unset(c);
            
            if (!m_closed_event.isSet(c)) {
                m_closed_event.prependNowNotAlready(c);
            }
        }
        
        void do_send (Context c)
        {
            AMBRO_ASSERT(m_state == State::RUNNING)
            
            while (m_send_buf_passed_length < m_send_buf_length) {
                size_t pass_offset = send_buf_add(m_send_buf_start, m_send_buf_passed_length);
                size_t pass_avail = m_send_buf_length - m_send_buf_passed_length;
                size_t pass_length = MinValue(pass_avail, (size_t)(ProvidedTxBufSize - pass_offset));
                
                u16_t written;
                auto err = tcp_write_ext(m_pcb, m_send_buf + pass_offset, pass_length, TCP_WRITE_FLAG_PARTIAL, &written);
                if (err != ERR_OK) {
                    go_erroring(c, false);
                    return;
                }
                
                AMBRO_ASSERT(written <= pass_length)
                m_send_buf_passed_length += written;
                
                if (written < pass_length) {
                    break;
                }
            }
            
            auto err = tcp_output(m_pcb);
            if (err != ERR_OK) {
                go_erroring(c, false);
            }
        }
        
        static void pcb_err_handler_wrapper (void *arg, err_t err)
        {
            TcpConnection *obj = (TcpConnection *)arg;
            return obj->pcb_err_handler(err);
        }
        
        void pcb_err_handler (err_t err)
        {
            Context c;
            AMBRO_ASSERT(m_state == State::RUNNING)
            
            go_erroring(c, true);
        }
        
        static err_t pcb_recv_handler_wrapper (void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
        {
            TcpConnection *obj = (TcpConnection *)arg;
            return obj->pcb_recv_handler(tpcb, p, err);
        }
        
        err_t pcb_recv_handler (struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
        {
            Context c;
            AMBRO_ASSERT(m_state == State::RUNNING)
            AMBRO_ASSERT(!m_received_pbuf)
            AMBRO_ASSERT(tpcb == m_pcb)
            AMBRO_ASSERT(err == ERR_OK)
            AMBRO_ASSERT(!p || p->tot_len <= RequiredRxBufSize - m_recv_pending)
            
            if (!p) {
                if (!m_recv_remote_closed) {
                    m_recv_remote_closed = true;
                    m_closed_event.prependNowNotAlready(c);
                }
            } else {
                if (!m_recv_remote_closed && p->tot_len > 0) {
                    m_recv_pending += p->tot_len;
                    m_received_pbuf = p;
                    m_received_offset = 0;
                    
                    m_recv_handler(c, p->tot_len);
                    
                    m_received_pbuf = nullptr;
                }
                
                pbuf_free(p);
            }
            
            return ERR_OK;
        }
        
        static err_t pcb_sent_handler_wrapper (void *arg, struct tcp_pcb *tpcb, uint16_t len)
        {
            TcpConnection *obj = (TcpConnection *)arg;
            return obj->pcb_sent_handler(tpcb, len);
        }
        
        err_t pcb_sent_handler (struct tcp_pcb *tpcb, uint16_t len)
        {
            Context c;
            AMBRO_ASSERT(m_state == State::RUNNING)
            AMBRO_ASSERT(len <= m_send_buf_passed_length)
            AMBRO_ASSERT(m_send_buf_passed_length <= m_send_buf_length)
            
            m_send_buf_start = send_buf_add(m_send_buf_start, len);
            m_send_buf_length -= len;
            m_send_buf_passed_length -= len;
            
            if (!m_sent_event.isSet(c)) {
                m_sent_event.prependNowNotAlready(c);
            }
            
            return ERR_OK;
        }
        
        void closed_event_handler (Context c)
        {
            switch (m_state) {
                case State::RUNNING: {
                    AMBRO_ASSERT(m_recv_remote_closed)
                    
                    return m_error_handler(c, true);
                } break;
                
                case State::ERRORING: {
                    if (m_pcb) {
                        close_pcb(m_pcb);
                        m_pcb = nullptr;
                    }
                    m_state = State::ERRORED;
                    
                    return m_error_handler(c, false);
                } break;
                
                default: AMBRO_ASSERT(false);
            }
        }
        
        void sent_event_handler (Context c)
        {
            AMBRO_ASSERT(m_state == State::RUNNING)
            
            do_send(c);
            
            return m_send_handler(c);
        }
        
        static void remove_pcb_callbacks (struct tcp_pcb *pcb)
        {
            tcp_arg(pcb, nullptr);
            tcp_err(pcb, nullptr);
            tcp_recv(pcb, nullptr);
            tcp_sent(pcb, nullptr);
        }
        
        static void close_pcb (struct tcp_pcb *pcb)
        {
            auto err = tcp_close(pcb);
            if (err != ERR_OK) {
                tcp_abort(pcb);
            }
        }
        
        static size_t send_buf_add (size_t start, size_t count)
        {
            size_t x = start + count;
            if (x >= ProvidedTxBufSize) {
                x -= ProvidedTxBufSize;
            }
            return x;
        }
        
        typename Context::EventLoop::QueuedEvent m_closed_event;
        typename Context::EventLoop::QueuedEvent m_sent_event;
        ErrorHandler m_error_handler;
        RecvHandler m_recv_handler;
        SendHandler m_send_handler;
        State m_state;
        bool m_recv_remote_closed;
        struct tcp_pcb *m_pcb;
        struct pbuf *m_received_pbuf;
        size_t m_received_offset;
        size_t m_recv_pending;
        size_t m_send_buf_start;
        size_t m_send_buf_length;
        size_t m_send_buf_passed_length;
        char m_send_buf[ProvidedTxBufSize];
    };
    
private:
    static ip4_addr_t make_ip4_addr (uint8_t const *addr)
    {
        uint32_t hostorder_addr = ReadBinaryInt<uint32_t, BinaryBigEndian>((char const *)addr);
        return ip4_addr_t{PP_HTONL(hostorder_addr)};
    }
    
    static void init_netif (Context c, NetworkParams const *params)
    {
        auto *o = Object::self(c);
        
        ip_addr_t the_ipaddr;
        ip_addr_t the_netmask;
        ip_addr_t the_gw;
        
        if (params->dhcp_enabled) {
            ip_addr_set_zero_ip4(&the_ipaddr);
            ip_addr_set_zero_ip4(&the_netmask);
            ip_addr_set_zero_ip4(&the_gw);
        } else {
            ip_addr_copy_from_ip4(the_ipaddr,  make_ip4_addr(params->ip_addr));
            ip_addr_copy_from_ip4(the_netmask, make_ip4_addr(params->ip_netmask));
            ip_addr_copy_from_ip4(the_gw,      make_ip4_addr(params->ip_gateway));
        }
        
        memset(&o->netif, 0, sizeof(o->netif));
        
        netif_add(&o->netif, &the_ipaddr, &the_netmask, &the_gw, (void *)params, &LwipNetwork::netif_if_init, ethernet_input);
        netif_set_up(&o->netif);
        netif_set_default(&o->netif);
        
        if (params->dhcp_enabled) {
            dhcp_start(&o->netif);
        }
        
        // Set the status callback last so we don't get unwanted callbacks.
        netif_set_status_callback(&o->netif, &LwipNetwork::netif_status_callback);
    }
    
    static void deinit_netif (Context c)
    {
        auto *o = Object::self(c);
        
        // Remove the status callback first.
        netif_set_status_callback(&o->netif, nullptr);
        
        if (o->netif.dhcp != nullptr) {
            dhcp_stop(&o->netif);
        }
        
        netif_remove(&o->netif);
    }
    
    static err_t netif_if_init (struct netif *netif)
    {
        NetworkParams const *params = (NetworkParams const *)netif->state;
        
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
    
    static void netif_status_callback (struct netif *netif)
    {
        Context c;
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->net_activated)
        
        if (o->netif.dhcp != nullptr) {
            NetworkEvent event{NetworkEventType::DHCP};
            event.dhcp.up = !ip_addr_isany(&o->netif.ip_addr);
            raise_network_event(c, event);
        }
    }
    
    static void timeouts_event_handler (Context c)
    {
        auto *o = Object::self(c);
        
        Context::EventLoop::template triggerFastEvent<TimeoutsFastEvent>(c);
        sys_check_timeouts();
    }
    
    static void ethernet_activate_handler (Context c, bool error)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->net_activated)
        AMBRO_ASSERT(!o->eth_activated)
        
        if (!error) {
            o->eth_activated = true;
        }
        
        NetworkEvent event{NetworkEventType::ACTIVATION};
        event.activation.error = error;
        raise_network_event(c, event);
    }
    struct EthernetActivateHandler : public AMBRO_WFUNC_TD(&LwipNetwork::ethernet_activate_handler) {};
    
    static void ethernet_link_handler (Context c, bool link_status)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->eth_activated)
        
        if (link_status) {
            netif_set_link_up(&o->netif);
        } else {
            netif_set_link_down(&o->netif);
        }
        
        NetworkEvent event{NetworkEventType::LINK};
        event.link.up = link_status;
        raise_network_event(c, event);
    }
    struct EthernetLinkHandler : public AMBRO_WFUNC_TD(&LwipNetwork::ethernet_link_handler) {};
    
    static err_t netif_link_output (struct netif *netif, struct pbuf *p)
    {
        Context c;
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->net_activated)
        
#if ETH_PAD_SIZE
        pbuf_header(p, -ETH_PAD_SIZE);
#endif
        err_t ret = ERR_BUF;
        
        debug_print_pbuf(c, "Tx", p, 0);
        
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
            if (!o->rx_pbuf) {
                o->rx_pbuf = pbuf_alloc(PBUF_RAW, PBUF_POOL_BUFSIZE, PBUF_POOL);
                if (!o->rx_pbuf) {
                    return;
                }
            }
            
            struct pbuf *p = o->rx_pbuf;
            
            size_t length;
            if (!TheEthernet::recvFrame(c, (char *)p->payload + ETH_PAD_SIZE, RxPbufPayloadSize, &length)) {
                return;
            }
            AMBRO_ASSERT(length <= RxPbufPayloadSize)
            
            if (length == 0) {
                LINK_STATS_INC(link.memerr);
                LINK_STATS_INC(link.drop);
                continue;
            }
            
            p->tot_len = ETH_PAD_SIZE + length;
            p->len = ETH_PAD_SIZE + length;
            
            debug_print_pbuf(c, "Rx", p, ETH_PAD_SIZE);
            
            LINK_STATS_INC(link.recv);
            
            if (o->netif.input(p, &o->netif) != ERR_OK) {
                pbuf_free(p);
            }
            
            o->rx_pbuf = nullptr;
        }
    }
    struct EthernetReceiveHandler : public AMBRO_WFUNC_TD(&LwipNetwork::ethernet_receive_handler) {};
    
    static void debug_print_pbuf (Context c, char const *event, struct pbuf *p, int16_t skip_header)
    {
#ifdef APRINTER_DEBUG_NETWORK
        pbuf_header(p, -skip_header);
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
        pbuf_header(p, skip_header);
#endif
    }
    
    static void raise_network_event (Context c, NetworkEvent event)
    {
        auto *o = Object::self(c);
        
        for (NetworkEventListener *nel = o->event_listeners.first(); nel != nullptr; nel = o->event_listeners.next(nel)) {
            AMBRO_ASSERT(nel->m_listening)
            nel->m_event_handler(c, event);
            AMBRO_ASSERT(nel->m_listening)
        }
    }
    
public:
    using EventLoopFastEvents = MakeTypeList<TimeoutsFastEvent>;
    
    using GetEthernet = TheEthernet;
    
    struct Object : public ObjBase<LwipNetwork, ParentObject, MakeTypeList<
        TheEthernet
    >> {
        DoubleEndedList<NetworkEventListener, &NetworkEventListener::m_node, false> event_listeners;
        bool net_activated;
        bool eth_activated;
        struct pbuf *rx_pbuf;
        struct netif netif;
    };
};

#include <aprinter/EndNamespace.h>

#endif
