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
#include <aprinter/meta/MinMax.h>
#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/Callback.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/WrapBuffer.h>
#include <aprinter/structure/DoubleEndedList.h>
#include <aprinter/misc/ClockUtils.h>
#include <aprinter/hal/common/EthernetCommon.h>
#include <aprinter/ipstack/misc/Struct.h>
#include <aprinter/ipstack/misc/Buf.h>
#include <aprinter/ipstack/proto/IpAddr.h>
#include <aprinter/ipstack/ip/IpStack.h>
#include <aprinter/ipstack/tcp/IpTcpProto.h>
#include <aprinter/ipstack/eth/EthIpIface.h>

#include <aprinter/BeginNamespace.h>

template <typename Arg>
class IpStackNetwork {
    using Context         = typename Arg::Context;
    using ParentObject    = typename Arg::ParentObject;
    
    using EthernetService            = typename Arg::Params::EthernetService;
    static int const NumArpEntries   = Arg::Params::NumArpEntries;
    static int const ArpProtectCount = Arg::Params::ArpProtectCount;
    static int const NumTcpPcbs      = Arg::Params::NumTcpPcbs;
    static int const NumOosSegs      = Arg::Params::NumOosSegs;
    static int const TcpWndUpdThrDiv = Arg::Params::TcpWndUpdThrDiv;
    
    static_assert(NumArpEntries >= 4, "");
    static_assert(ArpProtectCount >= 2, "");
    static_assert(NumTcpPcbs >= 2, "");
    static_assert(NumOosSegs >= 2 && NumOosSegs <= 255, "");
    static_assert(TcpWndUpdThrDiv >= 2, "");
    
    static size_t const EthMTU = 1514;
    static size_t const TcpMaxMSS = EthMTU - EthHeader::Size - Ip4Header::Size - Tcp4Header::Size;
    static_assert(TcpMaxMSS == 1460, "");
    
public:
    struct Object;
    class TcpListener;
    class TcpConnection;
    
private:
    static uint8_t const IpTTL = 64;
    
    using TheClockUtils = ClockUtils<Context>;
    using TimeType = typename Context::Clock::TimeType;
    
    using TheBufAllocator = StackBufAllocator;
    
    using TheIpStackService = IpStackService<
        EthHeader::Size, // HeaderBeforeIp
        IpTTL            // IcmpTTL
    >;
    APRINTER_MAKE_INSTANCE(TheIpStack, (TheIpStackService::template Compose<Context, TheBufAllocator>))
    
    using TheIpTcpProtoService = IpTcpProtoService<
        IpTTL,
        NumTcpPcbs,
        NumOosSegs
    >;
    APRINTER_MAKE_INSTANCE(TheIpTcpProto, (TheIpTcpProtoService::template Compose<Context, TheBufAllocator, TheIpStack>))
    
    using IpTcpListener           = typename TheIpTcpProto::TcpListener;
    using IpTcpListenerCallback   = typename TheIpTcpProto::TcpListenerCallback;
    using IpTcpConnection         = typename TheIpTcpProto::TcpConnection;
    using IpTcpConnectionCallback = typename TheIpTcpProto::TcpConnectionCallback;
    using SeqType                 = typename TheIpTcpProto::SeqType;
    
    struct EthernetActivateHandler;
    struct EthernetLinkHandler;
    struct EthernetReceiveHandler;
    using TheEthernetClientParams = EthernetClientParams<EthernetActivateHandler, EthernetLinkHandler, EthernetReceiveHandler, IpBufRef>;
    APRINTER_MAKE_INSTANCE(TheEthernet, (EthernetService::template Ethernet<Context, Object, TheEthernetClientParams>))
    
    using TheEthIpIfaceService = EthIpIfaceService<
        NumArpEntries,
        ArpProtectCount,
        0 // HeaderBeforeEth
    >;
    APRINTER_MAKE_INSTANCE(TheEthIpIface, (TheEthIpIfaceService::template Compose<Context, TheBufAllocator>))
    
public:
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
    
    static void enableDebugMessages (Context c, bool enabled)
    {
    }
    
