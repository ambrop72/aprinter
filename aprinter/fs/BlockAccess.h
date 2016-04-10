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

#ifndef AMBROLIB_BLOCK_ACCESS_H
#define AMBROLIB_BLOCK_ACCESS_H

#include <stdint.h>
#include <stddef.h>

#include <aprinter/meta/WrapFunction.h>
#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Callback.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/TransferVector.h>
#include <aprinter/structure/DoubleEndedList.h>

#include <aprinter/BeginNamespace.h>

template <typename Arg>
class BlockAccess {
    using Context         = typename Arg::Context;
    using ParentObject    = typename Arg::ParentObject;
    using ActivateHandler = typename Arg::ActivateHandler;
    using Params          = typename Arg::Params;
    
public:
    struct Object;
    class User;
    class UserFull;
    
private:
    using TheDebugObject = DebugObject<Context, Object>;
    struct SdInitHandler;
    struct SdCommandHandler;
    struct SdArg : public Params::SdService::template SdCard<Context, Object, SdInitHandler, SdCommandHandler> {};
    using TheSd = typename SdArg::template Instance<SdArg>;
    
    enum {STATE_INACTIVE, STATE_ACTIVATING, STATE_READY, STATE_BUSY};
    
public:
    using BlockIndexType = typename TheSd::BlockIndexType;
    static size_t const BlockSize = TheSd::BlockSize;
    using DataWordType = typename TheSd::DataWordType;
    static size_t const MaxIoBlocks = TheSd::MaxIoBlocks;
    static int const MaxIoDescriptors = TheSd::MaxIoDescriptors;
    static int const MaxBufferLocks = 1;
    
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        TheSd::init(c);
        o->state = STATE_INACTIVE;
        
        TheDebugObject::init(c);
    }
    
    static void deinit (Context c)
    {
        TheDebugObject::deinit(c);
        
        TheSd::deinit(c);
    }
    
    static void activate (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == STATE_INACTIVE)
        
        TheSd::activate(c);
        o->state = STATE_ACTIVATING;
    }
    
    static void deactivate (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        if (o->state != STATE_INACTIVE) {
            TheSd::deactivate(c);
        }
        o->state = STATE_INACTIVE;
    }
    
    static BlockIndexType getCapacityBlocks (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == STATE_READY || o->state == STATE_BUSY)
        
        BlockIndexType capacity = TheSd::getCapacityBlocks(c);
        AMBRO_ASSERT(capacity > 0)
        return capacity;
    }
    
    static bool isWritable (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == STATE_READY || o->state == STATE_BUSY)
        
        return TheSd::isWritable(c);
    }
    
    using GetSd = TheSd;
    
    class User {
    private:
        friend BlockAccess;
        friend class UserFull;
        
        enum {USER_STATE_IDLE, USER_STATE_READING, USER_STATE_WRITING};
        
    public:
        using HandlerType = Callback<void(Context c, bool error)>;
        
        void init (Context c, HandlerType handler)
        {
            m_handler = handler;
            m_state = USER_STATE_IDLE;
            m_is_full = false;
        }
        
        // WARNING: Only allowed together with deiniting the whole storage or when not reading!
        void deinit (Context c)
        {
        }
        
        void startReadOrWrite (Context c, bool is_write, BlockIndexType block_idx, size_t num_blocks, TransferVector<DataWordType> data_vector)
        {
            auto *o = Object::self(c);
            TheDebugObject::access(c);
            AMBRO_ASSERT(o->state == STATE_READY || o->state == STATE_BUSY)
            AMBRO_ASSERT(m_state == USER_STATE_IDLE)
            
            m_state = is_write ? USER_STATE_WRITING : USER_STATE_READING;
            m_block_idx = block_idx;
            m_num_blocks = num_blocks;
            m_data_vector = data_vector;
            
            add_request(c, this);
        }
        
    private:
        void maybe_call_locker (Context c, bool lock_else_unlock)
        {
            if (m_is_full) {
                UserFull *userfull = static_cast<UserFull *>(this);
                if (userfull->m_locker) {
                    userfull->m_locker(c, lock_else_unlock);
                }
            }
        }
        
        HandlerType m_handler;
        uint8_t m_state : 3;
        bool m_is_full : 1;
        BlockIndexType m_block_idx;
        size_t m_num_blocks;
        TransferVector<DataWordType> m_data_vector;
        DoubleEndedListNode<User> m_list_node;
    };
    
    class UserFull : public User {
        friend class User;
        
    public:
        using LockerType = Callback<void(Context c, bool lock_else_unlock)>;
        
        void init (Context c, typename User::HandlerType handler)
        {
            User::init(c, handler);
            this->m_is_full = true;
            m_locker = LockerType::MakeNull();
        }
        
        void setLocker (Context c, LockerType locker)
        {
            m_locker = locker;
        }
        
    private:
        LockerType m_locker;
    };
    
private:
    static void sd_init_handler (Context c, uint8_t error_code)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == STATE_ACTIVATING)
        
        if (error_code) {
            o->state = STATE_INACTIVE;
        } else {
            o->state = STATE_READY;
            o->queue.init();
        }
        return ActivateHandler::call(c, error_code);
    }
    struct SdInitHandler : public AMBRO_WFUNC_TD(&BlockAccess::sd_init_handler) {};
    
    static void sd_command_handler (Context c, bool error)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == STATE_BUSY)
        AMBRO_ASSERT(!o->queue.isEmpty())
        
        User *user = o->queue.first();
        AMBRO_ASSERT(user->m_state == User::USER_STATE_READING || user->m_state == User::USER_STATE_WRITING)
        
        if (user->m_state == User::USER_STATE_WRITING) {
            user->maybe_call_locker(c, false);
        }
        
        o->queue.removeFirst();
        user->m_state = User::USER_STATE_IDLE;
        o->state = STATE_READY;
        
        continue_queue(c);
        
        return user->m_handler(c, error);
    }
    struct SdCommandHandler : public AMBRO_WFUNC_TD(&BlockAccess::sd_command_handler) {};
    
    static void add_request (Context c, User *user)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->state == STATE_READY || o->state == STATE_BUSY)
        
        o->queue.append(user);
        if (o->state == STATE_READY) {
            continue_queue(c);
        }
    }
    
    static void continue_queue (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->state == STATE_READY)
        
        User *user = o->queue.first();
        if (user) {
            AMBRO_ASSERT(user->m_state == User::USER_STATE_READING || user->m_state == User::USER_STATE_WRITING)
            bool is_write = (user->m_state == User::USER_STATE_WRITING);
            if (is_write) {
                user->maybe_call_locker(c, true);
            }
            TheSd::startReadOrWrite(c, is_write, user->m_block_idx, user->m_num_blocks, user->m_data_vector);
            o->state = STATE_BUSY;
        }
    }
    
public:
    struct Object : public ObjBase<BlockAccess, ParentObject, MakeTypeList<
        TheDebugObject,
        TheSd
    >> {
        uint8_t state;
        DoubleEndedList<User, &User::m_list_node> queue;
    };
};

APRINTER_ALIAS_STRUCT_EXT(BlockAccessService, (
    APRINTER_AS_TYPE(SdService)
), (
    APRINTER_ALIAS_STRUCT_EXT(Access, (
        APRINTER_AS_TYPE(Context),
        APRINTER_AS_TYPE(ParentObject),
        APRINTER_AS_TYPE(ActivateHandler)
    ), (
        using Params = BlockAccessService;
        APRINTER_DEF_INSTANCE(Access, BlockAccess)
    ))
))

#include <aprinter/EndNamespace.h>

#endif
