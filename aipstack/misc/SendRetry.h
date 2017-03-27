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

#include <aprinter/base/Preprocessor.h>
#include <aprinter/structure/ObserverNotification.h>

#include <aipstack/BeginNamespace.h>

class IpSendRetry {
private:
    APRINTER_USE_TYPES1(APrinter::ObserverNotification, (Observer, Observable))
    
public:
    enum RequestType {
        RequestTypeNormal = 0,
        RequestTypeIpEthHwArpQuery = 1,
    };
    
private:
    class BaseRequest : public Observer
    {
        friend IpSendRetry;
        
    private:
        virtual RequestType getRequestType () = 0;
    };
    
public:
    class Request :
        private BaseRequest
    {
        friend IpSendRetry;
        
    public:
        using BaseRequest::init;
        using BaseRequest::deinit;
        using BaseRequest::reset;
        using BaseRequest::isActive;
        
    protected:
        virtual void retrySending () = 0;
        
    private:
        RequestType getRequestType () override final
        {
            return RequestTypeNormal;
        }
    };
    
    class BaseSpecialRequest :
        private BaseRequest
    {
        friend IpSendRetry;
        
    private:
        BaseSpecialRequest() = default;
    };
    
    template <RequestType SpecialRequestType>
    class SpecialRequest : public BaseSpecialRequest
    {
        static_assert(SpecialRequestType != RequestTypeNormal, "");
        
    public:
        using BaseRequest::init;
        using BaseRequest::deinit;
        using BaseRequest::reset;
        using BaseRequest::isActive;
        
    private:
        RequestType getRequestType () override final
        {
            return SpecialRequestType;
        }
    };
    
    class List :
        private Observable
    {
    public:
        inline void init ()
        {
            Observable::init();
        }
        
        inline void deinit ()
        {
            Observable::removeObservers();
        }
        
        inline void reset ()
        {
            Observable::removeObservers();
        }
        
        void addRequest (Request *req)
        {
            if (req != nullptr) {
                req->Observer::reset();
                req->Observer::observe(*this);
            }
        }
        
        void addSpecialRequest (BaseSpecialRequest &req)
        {
            AMBRO_ASSERT(req.getRequestType() != RequestTypeNormal)
            
            req.Observer::reset();
            req.Observer::observe(*this);
        }
        
        template <typename DispatchSpecialRequest>
        void dispatchRequests (DispatchSpecialRequest dispatch_special_request)
        {
            Observable::template notifyObservers<true>([&](Observer &observer) {
                BaseRequest &base_request = static_cast<BaseRequest &>(observer);
                RequestType request_type = base_request.getRequestType();
                if (request_type == RequestTypeNormal) {
                    Request &request = static_cast<Request &>(base_request);
                    request.retrySending();
                } else {
                    BaseSpecialRequest &special_request = static_cast<BaseSpecialRequest &>(base_request);
                    dispatch_special_request(special_request, request_type);
                }
            });
        }
    };
};

#include <aipstack/EndNamespace.h>

#endif
