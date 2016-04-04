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

#ifndef APRINTER_CONFIG_WEB_API_MODULE_H
#define APRINTER_CONFIG_WEB_API_MODULE_H

#include <aprinter/meta/AliasStruct.h>
#include <aprinter/meta/TypeListUtils.h>
#include <aprinter/base/MemRef.h>
#include <aprinter/printer/ServiceList.h>
#include <aprinter/printer/utils/WebRequest.h>
#include <aprinter/printer/utils/JsonBuilder.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, typename ThePrinterMain, typename Params>
class ConfigWebApiModule {
public:
    static bool handle_web_request (Context c, MemRef req_type, WebRequest<Context> *request)
    {
        if (req_type.equalTo("config")) {
            request->template acceptRequest<ConfigRequest>(c);
            return false;
        }
        return true;
    }
    
private:
    class ConfigRequest : public WebRequestHandler<Context, ConfigRequest> {
    public:
        void init (Context c)
        {
            JsonBuilder *json = this->startJson(c);
            json->startObject();
            json->addSafeKeyVal("test", JsonString{"LALALA"});
            json->endObject();
            this->endJson(c);
            
            return this->completeHandling(c);
        }
        
        //void deinit (Context c) {}
        //void jsonBufferAvailable (Context c) {}
    };
    
public:
    using WebApiRequestHandlers = MakeTypeList<ConfigRequest>;
    
public:
    struct Object {};
};

struct ConfigWebApiModuleService {
    APRINTER_MODULE_TEMPLATE(ConfigWebApiModuleService, ConfigWebApiModule)
    
    using ProvidedServices = MakeTypeList<ServiceDefinition<ServiceList::WebApiHandlerService>>;
};

#include <aprinter/EndNamespace.h>

#endif
