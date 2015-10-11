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

#include <stdint.h>
#include <stddef.h>

#include <lwip/init.h>

#include <aprinter/meta/WrapFunction.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/Callback.h>
#include <aprinter/hal/common/EthernetCommon.h>
#include <aprinter/printer/Console.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, typename EthernetService>
class LwipNetwork {
public:
    struct Object;
    
private:
    struct EthernetActivateHandler;
    struct EthernetLinkHandler;
    struct EthernetSendBuffer;
    struct EthernetRecvBuffer;
    using TheEthernetClientParams = EthernetClientParams<EthernetActivateHandler, EthernetLinkHandler, EthernetSendBuffer, EthernetRecvBuffer>;
    using TheEthernet = typename EthernetService::template Ethernet<Context, Object, TheEthernetClientParams>;
    
public:
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        //lwip_init();
        
        TheEthernet::init(c);
        
        o->mac_addr[0] = 0x6F;
        o->mac_addr[1] = 0xBB;
        o->mac_addr[2] = 0x93;
        o->mac_addr[3] = 0x43;
        o->mac_addr[4] = 0xC6;
        o->mac_addr[5] = 0x2D;
        
        TheEthernet::activate(c, o->mac_addr);
    }
    
    static void deinit (Context c)
    {
        TheEthernet::deinit(c);
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
    static void ethernet_activate_handler (Context c, bool error)
    {
        if (error) {
            APRINTER_CONSOLE_MSG("//EthActivateErr");
        } else {
            APRINTER_CONSOLE_MSG("//EthActivateOk");
        }
    }
    struct EthernetActivateHandler : public AMBRO_WFUNC_TD(&LwipNetwork::ethernet_activate_handler) {};
    
    static void ethernet_link_handler (Context c, bool link_status)
    {
        if (link_status) {
            APRINTER_CONSOLE_MSG("//EthLinkUp");
        } else {
            APRINTER_CONSOLE_MSG("//EthLinkDown");
        }
    }
    struct EthernetLinkHandler : public AMBRO_WFUNC_TD(&LwipNetwork::ethernet_link_handler) {};
    
    struct EthernetSendBuffer {
        size_t getTotalLength ()
        {
            return 0;
        }
        
        size_t getChunkLength ()
        {
            return 0;
        }
        
        char const * getChunkPtr ()
        {
            return nullptr;
        }
        
        bool nextChunk ()
        {
            return false;
        }
    };
    
    struct EthernetRecvBuffer {
        bool allocate (size_t length)
        {
            return false;
        }
        
        size_t getMaxChunkLength ()
        {
            return 0;
        }
        
        char * getChunkPtr ()
        {
            return nullptr;
        }
        
        void chunkWritten (size_t chunk_length)
        {
        }
    };
    
public:
    struct Object : public ObjBase<LwipNetwork, ParentObject, MakeTypeList<
        TheEthernet
    >> {
        uint8_t mac_addr[6];
    };
};

#include <aprinter/EndNamespace.h>

#endif
