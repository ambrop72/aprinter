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

#include <aprinter/base/Object.h>
#include <aprinter/base/Callback.h>

template <typename Context, typename ParentObject>
class LwipNetwork {
public:
    struct Object;
    
public:
    static size_t const MaxRecvChunkSize = ???;
    
    static void init (Context c)
    {
        lwip_init();
    }
    
    static void deinit (Context c)
    {
    }
    
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
    
public:
    struct Object : public ObjBase<LwipNetwork, ParentObject, EmptyTypeList> {
        //
    };
};

#endif
