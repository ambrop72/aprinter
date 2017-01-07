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

#ifndef APRINTER_IPSTACK_SEND_RETRY_H
#define APRINTER_IPSTACK_SEND_RETRY_H

#include <aprinter/base/Assert.h>
#include <aprinter/structure/DoubleEndedList.h>

#include <aipstack/BeginNamespace.h>

class IpSendRetry {
public:
    class List;
    
    class Request {
        friend IpSendRetry;
        friend class List;
        
    public:
        void init ()
        {
            m_list = nullptr;
        }
        
        void deinit ()
        {
            reset();
        }
        
        void reset ()
        {
            if (m_list != nullptr) {
                m_list->m_list.remove(this);
                m_list = nullptr;
            }
        }
        
    public:
        virtual void retrySending () = 0;
        
    private:
        List *m_list;
        APrinter::DoubleEndedListNode<Request> m_list_node;
    };
    
private:
    using ListStructure = APrinter::DoubleEndedList<Request, &Request::m_list_node, false>;
    
public:
    class List {
        friend class Request;
        
    public:
        void init ()
        {
            m_list.init();
        }
        
        void deinit ()
        {
            reset();
        }
        
        void reset ()
        {
            for (Request *req = m_list.first(); req != nullptr; req = m_list.next(req)) {
                AMBRO_ASSERT(req->m_list == this)
                req->m_list = nullptr;
            }
            m_list.init();
        }
        
        void addRequest (Request *req)
        {
            if (req != nullptr) {
                if (req->m_list != nullptr) {
                    req->m_list->m_list.remove(req);
                }
                m_list.prepend(req);
                req->m_list = this;
            }
        }
        
        void dispatchRequests ()
        {
            // Move the requests to a temporary list.
            List temp_list;
            temp_list.m_list = m_list;
            for (Request *req = m_list.first(); req != nullptr; req = m_list.next(req)) {
                AMBRO_ASSERT(req->m_list == this)
                req->m_list = &temp_list;
            }
            m_list.init();
            
            // Dispatch the requests from the temporary list.
            // We do it this way to avoid any issues if the callback adds the request
            // back or adds or removes other requests. We can safely consume the
            // temporary list from the front, as it is not possible that any request
            // be added back to it.
            Request *req;
            while ((req = temp_list.m_list.first()) != nullptr) {
                AMBRO_ASSERT(req->m_list == &temp_list)
                temp_list.m_list.removeFirst();
                req->m_list = nullptr;
                
                req->retrySending();
            }
        }
        
    private:
        ListStructure m_list;
    };
};

#include <aipstack/EndNamespace.h>

#endif
