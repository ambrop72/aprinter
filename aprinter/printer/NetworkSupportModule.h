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

#ifndef APRINTER_NETWORK_SUPPORT_MODULE_H
#define APRINTER_NETWORK_SUPPORT_MODULE_H

#include <aprinter/base/Object.h>
#include <aprinter/printer/Configuration.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, typename ThePrinterMain, typename Params>
class NetworkSupportModule {
public:
    struct Object;
    
private:
    using Config = typename ThePrinterMain::Config;
    using TheNetwork = typename Context::Network;
    
    using CMacAddress = decltype(Config::e(Params::MacAddress::i()));
    
public:
    static void configuration_changed (Context c)
    {
        if (!TheNetwork::isActivated(c)) {
            auto mac = APRINTER_CFG(Config, CMacAddress, c);
            TheNetwork::activate(c, typename TheNetwork::ActivateParams{mac.mac_addr});
        }
    }
    
public:
    using ConfigExprs = MakeTypeList<CMacAddress>;
    
    struct Object : public ObjBase<NetworkSupportModule, ParentObject, EmptyTypeList> {};
};

template <
    typename TMacAddress
>
struct NetworkSupportModuleService {
    using MacAddress = TMacAddress;
    
    template <typename Context, typename ParentObject, typename ThePrinterMain>
    using Module = NetworkSupportModule<Context, ParentObject, ThePrinterMain, NetworkSupportModuleService>;
};

#include <aprinter/EndNamespace.h>

#endif
