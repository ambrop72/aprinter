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

#ifndef APRINTER_WEB_INTERFACE_MODULE_H
#define APRINTER_WEB_INTERFACE_MODULE_H

#include <stdint.h>
#include <stddef.h>

#include <aprinter/meta/WrapFunction.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/ProgramMemory.h>
#include <aprinter/printer/HttpServer.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, typename ThePrinterMain, typename Params>
class WebInterfaceModule {
public:
    struct Object;
    
private:
    static size_t const HttpMaxRequestLineLength = 128;
    static size_t const HttpMaxHeaderLineLength = 128;
    static size_t const HttpExpectedResponseLength = 250;
    
    struct HttpRequestHandler;
    using TheTheHttpServerService = HttpServerService<Params::Port, Params::MaxClients, HttpMaxRequestLineLength, HttpMaxHeaderLineLength, HttpExpectedResponseLength>;
    using TheHttpServer = typename TheTheHttpServerService::template Server<Context, Object, ThePrinterMain, HttpRequestHandler>;
    
public:
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        TheHttpServer::init(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        
        TheHttpServer::deinit(c);
    }
    
private:
    static bool http_request_handler (Context c, typename TheHttpServer::HttpRequest const *request, char const **status)
    {
        auto *o = Object::self(c);
        
        auto *output = ThePrinterMain::get_msg_output(c);
        output->reply_append_pstr(c, AMBRO_PSTR("//HttpRequest "));
        output->reply_append_str(c, request->method);
        output->reply_append_ch(c, ' ');
        output->reply_append_str(c, request->path);
        output->reply_append_ch(c, '\n');
        output->reply_poke(c);
        
        *status = TheHttpServer::HttpStatusCodes::NotFound();
        return false;
    }
    struct HttpRequestHandler : public AMBRO_WFUNC_TD(&WebInterfaceModule::http_request_handler) {};
    
public:
    struct Object : public ObjBase<WebInterfaceModule, ParentObject, MakeTypeList<
        TheHttpServer
    >> {
        //
    };
};

template <
    uint16_t TPort,
    int TMaxClients
>
struct WebInterfaceModuleService {
    static uint16_t const Port = TPort;
    static int const MaxClients = TMaxClients;
    
    template <typename Context, typename ParentObject, typename ThePrinterMain>
    using Module = WebInterfaceModule<Context, ParentObject, ThePrinterMain, WebInterfaceModuleService>;
};

#include <aprinter/EndNamespace.h>

#endif
