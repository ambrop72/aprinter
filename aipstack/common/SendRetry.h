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

#include <aipstack/common/ObserverNotification.h>

namespace AIpStack {

class IpSendRetryList;

class IpSendRetryRequest :
    private Observer<IpSendRetryRequest>
{
    using BaseObserver = Observer<IpSendRetryRequest>;
    friend class IpSendRetryList;
    friend Observable<IpSendRetryRequest>;
    
public:
    inline bool isActive () const
    {
        return BaseObserver::isActive();
    }
    
    inline void reset ()
    {
        BaseObserver::reset();
    }
    
protected:
    virtual void retrySending () = 0;
};

class IpSendRetryList :
    private Observable<IpSendRetryRequest>
{
    using BaseObservable = Observable<IpSendRetryRequest>;
    
public:
    inline void reset ()
    {
        BaseObservable::reset();
    }
    
    inline bool hasRequests () const
    {
        return BaseObservable::hasObservers();
    }
    
    void addRequest (IpSendRetryRequest *req)
    {
        if (req != nullptr) {
            req->BaseObserver::reset();
            BaseObservable::addObserver(*req);
        }
    }
    
    void dispatchRequests ()
    {
        BaseObservable::notifyRemoveObservers([&](IpSendRetryRequest &request) {
            request.retrySending();
        });
    }
};

}

#endif
