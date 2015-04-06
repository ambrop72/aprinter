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

#include <aprinter/meta/MinMax.h>
#include <aprinter/meta/WrapFunction.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Callback.h>
#include <aprinter/base/WrapBuffer.h>
#include <aprinter/structure/DoubleEndedList.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, typename ActivateHandler, typename Params>
class BlockAccess {
public:
    struct Object;
    
private:
    using TheDebugObject = DebugObject<Context, Object>;
    struct SdInitHandler;
    struct SdCommandHandler;
    using TheSd = typename Params::SdService::template SdCard<Context, Object, 1, SdInitHandler, SdCommandHandler>;
    enum {STATE_INACTIVE, STATE_ACTIVATING, STATE_READY, STATE_BUSY};
    
public:
    using BlockIndexType = typename TheSd::BlockIndexType;
    static size_t const BlockSize = TheSd::BlockSize;
    
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        TheSd::init(c);
        o->state = STATE_INACTIVE;
        
        TheDebugObject::init(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
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
        AMBRO_ASSERT(o->state != STATE_INACTIVE)
        
        TheSd::deactivate(c);
        o->state = STATE_INACTIVE;
    }
    
    static BlockIndexType getCapacityBlocks (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == STATE_READY || o->state == STATE_BUSY)
        
        return TheSd::getCapacityBlocks(c);
    }
    
    using GetSd = TheSd;
    
    class User {
    private:
        friend BlockAccess;
        enum {USER_STATE_IDLE, USER_STATE_READING};
        
    public:
        using ReadHandlerType = Callback<void(Context c, bool error)>;
        
        void init (Context c, ReadHandlerType read_handler)
        {
            m_read_handler = read_handler;
            m_state = USER_STATE_IDLE;
        }
        
        // WARNING: Only allowed together with deiniting the whole storage!
        void deinit (Context c)
        {
        }
        
        void startRead (Context c, BlockIndexType block_idx, WrapBuffer buf)
        {
            auto *o = Object::self(c);
            TheDebugObject::access(c);
            AMBRO_ASSERT(o->state == STATE_READY || o->state == STATE_BUSY)
            AMBRO_ASSERT(m_state == USER_STATE_IDLE)
            
            m_state = USER_STATE_READING;
            m_block_idx = block_idx;
            m_buf = buf;
            add_request(c, this);
        }
        
    private:
        ReadHandlerType m_read_handler;
        uint8_t m_state;
        BlockIndexType m_block_idx;
        WrapBuffer m_buf;
        DoubleEndedListNode<User> m_list_node;
    };
    
private:
    using QueueList = DoubleEndedList<User, &User::m_list_node>;
    
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
    
    static void sd_command_handler (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == STATE_BUSY)
        AMBRO_ASSERT(!o->queue.isEmpty())
        
        bool error;
        if (!TheSd::checkReadBlock(c, &o->read_state, &error)) {
            return;
        }
        TheSd::unsetEvent(c);
        User *user = o->queue.first();
        AMBRO_ASSERT(user->m_state == USER_STATE_READING)
        o->queue.removeFirst();
        o->state = STATE_READY;
        continue_queue(c);
        user->m_state = USER_STATE_IDLE;
        return user->m_read_handler(c, error);
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
            AMBRO_ASSERT(user->m_state == USER_STATE_READING)
            size_t effective_wrap = MinValue(BlockSize, user->m_buf.wrap);
            TheSd::queueReadBlock(c, user->m_block_idx, user->m_buf.ptr1, effective_wrap, user->m_buf.ptr2, &o->read_state);
            o->state = STATE_BUSY;
        }
    }
    
public:
    struct Object : public ObjBase<BlockAccess, ParentObject, MakeTypeList<
        TheDebugObject,
        TheSd
    >> {
        uint8_t state;
        typename TheSd::ReadState read_state;
        QueueList queue;
    };
};

template <typename TSdService>
struct BlockAccessService {
    using SdService = TSdService;
    
    template <typename Context, typename ParentObject, typename ActivateHandler>
    using Access = BlockAccess<Context, ParentObject, ActivateHandler, BlockAccessService>;
};

#include <aprinter/EndNamespace.h>

#endif