    static void activate (Context c, NetworkParams const *params)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->activation_state == NOT_ACTIVATED)
        
        MacAddr mac_addr = ReadSingleField<MacAddr>((char const *)params->mac_addr);
        
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
            
            IpIfaceIp4AddrSetting addr_setting = o->ip_iface.getIp4Addr();
            if (addr_setting.present) {
                WriteSingleField<Ip4Addr>((char *)status.ip_addr, addr_setting.addr);
                WriteSingleField<Ip4Addr>((char *)status.ip_netmask, Ip4Addr::PrefixMask(addr_setting.prefix));
            }
            
            IpIfaceIp4GatewaySetting gw_setting = o->ip_iface.getIp4Gateway();
            if (gw_setting.present) {
                WriteSingleField<Ip4Addr>((char *)status.ip_gateway, gw_setting.addr);
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
    
    class TcpListenerQueueEntry : private IpTcpConnectionCallback {
        friend class TcpListener;
        
    private:
        void connectionAborted () override
        {
            AMBRO_ASSERT(!m_ip_connection.isInit())
            
            m_ip_connection.reset();
            m_listener->update_timeout(Context());
        }
        
        void dataReceived (size_t) override
        {
            AMBRO_ASSERT(false) // zero window
        }
        
        void dataSent (size_t) override
        {
            AMBRO_ASSERT(false) // nothing sent
        }
        
    private:
        TcpListener *m_listener;
        IpTcpConnection m_ip_connection;
        TimeType m_time;
    };
    
    struct TcpListenParams {
        uint16_t port;
        int max_pcbs;
        size_t min_rcv_buf_size;
        int queue_size;
        TimeType queue_timeout;
        TcpListenerQueueEntry *queue_entries;
    };
    
    class TcpListener : private IpTcpListenerCallback {
        friend class TcpConnection;
        
    public:
        using AcceptHandler = Callback<void(Context)>;
        
        void init (Context c, AcceptHandler accept_handler)
        {
            auto *o = Object::self(c);
            
            m_dequeue_event.init(c, APRINTER_CB_OBJFUNC_T(&TcpListener::dequeue_event_handler, this));
            m_timeout_event.init(c, APRINTER_CB_OBJFUNC_T(&TcpListener::timeout_event_handler, this));
            m_ip_listener.init(this);
            m_accept_handler = accept_handler;
        }
        
        void deinit (Context c)
        {
            deinit_queue();
            m_ip_listener.deinit();
            m_timeout_event.deinit(c);
            m_dequeue_event.deinit(c);
        }
        
        void reset (Context c)
        {
            deinit_queue();
            m_dequeue_event.unset(c);
            m_timeout_event.unset(c);
            m_ip_listener.reset();
        }
        
        bool startListening (Context c, TcpListenParams const &params)
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(!m_ip_listener.isListening())
            AMBRO_ASSERT(params.max_pcbs > 0)
            AMBRO_ASSERT(params.queue_size >= 0)
            AMBRO_ASSERT(params.queue_size == 0 || params.queue_entries != nullptr)
            
            // Start listening.
            if (!m_ip_listener.listenIp4(&o->ip_tcp_proto, Ip4Addr::ZeroAddr(), params.port, params.max_pcbs)) {
                return false;
            }
            
            // Init queue variables.
            m_queue = params.queue_entries;
            m_queue_size = params.queue_size;
            m_queue_timeout = params.queue_timeout;
            m_queued_to_accept = nullptr;
            
            // Init queue entries.
            for (int i = 0; i < m_queue_size; i++) {
                TcpListenerQueueEntry *entry = &m_queue[i];
                entry->m_listener = this;
                entry->m_ip_connection.init(entry);
            }
            
            // If there is no queue, raise the initial receive window.
            // If there is a queue, we have to leave it at zero.
            if (m_queue_size == 0) {
                m_ip_listener.setInitialReceiveWindow(params.min_rcv_buf_size);
            }
            
            return true;
        }
        
        void scheduleDequeue (Context c)
        {
            AMBRO_ASSERT(m_ip_listener.isListening())
            
            if (m_queue_size > 0) {
                m_dequeue_event.prependNow(c);
            }
        }
        
        void acceptIpConnection (Context c, IpTcpConnection *dst_con)
        {
            AMBRO_ASSERT(m_ip_listener.isListening())
            
            if (m_queued_to_accept != nullptr) {
                TcpListenerQueueEntry *entry = m_queued_to_accept;
                dst_con->moveConnection(&entry->m_ip_connection);
            } else {
                dst_con->acceptConnection(&m_ip_listener);
            }
        }
        
    private:
        void connectionEstablished (IpTcpListener *lis) override
        {
            Context c;
            AMBRO_ASSERT(lis == &m_ip_listener)
            AMBRO_ASSERT(m_ip_listener.isListening())
            AMBRO_ASSERT(m_ip_listener.hasAcceptPending())
            AMBRO_ASSERT(m_queued_to_accept == nullptr)
            
            // Call the accept callback so the user can call acceptConnection.
            m_accept_handler(c);
            
            // If the user did not accept the connection, try to queue it.
            if (m_ip_listener.hasAcceptPending()) {
                for (int i = 0; i < m_queue_size; i++) {
                    TcpListenerQueueEntry *entry = &m_queue[i];
                    if (entry->m_ip_connection.isInit()) {
                        entry->m_ip_connection.acceptConnection(&m_ip_listener);
                        entry->m_time = Context::Clock::getTime(c);
                        update_timeout(c);
                        break;
                    }
                }
            }
        }
        
        void dequeue_event_handler (Context c)
        {
            AMBRO_ASSERT(m_ip_listener.isListening())
            AMBRO_ASSERT(m_queue_size > 0)
            AMBRO_ASSERT(m_queued_to_accept == nullptr)
            
            bool queue_changed = false;
            
            // Find the oldest queued connection.
            TcpListenerQueueEntry *oldest_entry = find_oldest_queued_pcb();
            
            while (oldest_entry != nullptr) {
                AMBRO_ASSERT(!oldest_entry->m_ip_connection.isInit())
                
                // Call the accept handler, while publishing the connection.
                m_queued_to_accept = oldest_entry;
                m_accept_handler(c);
                m_queued_to_accept = nullptr;
                
                // If the connection was not taken, stop trying.
                if (!oldest_entry->m_ip_connection.isInit()) {
                    break;
                }
                
                queue_changed = true;
                
                // Refresh the oldest entry.
                oldest_entry = find_oldest_queued_pcb();
            }
            
            // Update the dequeue timeout if we dequeued any connection.
            if (queue_changed) {
                update_timeout(c, oldest_entry);
            }
        }
        
        void update_timeout (Context c)
        {
            AMBRO_ASSERT(m_ip_listener.isListening())
            AMBRO_ASSERT(m_queue_size > 0)
            
            update_timeout(c, find_oldest_queued_pcb());
        }
        
        void update_timeout (Context c, TcpListenerQueueEntry *oldest_entry)
        {
            AMBRO_ASSERT(m_ip_listener.isListening())
            AMBRO_ASSERT(m_queue_size > 0)
            
            if (oldest_entry != nullptr) {
                TimeType expire_time = oldest_entry->m_time + m_queue_timeout;
                m_timeout_event.appendAt(c, expire_time);
            } else {
                m_timeout_event.unset(c);
            }
        }
        
        void timeout_event_handler (Context c)
        {
            AMBRO_ASSERT(m_ip_listener.isListening())
            AMBRO_ASSERT(m_queue_size > 0)
            
            // The oldest queued connection has expired, close it.
            TcpListenerQueueEntry *entry = find_oldest_queued_pcb();
            AMBRO_ASSERT(entry != nullptr)
            entry->m_ip_connection.reset();
            update_timeout(c);
        }
        
        void deinit_queue ()
        {
            if (m_ip_listener.isListening()) {
                for (int i = 0; i < m_queue_size; i++) {
                    TcpListenerQueueEntry *entry = &m_queue[i];
                    entry->m_ip_connection.deinit();
                }
            }
        }
        
        TcpListenerQueueEntry * find_oldest_queued_pcb ()
        {
            TcpListenerQueueEntry *oldest_entry = nullptr;
            for (int i = 0; i < m_queue_size; i++) {
                TcpListenerQueueEntry *entry = &m_queue[i];
                if (!entry->m_ip_connection.isInit() &&
                    (oldest_entry == nullptr || !TheClockUtils::timeGreaterOrEqual(entry->m_time, oldest_entry->m_time)))
                {
                    oldest_entry = entry;
                }
            }
            return oldest_entry;
        }
        
        typename Context::EventLoop::QueuedEvent m_dequeue_event;
        typename Context::EventLoop::TimedEvent m_timeout_event;
        AcceptHandler m_accept_handler;
        IpTcpListener m_ip_listener;
        TcpListenerQueueEntry *m_queue;
        int m_queue_size;
        TimeType m_queue_timeout;
        TcpListenerQueueEntry *m_queued_to_accept;
    };
    
    class TcpConnectionCallback {
    public:
        virtual void connectionAborted (Context c) = 0;
        virtual void dataReceived (Context c, size_t amount) = 0;
        virtual void dataSent (Context c, size_t amount) = 0;
    };
    
    struct TcpConnectionBufferInfo {
        char *send_buf;
        size_t send_buf_size;
        char *recv_buf;
        size_t recv_buf_size;
        size_t recv_buf_mirror;
    };
    
    class TcpConnection : private IpTcpConnectionCallback {
    public:
        static size_t const MinSendBufSize = 2*TcpMaxMSS;
        static size_t const MinRecvBufSize = 2*TcpMaxMSS;
        
        // How much less free TX buffer than its size we guarantee to provide to
        // the application eventually. See IpTcpProto::TcpConnection::getSndBufOverhead
        // for an explanation.
        static size_t const MaxSndBufOverhead = TcpMaxMSS - 1;
        
        void init (Context c, TcpConnectionCallback *callback)
        {
            AMBRO_ASSERT(callback != nullptr)
            
            m_ip_connection.init(this);
            m_callback = callback;
        }
        
        void deinit (Context c)
        {
            m_ip_connection.deinit();
        }
        
        void reset (Context c)
        {
            m_ip_connection.reset();
        }
        
        void acceptConnection (Context c, TcpListener *listener, TcpConnectionBufferInfo const &bufinfo)
        {
            AMBRO_ASSERT(m_ip_connection.isInit())
            AMBRO_ASSERT(listener->m_ip_listener.isListening())
            AMBRO_ASSERT(bufinfo.send_buf_size >= MinSendBufSize)
            AMBRO_ASSERT(bufinfo.recv_buf_size >= MinRecvBufSize)
            AMBRO_ASSERT(bufinfo.recv_buf_mirror < bufinfo.recv_buf_size)
            
            m_send_buf_node = IpBufNode{bufinfo.send_buf, bufinfo.send_buf_size, &m_send_buf_node};
            m_recv_buf_node = IpBufNode{bufinfo.recv_buf, bufinfo.recv_buf_size, &m_recv_buf_node};
            m_recv_buf_mirror = bufinfo.recv_buf_mirror;
            
            listener->acceptIpConnection(c, &m_ip_connection);
            setup_connection();
        }
        
        size_t getRecvBufferUsed (Context c)
        {
            AMBRO_ASSERT(!m_ip_connection.isInit())
            
            return m_recv_buf_node.len - get_recv_buf().tot_len;
        }
        
        WrapBuffer getRecvBufferPtr (Context c)
        {
            AMBRO_ASSERT(!m_ip_connection.isInit())
            
            IpBufRef rcv_buf = get_recv_buf();
            size_t write_offset = (rcv_buf.offset == m_recv_buf_node.len) ? 0 : rcv_buf.offset;
            size_t read_offset = add_modulo(write_offset, rcv_buf.tot_len, m_recv_buf_node.len);
            return WrapBuffer(m_recv_buf_node.len - read_offset, m_recv_buf_node.ptr + read_offset, m_recv_buf_node.ptr);
        }
        
        void consumeRecvData (Context c, size_t amount)
        {
            AMBRO_ASSERT(!m_ip_connection.isInit())
            AMBRO_ASSERT(amount <= getRecvBufferUsed(c))
            
            m_ip_connection.extendRecvBuf(amount);
        }
        
        void copyRecvData (Context c, MemRef data)
        {
            AMBRO_ASSERT(!m_ip_connection.isInit())
            AMBRO_ASSERT(data.len <= getRecvBufferUsed(c))
            
            getRecvBufferPtr(c).copyOut(data);
            m_ip_connection.extendRecvBuf(data.len);
        }
        
        bool wasEndReceived (Context c)
        {
            AMBRO_ASSERT(!m_ip_connection.isInit())
            
            return m_ip_connection.wasEndReceived();
        }
        
        size_t getSendBufferSpace (Context c)
        {
            AMBRO_ASSERT(!m_ip_connection.isInit())
            
            return m_send_buf_node.len - get_send_buf().tot_len;
        }
        
        WrapBuffer getSendBufferPtr (Context c)
        {
            AMBRO_ASSERT(!m_ip_connection.isInit())
            AMBRO_ASSERT(!m_ip_connection.wasSendingClosed())
            
            IpBufRef snd_buf = get_send_buf();
            size_t read_offset = (snd_buf.offset == m_send_buf_node.len) ? 0 : snd_buf.offset;
            size_t write_offset = add_modulo(read_offset, snd_buf.tot_len, m_send_buf_node.len);
            return WrapBuffer(m_send_buf_node.len - write_offset, m_send_buf_node.ptr + write_offset, m_send_buf_node.ptr);
        }
        
        void provideSendData (Context c, size_t amount)
        {
            AMBRO_ASSERT(!m_ip_connection.isInit())
            AMBRO_ASSERT(!m_ip_connection.wasSendingClosed())
            AMBRO_ASSERT(amount <= getSendBufferSpace(c))
            
            m_ip_connection.extendSendBuf(amount);
        }
        
        void copySendData (Context c, MemRef data)
        {
            AMBRO_ASSERT(!m_ip_connection.isInit())
            AMBRO_ASSERT(!m_ip_connection.wasSendingClosed())
            AMBRO_ASSERT(data.len <= getSendBufferSpace(c))
            
            getSendBufferPtr(c).copyIn(data);
            m_ip_connection.extendSendBuf(data.len);
        }
        
        void pokeSending (Context c)
        {
            AMBRO_ASSERT(!m_ip_connection.isInit())
            AMBRO_ASSERT(!m_ip_connection.wasSendingClosed())
            
            m_ip_connection.sendPush();
        }
        
        void closeSending (Context c)
        {
            AMBRO_ASSERT(!m_ip_connection.isInit())
            AMBRO_ASSERT(!m_ip_connection.wasSendingClosed())
            
            m_ip_connection.closeSending();
        }
        
        bool wasSendingClosed (Context c)
        {
            AMBRO_ASSERT(!m_ip_connection.isInit())
            
            return m_ip_connection.wasSendingClosed();
        }
        
        bool wasEndSent (Context c)
        {
            AMBRO_ASSERT(!m_ip_connection.isInit())
            
            return m_ip_connection.wasEndSent();
        }
        
    private:
        void setup_connection ()
        {
            // Configure the send buffer (this is done just once since it's circular).
            m_ip_connection.setSendBuf(IpBufRef{&m_send_buf_node, (size_t)0, (size_t)0});
            
            // Set the window update threshold before setting the receive buffer.
            SeqType max_rx_window = MinValueU(m_recv_buf_node.len, TheIpTcpProto::MaxRcvWnd);
            SeqType thres = MaxValue((SeqType)1, max_rx_window / TcpWndUpdThrDiv);
            m_ip_connection.setWindowUpdateThreshold(thres);
            
            // Configure the receive buffer.
            m_ip_connection.setRecvBuf(IpBufRef{&m_recv_buf_node, (size_t)0, m_recv_buf_node.len});
        }
        
        void connectionAborted () override
        {
            m_callback->connectionAborted(Context());
        }
        
        void dataReceived (size_t amount) override
        {
            if (amount > 0 && m_recv_buf_mirror > 0) {
                // Calculate the offset in the buffer to which new data was written.
                IpBufRef rcv_buf = m_ip_connection.getRecvBuf();
                AMBRO_ASSERT(rcv_buf.tot_len + amount <= m_recv_buf_node.len)
                AMBRO_ASSERT(rcv_buf.offset <= m_recv_buf_node.len)
                size_t write_offset = (rcv_buf.offset == m_recv_buf_node.len) ? 0 : rcv_buf.offset;
                size_t data_offset = add_modulo(write_offset, m_recv_buf_node.len - amount, m_recv_buf_node.len);
                
                // Copy data to the mirror region as needed.
                if (data_offset < m_recv_buf_mirror) {
                    ::memcpy(m_recv_buf_node.ptr + m_recv_buf_node.len + data_offset, m_recv_buf_node.ptr + data_offset, MinValue(amount, m_recv_buf_mirror - data_offset));
                }
                if (amount > m_recv_buf_node.len - data_offset) {
                    ::memcpy(m_recv_buf_node.ptr + m_recv_buf_node.len, m_recv_buf_node.ptr, MinValue(amount - (m_recv_buf_node.len - data_offset), m_recv_buf_mirror));
                }
            }
            
            m_callback->dataReceived(Context(), amount);
        }
        
        void dataSent (size_t amount) override
        {
            m_callback->dataSent(Context(), amount);
        }
        
        inline IpBufRef get_send_buf ()
        {
            IpBufRef snd_buf = m_ip_connection.getSendBuf();
            AMBRO_ASSERT(snd_buf.tot_len <= m_send_buf_node.len)
            AMBRO_ASSERT(snd_buf.offset <= m_send_buf_node.len)
            return snd_buf;
        }
        
        inline IpBufRef get_recv_buf ()
        {
            IpBufRef rcv_buf = m_ip_connection.getRecvBuf();
            AMBRO_ASSERT(rcv_buf.tot_len <= m_recv_buf_node.len)
            AMBRO_ASSERT(rcv_buf.offset <= m_recv_buf_node.len)
            return rcv_buf;
        }
        
        static size_t add_modulo (size_t start, size_t count, size_t modulo)
        {
            size_t x = start + count;
            if (x >= modulo) {
                x -= modulo;
            }
            return x;
        }
        
        IpTcpConnection m_ip_connection;
        TcpConnectionCallback *m_callback;
        IpBufNode m_send_buf_node;
        IpBufNode m_recv_buf_node;
        size_t m_recv_buf_mirror;
    };
    
