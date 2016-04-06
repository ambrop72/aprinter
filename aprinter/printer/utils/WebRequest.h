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

#ifndef APRINTER_WEB_REQUEST_H
#define APRINTER_WEB_REQUEST_H

#include <stddef.h>

#include <aprinter/base/MemRef.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/PlacementNew.h>
#include <aprinter/net/http/HttpServerConstants.h>
#include <aprinter/printer/utils/JsonBuilder.h>

#include <aprinter/BeginNamespace.h>

template <typename Context>
class WebRequestCallback {
public:
    virtual void cbRequestTerminated (Context c) = 0;
    virtual void cbJsonBufferAvailable (Context c) = 0;
};

template <typename, typename>
class WebRequestHandler;

template <typename Context>
class WebRequest {
    template <typename, typename >
    friend class WebRequestHandler;
    
public:
    virtual MemRef getPath (Context c) = 0;
    virtual bool getParam (Context c, MemRef name, MemRef *value=nullptr) = 0;
    
    template <typename HandlerType>
    void acceptRequest (Context c)
    {
        void *ptr = doAcceptRequest(c, sizeof(HandlerType), alignof(HandlerType));
        HandlerType *state = (HandlerType *)ptr;
        new(state) HandlerType();
        static_cast<WebRequestHandler<Context, HandlerType> *>(state)->initRequest(c, this);
    }
    
private:
    virtual void * doAcceptRequest (Context c, size_t state_size, size_t state_align) = 0;
    virtual void doSetCallback (Context c, WebRequestCallback<Context> *callback) = 0;
    virtual void doCompleteHandling (Context c, char const *http_status) = 0;
    virtual void doWaitForJsonBuffer (Context c) = 0;
    virtual JsonBuilder * doStartJson (Context c) = 0;
    virtual bool doEndJson (Context c) = 0;
};

template <typename Context, typename HandlerType>
class WebRequestHandler : private WebRequestCallback<Context> {
    template <typename>
    friend class WebRequest;
    
public:
    void deinit (Context c) {}
    void jsonBufferAvailable (Context c) {}
    
public:
    MemRef getPath (Context c)
    {
        return m_request->getPath(c);
    }
    
    bool getParam (Context c, MemRef name, MemRef *value=nullptr)
    {
        return m_request->getParam(c, name, value);
    }
    
    void completeHandling (Context c, char const *http_status=nullptr)
    {
        return m_request->doCompleteHandling(c, http_status);
    }
    
    void waitForJsonBuffer (Context c)
    {
        return m_request->doWaitForJsonBuffer(c);
    }
    
    JsonBuilder * startJson (Context c)
    {
        return m_request->doStartJson(c);
    }
    
    bool endJson (Context c)
    {
        return m_request->doEndJson(c);
    }
    
private:
    HandlerType * user ()
    {
        return static_cast<HandlerType *>(this);
    }
    
    void initRequest (Context c, WebRequest<Context> *request)
    {
        m_request = request;
        m_request->doSetCallback(c, this);
        user()->init(c);
    }
    
    void cbRequestTerminated (Context c) override
    {
        user()->deinit(c);
        user()->~HandlerType();
    }
    
    void cbJsonBufferAvailable (Context c) override
    {
        return user()->jsonBufferAvailable(c);
    }
    
private:
    WebRequest<Context> *m_request;
};

#include <aprinter/EndNamespace.h>

#endif
