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
#include <aprinter/base/Accessor.h>
#include <aprinter/structure/LinkedList.h>
#include <aprinter/structure/LinkModel.h>

#include <aipstack/BeginNamespace.h>

class IpSendRetry {
private:
    struct ListEntry;
    
    using LinkModel = APrinter::PointerLinkModel<ListEntry>;
    
    using ListNode = APrinter::LinkedListNode<LinkModel>;
    
    struct ListEntry : public ListNode {};
    
    using ListNodeAccessor = APrinter::BaseClassAccessor<ListEntry, ListNode>;
    
    using ListStructure = APrinter::AnonymousLinkedList<ListNodeAccessor, LinkModel>;
    
public:
    class Request :
        private ListEntry
    {
        friend IpSendRetry;
        
    public:
        void init ()
        {
            ListStructure::markRemoved(*this);
        }
        
        void deinit ()
        {
            reset();
        }
        
        void reset ()
        {
            if (!ListStructure::isRemoved(*this)) {
                ListStructure::remove(*this);
                ListStructure::markRemoved(*this);
            }
        }
        
    protected:
        virtual void retrySending () = 0;
    };
    
    class List :
        private ListEntry
    {
        friend IpSendRetry;
        
    public:
        void init ()
        {
            ListStructure::initLonely(*this);
        }
        
        void deinit ()
        {
            reset();
        }
        
        void reset ()
        {
            ListEntry *e = ListStructure::next(*this);
            while (e != nullptr) {
                AMBRO_ASSERT(!ListStructure::isRemoved(*e))
                ListEntry *next = ListStructure::next(*e);
                ListStructure::markRemoved(*e);
                e = next;
            }
            ListStructure::initLonely(*this);
        }
        
        void addRequest (Request *req)
        {
            if (req != nullptr) {
                if (!ListStructure::isRemoved(*req)) {
                    ListStructure::remove(*req);
                }
                ListStructure::initAfter(*req, *this);
            }
        }
        
        void dispatchRequests ()
        {
            // Move the requests to a temporary list and clear the main list.
            ListEntry temp_head;
            ListStructure::replaceFirst(temp_head, *this);
            ListStructure::initLonely(*this);
            
            // Dispatch the requests from the temporary list.
            // We do it this way to avoid any issues if the callback adds the request
            // back or adds or removes other requests. We can safely consume the
            // temporary list from the front, as it is not possible that any request
            // be added back to it.
            ListEntry *e;
            while ((e = ListStructure::next(temp_head)) != nullptr) {
                AMBRO_ASSERT(!ListStructure::isRemoved(*e))
                ListStructure::remove(*e);
                ListStructure::markRemoved(*e);
                Request *req = static_cast<Request *>(e);
                req->retrySending();
            }
            
            AMBRO_ASSERT(ListStructure::prev(temp_head).isNull())
        }
    };
};

#include <aipstack/EndNamespace.h>

#endif