private:
    class EthDriverProxy : public EthIfaceDriver {
        friend IpStackNetwork;
        
        void clear ()
        {
            m_callback = nullptr;
        }
        
    public:
        void setCallback (EthIfaceDriverCallback *callback) override
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
            
            if (!TheEthernet::sendFrame(Context(), &frame)) {
                return IpErr::BUFFER_FULL;
            }
            
            return IpErr::SUCCESS;
        }
        
    private:
        EthIfaceDriverCallback *m_callback;
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
                Ip4Addr addr    = ReadSingleField<Ip4Addr>((char const *)o->config.ip_addr);
                Ip4Addr netmask = ReadSingleField<Ip4Addr>((char const *)o->config.ip_netmask);
                Ip4Addr gateway = ReadSingleField<Ip4Addr>((char const *)o->config.ip_gateway);
                
                if (addr != Ip4Addr::ZeroAddr()) {
                    o->ip_iface.setIp4Addr(IpIfaceIp4AddrSetting{true, (uint8_t)netmask.countLeadingOnes(), addr});
                }
                if (gateway != Ip4Addr::ZeroAddr()) {
                    o->ip_iface.setIp4Gateway(IpIfaceIp4GatewaySetting{true, gateway});
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
        typename TheIpStack::Iface ip_iface;
        NetworkParams config;
    };
};

APRINTER_ALIAS_STRUCT_EXT(IpStackNetworkService, (
    APRINTER_AS_TYPE(EthernetService),
    APRINTER_AS_VALUE(int, NumArpEntries),
    APRINTER_AS_VALUE(int, ArpProtectCount),
    APRINTER_AS_VALUE(int, NumTcpPcbs),
    APRINTER_AS_VALUE(int, NumOosSegs),
    APRINTER_AS_VALUE(int, TcpWndUpdThrDiv)
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
