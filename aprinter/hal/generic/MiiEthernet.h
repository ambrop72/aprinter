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

#ifndef APRINTER_MII_ETHERNET_H
#define APRINTER_MII_ETHERNET_H

//#define APRINTER_DEBUG_MII

#include <stdint.h>

#include <aprinter/meta/WrapFunction.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/Callback.h>
#include <aprinter/base/Assert.h>
#include <aprinter/hal/common/MiiCommon.h>

#ifdef APRINTER_DEBUG_MII
#include <aprinter/base/ProgramMemory.h>
#include <aprinter/printer/Console.h>
#endif

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, typename ClientParams, typename Params>
class MiiEthernet {
public:
    struct Object;
    
private:
    using SendBufferType = typename ClientParams::SendBufferType;
    
    struct MiiActivateHandler;
    struct MiiPhyMaintHandler;
    using TheMiiClientParams = MiiClientParams<MiiActivateHandler, MiiPhyMaintHandler, typename ClientParams::ReceiveHandler, SendBufferType, Params::PhyService::Rmii>;
    using TheMii = typename Params::MiiService::template Mii<Context, Object, TheMiiClientParams>;
    
    class PhyRequester;
    using ThePhyClientParams = PhyClientParams<PhyRequester>;
    using ThePhy = typename Params::PhyService::template Phy<Context, Object, ThePhyClientParams>;
    
    enum class InitState : uint8_t {INACTIVE, INITING, RUNNING};
    
public:
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        TheMii::init(c);
        ThePhy::init(c);
        o->init_state = InitState::INACTIVE;
        o->link_up = false;
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        
        ThePhy::deinit(c);
        TheMii::deinit(c);
    }
    
    static void reset (Context c)
    {
        auto *o = Object::self(c);
        
        ThePhy::reset(c);
        TheMii::reset(c);
        o->init_state = InitState::INACTIVE;
        o->link_up = false;
    }
    
    static void activate (Context c, uint8_t const *mac_addr)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->init_state == InitState::INACTIVE)
        
        o->init_state = InitState::INITING;
        TheMii::activate(c, mac_addr);
    }
    
    static bool recvFrame (Context c, char *data, size_t max_length, size_t *out_length)
    {
        auto *o = Object::self(c);
        
        if (o->init_state != InitState::RUNNING) {
            return false;
        }
        return TheMii::recvFrame(c, data, max_length, out_length);
    }
    
    static bool sendFrame (Context c, SendBufferType *send_buffer)
    {
        auto *o = Object::self(c);
        
        if (o->init_state != InitState::RUNNING) {
            return false;
        }
        return TheMii::sendFrame(c, send_buffer);
    }
    
    static bool getLinkUp (Context c)
    {
        auto *o = Object::self(c);
        return o->link_up;
    }
    
private:
    static void mii_activate_handler (Context c, bool error)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->init_state == InitState::INITING)
        AMBRO_ASSERT(!o->link_up)
        
        if (error) {
            o->init_state = InitState::INACTIVE;
        } else {
            o->init_state = InitState::RUNNING;
            ThePhy::activate(c);
        }
        
        return ClientParams::ActivateHandler::call(c, error);
    }
    struct MiiActivateHandler : public AMBRO_WFUNC_TD(&MiiEthernet::mii_activate_handler) {};
    
    static void mii_phy_maint_handler (Context c, MiiPhyMaintResult result)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->init_state == InitState::RUNNING)
        
#ifdef APRINTER_DEBUG_MII
        auto *out = Context::Printer::get_msg_output(c);
        out->reply_append_pstr(c, AMBRO_PSTR("//MiiResult err="));
        out->reply_append_uint32(c, result.error);
        out->reply_append_pstr(c, AMBRO_PSTR(" data="));
        out->reply_append_uint32(c, result.data);
        out->reply_append_ch(c, '\n');
        out->reply_poke(c);
#endif
        
        return ThePhy::phyMaintCompleted(c, result);
    }
    struct MiiPhyMaintHandler : public AMBRO_WFUNC_TD(&MiiEthernet::mii_phy_maint_handler) {};
    
    class PhyRequester {
    public:
        static uint8_t const SupportedSpeeds = TheMii::SupportedSpeeds;
        static uint8_t const SupportedPause = TheMii::SupportedPause;
        
        static void startPhyMaintenance (Context c, MiiPhyMaintCommand command)
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(o->init_state == InitState::RUNNING)
            
#ifdef APRINTER_DEBUG_MII
            auto *out = Context::Printer::get_msg_output(c);
            if (command.io_type == PhyMaintCommandIoType::READ_WRITE) {
                out->reply_append_pstr(c, AMBRO_PSTR("//MiiWrite reg="));
            } else {
                out->reply_append_pstr(c, AMBRO_PSTR("//MiiRead reg="));
            }
            out->reply_append_uint32(c, command.reg_address);
            if (command.io_type == PhyMaintCommandIoType::READ_WRITE) {
                out->reply_append_pstr(c, AMBRO_PSTR(" data="));
                out->reply_append_uint32(c, command.data);
            }
            out->reply_append_ch(c, '\n');
            out->reply_poke(c);
#endif
            
            return TheMii::startPhyMaintenance(c, command);
        }
        
        static void linkIsUp (Context c, MiiLinkParams link_params)
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(o->init_state == InitState::RUNNING)
            AMBRO_ASSERT(!o->link_up)
            
            TheMii::configureLink(c, link_params);
            o->link_up = true;
            
            return ClientParams::LinkHandler::call(c, true);
        }
        
        static void linkIsDown (Context c)
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(o->init_state == InitState::RUNNING)
            
            if (!o->link_up) {
                return;
            }
            
            TheMii::resetLink(c);
            o->link_up = false;
            
            return ClientParams::LinkHandler::call(c, false);
        }
    };
    
public:
    using GetMii = TheMii;
    
    struct Object : public ObjBase<MiiEthernet, ParentObject, MakeTypeList<
        TheMii,
        ThePhy
    >> {
        InitState init_state;
        bool link_up;
    };
};

template <
    typename TMiiService,
    typename TPhyService
>
struct MiiEthernetService {
    using MiiService = TMiiService;
    using PhyService = TPhyService;
    
    template <typename Context, typename ParentObject, typename ClientParams>
    using Ethernet = MiiEthernet<Context, ParentObject, ClientParams, MiiEthernetService>;
};

#include <aprinter/EndNamespace.h>

#endif
