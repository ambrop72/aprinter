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

#ifndef APRINTER_BLOCK_CACHE_H
#define APRINTER_BLOCK_CACHE_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include <aprinter/meta/ChooseInt.h>
#include <aprinter/meta/StructIf.h>
#include <aprinter/meta/FunctionIf.h>
#include <aprinter/meta/WrapValue.h>
#include <aprinter/meta/If.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/Callback.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/WrapBuffer.h>
#include <aprinter/structure/DoubleEndedList.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, typename TTheBlockAccess, int TNumCacheEntries, bool TWritable>
class BlockCache {
public:
    struct Object;
    using TheBlockAccess = TTheBlockAccess;
    static int const NumCacheEntries = TNumCacheEntries;
    static bool const Writable = TWritable;
    
private:
    static_assert(NumCacheEntries > 0, "");
    
    class CacheEntry;
    using TheDebugObject = DebugObject<Context, Object>;
    using BlockAccessUser = If<Writable, typename TheBlockAccess::UserFull, typename TheBlockAccess::User>;
    using DirtTimeType = uint32_t;
    static constexpr DirtTimeType DirtSignBit = ((DirtTimeType)-1 / 2) + 1;
    using CacheEntryIndexType = ChooseIntForMax<NumCacheEntries, true>;
    static int const NumBuffers = NumCacheEntries + (Writable ? TheBlockAccess::MaxBufferLocks : 0);
    using BufferIndexType = ChooseIntForMax<NumBuffers, true>;
    
public:
    using BlockIndexType = typename TheBlockAccess::BlockIndexType;
    static size_t const BlockSize = TheBlockAccess::BlockSize;
    
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        writable_init(c);
        
        for (CacheEntryIndexType i = 0; i < NumCacheEntries; i++) {
            o->cache_entries[i].init(c);
        }
        
        TheDebugObject::init(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::deinit(c);
        writable_deinit_assert(c);
        
        for (CacheEntryIndexType i = 0; i < NumCacheEntries; i++) {
            o->cache_entries[i].deinit(c);
        }
        
        writable_deinit(c);
    }
    
    template <typename Dummy=void>
    class FlushRequest : private SimpleDebugObject<Context> {
        friend BlockCache;
        
        enum class State : uint8_t {IDLE, WAITING, REPORTING};
        
    public:
        using FlushHandler = Callback<void(Context c, bool error)>;
        
        void init (Context c, FlushHandler handler)
        {
            m_handler = handler;
            m_event.init(c, APRINTER_CB_OBJFUNC_T(&FlushRequest::event_handler, this));
            m_state = State::IDLE;
            this->debugInit(c);
        }
        
        void deinit (Context c)
        {
            this->debugDeinit(c);
            reset_internal(c);
            m_event.deinit(c);
        }
        
        void reset (Context c)
        {
            this->debugAccess(c);
            reset_internal(c);
        }
        
        void requestFlush (Context c)
        {
            this->debugAccess(c);
            TheDebugObject::access(c);
            auto *o = Object::self(c);
            AMBRO_ASSERT(m_state == State::IDLE)
            
            if (is_flush_completed(c, true, nullptr)) {
                return complete(c, false);
            }
            o->waiting_flush_requests.prepend(this);
            m_state = State::WAITING;
            start_writing_for_flush(c);
        }
        
    private:
        void reset_internal (Context c)
        {
            if (m_state == State::WAITING) {
                auto *o = Object::self(c);
                o->waiting_flush_requests.remove(this);
            }
            m_event.unset(c);
            m_state = State::IDLE;
        }
        
        void complete (Context c, bool error)
        {
            m_state = State::REPORTING;
            m_error = error;
            m_event.prependNowNotAlready(c);
        }
        
        void flush_request_result (Context c, bool error)
        {
            this->debugAccess(c);
            AMBRO_ASSERT(m_state == State::WAITING)
            
            complete(c, error);
        }
        
        void event_handler (Context c)
        {
            this->debugAccess(c);
            AMBRO_ASSERT(m_state == State::REPORTING)
            
            m_state = State::IDLE;
            return m_handler(c, m_error);
        }
        
        FlushHandler m_handler;
        DoubleEndedListNode<FlushRequest> m_waiting_flush_requests_node;
        typename Context::EventLoop::QueuedEvent m_event;
        State m_state;
        bool m_error;
    };
    
    APRINTER_STRUCT_IF_TEMPLATE(CacheRefWritableMemebers) {
        BlockIndexType m_allocating_block;
        BlockIndexType m_write_stride;
        uint8_t m_write_count;
        bool m_allocating_for_user;
        bool m_no_need_to_read;
    };
    
    class CacheRef : private SimpleDebugObject<Context>, private CacheRefWritableMemebers<Writable> {
        friend BlockCache;
        friend class CacheEntry;
        
        enum class State : uint8_t {INVALID, ALLOCATING_ENTRY, WAITING_READ, INIT_COMPL_EVENT, AVAILABLE};
        
    public:
        using CacheHandler = Callback<void(Context c, bool error)>;
        
        enum Flags : uint8_t {
            FLAG_NO_IMMEDIATE_COMPLETION = 1 << 0,
            FLAG_NO_NEED_TO_READ         = 1 << 1
        };
        
        void init (Context c, CacheHandler handler)
        {
            m_handler = handler;
            m_event.init(c, APRINTER_CB_OBJFUNC_T(&CacheRef::event_handler, this));
            m_state = State::INVALID;
            m_entry_index = -1;
            this->debugInit(c);
        }
        
        void deinit (Context c)
        {
            this->debugDeinit(c);
            reset_internal(c);
            m_event.deinit(c);
        }
        
        void reset (Context c)
        {
            this->debugAccess(c);
            reset_internal(c);
        }
        
        bool requestBlock (Context c, BlockIndexType block, BlockIndexType write_stride, uint8_t write_count, uint8_t flags)
        {
            this->debugAccess(c);
            TheDebugObject::access(c);
            
            if (!(flags & FLAG_NO_IMMEDIATE_COMPLETION) && (is_allocating_block(block) || (m_entry_index != -1 && !get_entry(c)->isAssignedForUser(c) && block == get_entry(c)->getBlock(c)))) {
                check_write_params(write_stride, write_count);
                return m_state == State::AVAILABLE;
            }
            request_common(c, block, write_stride, write_count, flags, false);
            return false; // never do we end up in State::AVAILABLE in this branch
        }
        
        void requestUserBuffer (Context c)
        {
            this->debugAccess(c);
            TheDebugObject::access(c);
            
            request_common(c, 0, 0, 0, 0, true);
        }
        
        bool isAvailable (Context c)
        {
            this->debugAccess(c);
            return m_state == State::AVAILABLE;
        }
        
        BlockIndexType getAvailableBlock (Context c)
        {
            this->debugAccess(c);
            AMBRO_ASSERT(isAvailable(c))
            AMBRO_ASSERT(get_entry(c)->isAssignedForBlock(c))
            
            return get_entry(c)->getBlock(c);
        }
        
        char const * getData (Context c, WrapBool<false> for_reading)
        {
            this->debugAccess(c);
            AMBRO_ASSERT(isAvailable(c))
            AMBRO_ASSERT(get_entry(c)->isAssignedForBlock(c))
            
            return get_entry(c)->getDataForReading(c);
        }
        
        APRINTER_FUNCTION_IF(Writable, char *, getData (Context c, WrapBool<true> for_writing))
        {
            this->debugAccess(c);
            AMBRO_ASSERT(isAvailable(c))
            AMBRO_ASSERT(get_entry(c)->isAssignedForBlock(c))
            
            return get_entry(c)->getDataForWriting(c);
        }
        
        APRINTER_FUNCTION_IF(Writable, void, markDirty (Context c))
        {
            this->debugAccess(c);
            AMBRO_ASSERT(isAvailable(c))
            AMBRO_ASSERT(get_entry(c)->isAssignedForBlock(c))
            
            get_entry(c)->markDirty(c);
        }
        
        char * getUserBuffer (Context c)
        {
            this->debugAccess(c);
            AMBRO_ASSERT(isAvailable(c))
            AMBRO_ASSERT(get_entry(c)->isAssignedForUser(c))
            
            return get_entry(c)->getDataForUser(c);
        }
        
    private:
        APRINTER_FUNCTION_IF_OR_EMPTY(Writable, void, check_write_params (BlockIndexType write_stride, uint8_t write_count))
        {
            AMBRO_ASSERT(write_stride == this->m_write_stride)
            AMBRO_ASSERT(write_count == this->m_write_count)
        }
        
        APRINTER_FUNCTION_IF_OR_EMPTY(Writable, void, set_write_params (BlockIndexType write_stride, uint8_t write_count, uint8_t flags))
        {
            this->m_write_stride = write_stride;
            this->m_write_count = write_count;
            this->m_no_need_to_read = (flags & FLAG_NO_NEED_TO_READ);
        }
        
        APRINTER_FUNCTION_IF_OR_EMPTY(Writable, void, start_allocation (Context c, BlockIndexType block, bool for_user))
        {
            auto *o = Object::self(c);
            m_state = State::ALLOCATING_ENTRY;
            this->m_allocating_block = block;
            this->m_allocating_for_user = for_user;
            o->pending_allocations.append(this);
            schedule_allocations_check(c);
        }
        
        CacheEntry * get_entry (Context c)
        {
            auto *o = Object::self(c);
            return &o->cache_entries[m_entry_index];
        }
        
        APRINTER_FUNCTION_IF_ELSE(Writable, bool, is_allocating_block (BlockIndexType block), {
            return (m_state == State::ALLOCATING_ENTRY && !this->m_allocating_for_user && this->m_allocating_block == block);
        }, {
            return false;
        })
        
        APRINTER_FUNCTION_IF_ELSE(Writable, bool, get_no_need_to_read (), {
            return this->m_no_need_to_read;
        }, {
            return false;
        })
        
        void reset_internal (Context c)
        {
            if (m_state == State::INVALID) {
                AMBRO_ASSERT(m_entry_index == -1)
                AMBRO_ASSERT(!m_event.isSet(c))
                return;
            }
            if (m_entry_index != -1) {
                get_entry(c)->detachUser(c, this);
                m_entry_index = -1;
            }
            if (Writable && m_state == State::ALLOCATING_ENTRY) {
                remove_from_pending_allocations(c);
            }
            m_event.unset(c);
            m_state = State::INVALID;
        }
        
        void request_common (Context c, BlockIndexType block, BlockIndexType write_stride, uint8_t write_count, uint8_t flags, bool for_user)
        {
            reset_internal(c);
            
            set_write_params(write_stride, write_count, flags);
            
            CacheEntryIndexType entry_index = get_entry_for_block(c, block, for_user);
            if (entry_index != -1) {
                attach_to_entry(c, entry_index, block, write_stride, write_count, for_user);
            } else {
                if (Writable) {
                    start_allocation(c, block, for_user);
                } else {
                    complete_init(c);
                }
            }
        }
        
        APRINTER_FUNCTION_IF_OR_EMPTY(Writable, void, remove_from_pending_allocations (Context c))
        {
            auto *o = Object::self(c);
            o->pending_allocations.remove(this);
        }
        
        void complete_init (Context c)
        {
            m_state = State::INIT_COMPL_EVENT;
            m_event.prependNowNotAlready(c);
        }
        
        void attach_to_entry (Context c, CacheEntryIndexType entry_index, BlockIndexType block, BlockIndexType write_stride, uint8_t write_count, bool for_user)
        {
            m_entry_index = entry_index;
            if (for_user) {
                get_entry(c)->assignToUserBlock(c, this);
                complete_init(c);
            } else {
                get_entry(c)->assignBlockAndAttachUser(c, block, write_stride, write_count, get_no_need_to_read(), this);
                if (!get_entry(c)->isInitialized(c)) {
                    m_state = State::WAITING_READ;
                } else {
                    complete_init(c);
                }
            }
        }
        
        void event_handler (Context c)
        {
            this->debugAccess(c);
            AMBRO_ASSERT(m_state == State::INIT_COMPL_EVENT)
            
            bool error = (m_entry_index == -1);
            m_state = error ? State::INVALID : State::AVAILABLE;
            return m_handler(c, error);
        }
        
        APRINTER_FUNCTION_IF(Writable, void, allocation_event (Context c, bool error))
        {
            auto *o = Object::self(c);
            this->debugAccess(c);
            AMBRO_ASSERT(m_state == State::ALLOCATING_ENTRY)
            AMBRO_ASSERT(m_entry_index == -1)
            
            if (error) {
                o->pending_allocations.remove(this);
                return complete_init(c);
            }
            
            CacheEntryIndexType entry_index = get_entry_for_block(c, this->m_allocating_block, this->m_allocating_for_user);
            if (entry_index != -1) {
                o->pending_allocations.remove(this);
                attach_to_entry(c, entry_index, this->m_allocating_block, this->m_write_stride, this->m_write_count, this->m_allocating_for_user);
            }
        }
        
        void cache_read_completed (Context c, bool error)
        {
            this->debugAccess(c);
            AMBRO_ASSERT(m_entry_index != -1)
            AMBRO_ASSERT(m_state == State::WAITING_READ)
            
            if (error) {
                get_entry(c)->detachUser(c, this);
                m_entry_index = -1;
            }
            complete_init(c);
        }
        
        CacheHandler m_handler;
        typename Context::EventLoop::QueuedEvent m_event;
        DoubleEndedListNode<CacheRef> m_list_node;
        CacheEntryIndexType m_entry_index;
        State m_state;
    };
    
private:
    APRINTER_FUNCTION_IF_OR_EMPTY_EXT(Writable, static, void, writable_init (Context c))
    {
        auto *o = Object::self(c);
        o->allocations_event.init(c, APRINTER_CB_STATFUNC_T(&BlockCache::allocations_event_handler<>));
        o->current_dirt_time = 0;
        o->waiting_flush_requests.init();
        o->pending_allocations.init();
        for (BufferIndexType i = 0; i < NumBuffers; i++) {
            o->buffer_usage[i] = false;
        }
    }
    
    APRINTER_FUNCTION_IF_OR_EMPTY_EXT(Writable, static, void, writable_deinit_assert (Context c))
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->pending_allocations.isEmpty())
        AMBRO_ASSERT(o->waiting_flush_requests.isEmpty())
    }
    
    APRINTER_FUNCTION_IF_OR_EMPTY_EXT(Writable, static, void, writable_deinit (Context c))
    {
        auto *o = Object::self(c);
        o->allocations_event.deinit(c);
    }
    
    APRINTER_FUNCTION_IF_EXT(Writable, static, bool, is_flush_completed (Context c, bool for_new_request, bool *out_error))
    {
        auto *o = Object::self(c);
        
        bool error = false;
        for (CacheEntryIndexType i = 0; i < NumCacheEntries; i++) {
            CacheEntry *ce = &o->cache_entries[i];
            if (ce->isDirty(c)) {
                if (!for_new_request && ce->hasLastWriteFailed(c)) {
                    error = true;
                } else {
                    AMBRO_ASSERT(for_new_request || ce->isWriteScheduledOrActive(c))
                    return false;
                }
            }
        }
        
        if (out_error) {
            *out_error = error;
        }
        return true;
    }
    
    APRINTER_FUNCTION_IF_EXT(Writable, static, void, start_writing_for_flush (Context c))
    {
        auto *o = Object::self(c);
        for (CacheEntryIndexType i = 0; i < NumCacheEntries; i++) {
            CacheEntry *ce = &o->cache_entries[i];
            if (ce->canStartWrite(c)) {
                ce->scheduleWriting(c);
            }
        }
    }
    
    APRINTER_FUNCTION_IF_EXT(Writable, static, void, complete_flush (Context c, bool error))
    {
        auto *o = Object::self(c);
        for (FlushRequest<> *req = o->waiting_flush_requests.first(); req; req = o->waiting_flush_requests.next(req)) {
            req->flush_request_result(c, error);
        }
        o->waiting_flush_requests.init();
    }
    
    static CacheEntryIndexType get_entry_for_block (Context c, BlockIndexType block, bool for_user)
    {
        auto *o = Object::self(c);
        
        CacheEntryIndexType invalid_entry = -1;
        CacheEntryIndexType recyclable_entry = -1;
        
        for (CacheEntryIndexType entry_index = 0; entry_index < NumCacheEntries; entry_index++) {
            CacheEntry *ce = &o->cache_entries[entry_index];
            
            if (!for_user && ce->isAssignedForBlock(c) && ce->getBlock(c) == block) {
                return ce->isBeingReleased(c) ? -1 : entry_index;
            }
            
            if (!ce->isBeingReleased(c)) {
                if (!ce->isAssigned(c)) {
                    invalid_entry = entry_index;
                } else if (ce->canReassign(c)) {
                    recyclable_entry = entry_index;
                }
            }
        }
        
        return (invalid_entry != -1) ? invalid_entry : recyclable_entry;
    }
    
    APRINTER_FUNCTION_IF_EXT(Writable, static, void, report_allocation_event (Context c, bool error))
    {
        auto *o = Object::self(c);
        CacheRef *ref = o->pending_allocations.first();
        while (ref) {
            CacheRef *next = o->pending_allocations.next(ref);
            ref->allocation_event(c, error);
            ref = next;
        }
        AMBRO_ASSERT(!error || o->pending_allocations.isEmpty())
    }
    
    APRINTER_FUNCTION_IF_OR_EMPTY_EXT(Writable, static, void, schedule_allocations_check (Context c))
    {
        auto *o = Object::self(c);
        if (!o->allocations_event.isSet(c)) {
            o->allocations_event.prependNowNotAlready(c);
        }
    }
    
    APRINTER_FUNCTION_IF_EXT(Writable, static, void, allocations_event_handler (Context c))
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        for (CacheEntryIndexType i = 0; i < NumCacheEntries; i++) {
            CacheEntry *ce = &o->cache_entries[i];
            if (ce->isBeingReleased(c) && !ce->isAssigned(c)) {
                ce->completeRelease(c);
            }
        }
        
        report_allocation_event(c, false);
        
        if (!o->pending_allocations.isEmpty()) {
            bool could_assure_release = assure_release_in_progress(c);
            if (!could_assure_release) {
                report_allocation_event(c, true);
            }
        }
    }
    
    APRINTER_FUNCTION_IF_EXT(Writable, static, bool, assure_release_in_progress (Context c))
    {
        auto *o = Object::self(c);
        
        CacheEntry *release_entry = nullptr;
        for (CacheEntryIndexType i = 0; i < NumCacheEntries; i++) {
            CacheEntry *ce = &o->cache_entries[i];
            if (ce->isBeingReleased(c)) {
                AMBRO_ASSERT(ce->isAssignedForBlock(c))
                return true;
            }
            if (ce->canStartRelease(c)) {
                if (!release_entry || !ce->isDirty(c) || (release_entry->isDirty(c) && dirt_times_less(ce->getDirtTime(c), release_entry->getDirtTime(c)))) {
                    release_entry = ce;
                }
            }
        }
        if (!release_entry) {
            return false;
        }
        release_entry->startRelease(c);
        return true;
    }
    
    static bool dirt_times_less (DirtTimeType t1, DirtTimeType t2)
    {
        return ((DirtTimeType)(t1 - t2) >= DirtSignBit);
    }
    
    APRINTER_FUNCTION_IF_EXT(Writable, static, BufferIndexType, find_free_buffer (Context c))
    {
        auto *o = Object::self(c);
        for (BufferIndexType i = 0; i < NumBuffers; i++) {
            if (!o->buffer_usage[i]) {
                return i;
            }
        }
        return -1;
    }
    
    APRINTER_FUNCTION_IF_EXT(Writable, static, void, assert_used_buffer (Context c, BufferIndexType i))
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(i >= 0)
        AMBRO_ASSERT(i < NumBuffers)
        AMBRO_ASSERT(o->buffer_usage[i])
    }
    
    enum class DirtState : uint8_t {CLEAN, DIRTY, WRITING};
    
    APRINTER_STRUCT_IF_TEMPLATE(CacheEntryWritableMemebers) {
        typename Context::EventLoop::QueuedEvent m_write_event;
        bool m_releasing;
        bool m_last_write_failed;
        DirtState m_dirt_state;
        uint8_t m_write_count;
        uint8_t m_write_index;
        DirtTimeType m_dirt_time;
        BlockIndexType m_write_stride;
        BufferIndexType m_active_buffer;
        BufferIndexType m_writing_buffer;
    };
    
    class CacheEntry : private CacheEntryWritableMemebers<Writable> {
        enum class State : uint8_t {INVALID, READING, IDLE, WRITING, USER};
        
    public:
        void init (Context c)
        {
            m_block_user.init(c, APRINTER_CB_OBJFUNC_T(&CacheEntry::block_user_handler, this));
            m_cache_users_list.init();
            m_state = State::INVALID;
            writable_entry_init(c);
        }
        
        void deinit (Context c)
        {
            AMBRO_ASSERT(m_cache_users_list.isEmpty())
            
            writable_entry_deinit(c);
            m_block_user.deinit(c);
        }
        
        bool isAssigned (Context c)
        {
            return m_state != State::INVALID;
        }
        
        bool isAssignedForBlock (Context c)
        {
            return m_state != State::INVALID && m_state != State::USER;
        }
        
        bool isAssignedForUser (Context c)
        {
            return m_state == State::USER;
        }
        
        bool isInitialized (Context c)
        {
            return m_state == State::IDLE || m_state == State::WRITING;
        }
        
        bool canReassign (Context c)
        {
            return m_state == State::IDLE && get_dirt_state() == DirtState::CLEAN && m_cache_users_list.isEmpty();
        }
        
        APRINTER_FUNCTION_IF(Writable, bool, isDirty (Context c))
        {
            return isInitialized(c) && this->m_dirt_state != DirtState::CLEAN;
        }
        
        APRINTER_FUNCTION_IF(Writable, bool, canStartWrite (Context c))
        {
            return m_state == State::IDLE && this->m_dirt_state != DirtState::CLEAN;
        }
        
        bool isReferenced (Context c)
        {
            return !m_cache_users_list.isEmpty();
        }
        
        APRINTER_FUNCTION_IF_ELSE(Writable, bool, isBeingReleased (Context c), {
            return this->m_releasing;
        }, {
            return false;
        })
        
        APRINTER_FUNCTION_IF(Writable, bool, canStartRelease (Context c))
        {
            AMBRO_ASSERT(!isBeingReleased(c))
            return m_cache_users_list.isEmpty() && m_state != State::INVALID && (m_state != State::IDLE || this->m_dirt_state != DirtState::CLEAN);
        }
        
        APRINTER_FUNCTION_IF(Writable, bool, hasLastWriteFailed (Context c))
        {
            AMBRO_ASSERT(isAssignedForBlock(c))
            return this->m_last_write_failed;
        }
        
        APRINTER_FUNCTION_IF(Writable, bool, isWriteScheduledOrActive (Context c))
        {
            return (this->m_write_event.isSet(c) || m_state == State::WRITING);
        }
        
        BlockIndexType getBlock (Context c)
        {
            AMBRO_ASSERT(isAssignedForBlock(c))
            return m_block;
        }
        
        char const * getDataForReading (Context c)
        {
            AMBRO_ASSERT(isInitialized(c))
            return get_buffer(c);
        }
        
        APRINTER_FUNCTION_IF(Writable, char *, getDataForWriting (Context c))
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(isInitialized(c))
            
            if (this->m_active_buffer == this->m_writing_buffer) {
                AMBRO_ASSERT(m_state == State::WRITING)
                BufferIndexType new_buffer = find_free_buffer(c);
                AMBRO_ASSERT(new_buffer != -1)
                o->buffer_usage[new_buffer] = true;
                memcpy(o->buffers[new_buffer], get_buffer(c), BlockSize);
                this->m_active_buffer = new_buffer;
            }
            
            return get_buffer(c);
        }
        
        char * getDataForUser (Context c)
        {
            AMBRO_ASSERT(isAssignedForUser(c))
            return get_buffer(c);
        }
        
        APRINTER_FUNCTION_IF(Writable, DirtTimeType, getDirtTime (Context c))
        {
            AMBRO_ASSERT(isDirty(c))
            return this->m_dirt_time;
        }
        
        void assignBlockAndAttachUser (Context c, BlockIndexType block, BlockIndexType write_stride, uint8_t write_count, bool no_need_to_read, CacheRef *user)
        {
            AMBRO_ASSERT(write_count >= 1)
            AMBRO_ASSERT(!isBeingReleased(c))
            
            if (isAssignedForBlock(c) && block == m_block) {
                check_write_params(write_stride, write_count);
                m_cache_users_list.prepend(user);
                return;
            }
            
            AMBRO_ASSERT(m_cache_users_list.isEmpty())
            AMBRO_ASSERT(m_state == State::INVALID || m_state == State::IDLE)
            AMBRO_ASSERT(m_state == State::INVALID || get_dirt_state() == DirtState::CLEAN)
            
            m_block = block;
            writable_assign(c, write_stride, write_count);
            
            m_cache_users_list.prepend(user);
            
            if (Writable && no_need_to_read) {
                m_state = State::IDLE;
                memset(get_buffer(c), 0, BlockSize);
                // No implicit markDirty.
            } else {
                m_state = State::READING;
                m_block_user.startRead(c, m_block, WrapBuffer::Make(get_buffer(c)));
            }
        }
        
        void assignToUserBlock (Context c, CacheRef *user)
        {
            AMBRO_ASSERT(!isBeingReleased(c))
            AMBRO_ASSERT(m_cache_users_list.isEmpty())
            AMBRO_ASSERT(m_state == State::INVALID || m_state == State::IDLE)
            AMBRO_ASSERT(m_state == State::INVALID || get_dirt_state() == DirtState::CLEAN)
            
            m_state = State::USER;
            writable_assign(c, 0, 0);
            m_cache_users_list.prepend(user);
        }
        
        void detachUser (Context c, CacheRef *user)
        {
            m_cache_users_list.remove(user);
            if (m_state == State::USER) {
                AMBRO_ASSERT(m_cache_users_list.isEmpty())
                m_state = State::INVALID;
            }
        }
        
        APRINTER_FUNCTION_IF(Writable, void, markDirty (Context c))
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(isInitialized(c))
            AMBRO_ASSERT(isReferenced(c))
            AMBRO_ASSERT(!isBeingReleased(c))
            
            if (this->m_dirt_state != DirtState::DIRTY) {
                this->m_dirt_state = DirtState::DIRTY;
                this->m_dirt_time = o->current_dirt_time++;
            }
            
            if (!o->waiting_flush_requests.isEmpty() && m_state == State::IDLE) {
                scheduleWriting(c);
            }
        }
        
        APRINTER_FUNCTION_IF(Writable, void, scheduleWriting (Context c))
        {
            AMBRO_ASSERT(m_state == State::IDLE)
            AMBRO_ASSERT(this->m_dirt_state == DirtState::DIRTY)
            
            if (!this->m_write_event.isSet(c)) {
                this->m_write_event.prependNowNotAlready(c);
            }
        }
        
        APRINTER_FUNCTION_IF(Writable, void, startWriting (Context c))
        {
            AMBRO_ASSERT(m_state == State::IDLE)
            AMBRO_ASSERT(this->m_dirt_state == DirtState::DIRTY)
            AMBRO_ASSERT(this->m_writing_buffer == -1)
            
            m_state = State::WRITING;
            this->m_last_write_failed = false;
            this->m_dirt_state = DirtState::WRITING;
            this->m_write_index = 1;
            this->m_write_event.unset(c);
            
            m_block_user.startWrite(c, m_block, WrapBuffer::Make(get_buffer(c)));
        }
        
        APRINTER_FUNCTION_IF(Writable, void, startRelease (Context c))
        {
            AMBRO_ASSERT(isAssignedForBlock(c))
            AMBRO_ASSERT(!canReassign(c))
            AMBRO_ASSERT(!isReferenced(c))
            AMBRO_ASSERT(!isBeingReleased(c))
            
            this->m_releasing = true;
            if (m_state == State::IDLE) {
                scheduleWriting(c);
            }
        }
        
        APRINTER_FUNCTION_IF(Writable, void, completeRelease (Context c))
        {
            AMBRO_ASSERT(this->m_releasing)
            this->m_releasing = false;
        }
        
    private:
        CacheEntryIndexType get_entry_index (Context c)
        {
            auto *o = Object::self(c);
            return (this - o->cache_entries);
        }
        
        APRINTER_FUNCTION_IF_OR_EMPTY(Writable, void, writable_entry_init (Context c))
        {
            auto *o = Object::self(c);
            
            m_block_user.setLocker(c, APRINTER_CB_OBJFUNC_T(&CacheEntry::block_user_locker<>, this));
            this->m_write_event.init(c, APRINTER_CB_OBJFUNC_T(&CacheEntry::write_event_handler<>, this));
            this->m_releasing = false;
            this->m_active_buffer = get_entry_index(c);
            o->buffer_usage[this->m_active_buffer] = true;
            this->m_writing_buffer = -1;
        }
        
        APRINTER_FUNCTION_IF_OR_EMPTY(Writable, void, writable_entry_deinit (Context c))
        {
            this->m_write_event.deinit(c);
        }
        
        APRINTER_FUNCTION_IF_ELSE(Writable, DirtState, get_dirt_state (), {
            return this->m_dirt_state;
        }, {
            return DirtState::CLEAN;
        })
        
        APRINTER_FUNCTION_IF_OR_EMPTY(Writable, void, check_write_params (BlockIndexType write_stride, uint8_t write_count))
        {
            AMBRO_ASSERT(write_stride == this->m_write_stride)
            AMBRO_ASSERT(write_count == this->m_write_count)
        }
        
        APRINTER_FUNCTION_IF_OR_EMPTY(Writable, void, writable_assign (Context c, BlockIndexType write_stride, uint8_t write_count))
        {
            AMBRO_ASSERT(!this->m_write_event.isSet(c))
            AMBRO_ASSERT(!this->m_releasing)
            AMBRO_ASSERT(this->m_writing_buffer == -1)
            
            this->m_last_write_failed = false;
            this->m_dirt_state = DirtState::CLEAN;
            this->m_write_stride = write_stride;
            this->m_write_count = write_count;
        }
        
        APRINTER_FUNCTION_IF_ELSE(Writable, char *, get_buffer (Context c), {
            auto *o = Object::self(c);
            return o->buffers[this->m_active_buffer];
        }, {
            auto *o = Object::self(c);
            return o->buffers[get_entry_index(c)];
        })
        
        void raise_read_completed (Context c, bool error)
        {
            CacheRef *ref = m_cache_users_list.first();
            while (ref) {
                CacheRef *next = m_cache_users_list.next(ref);
                ref->cache_read_completed(c, error);
                ref = next;
            }
        }
        
        void block_user_handler (Context c, bool error)
        {
            auto *o = Object::self(c);
            TheDebugObject::access(c);
            AMBRO_ASSERT(!isBeingReleased(c) || !isReferenced(c))
            
            if (m_state == State::READING) {
                if (isBeingReleased(c)) {
                    m_state = State::INVALID;
                    return schedule_allocations_check(c);
                }
                m_state = error ? State::INVALID : State::IDLE;
                raise_read_completed(c, error);
                AMBRO_ASSERT(!error || !isReferenced(c))
            }
            else if (Writable && m_state == State::WRITING) {
                block_write_completed(c, error);
            }
            else {
                AMBRO_ASSERT(false)
            }
        }
        
        APRINTER_FUNCTION_IF(Writable, void, block_user_locker (Context c, bool lock_else_unlock))
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(m_state == State::WRITING)
            
            if (lock_else_unlock) {
                assert_used_buffer(c, this->m_active_buffer);
                AMBRO_ASSERT(this->m_writing_buffer == -1)
                this->m_writing_buffer = this->m_active_buffer;
            } else {
                assert_used_buffer(c, this->m_active_buffer);
                assert_used_buffer(c, this->m_writing_buffer);
                if (this->m_active_buffer != this->m_writing_buffer) {
                    o->buffer_usage[this->m_writing_buffer] = false;
                }
                this->m_writing_buffer = -1;
            }
        }
        
        APRINTER_FUNCTION_IF(Writable, void, write_event_handler (Context c))
        {
            startWriting(c);
        }
        
        APRINTER_FUNCTION_IF_OR_EMPTY(Writable, void, block_write_completed (Context c, bool error))
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(this->m_dirt_state != DirtState::CLEAN)
            AMBRO_ASSERT(!this->m_last_write_failed)
            AMBRO_ASSERT(this->m_write_index <= this->m_write_count)
            AMBRO_ASSERT(this->m_writing_buffer == -1)
            
            if (!error && this->m_write_index < this->m_write_count) {
                BlockIndexType write_block = m_block + this->m_write_index * this->m_write_stride;
                this->m_write_index++;
                return m_block_user.startWrite(c, write_block, WrapBuffer::Make(get_buffer(c)));
            }
            
            m_state = State::IDLE;
            this->m_last_write_failed = error;
            this->m_dirt_state = (!error && this->m_dirt_state == DirtState::WRITING) ? DirtState::CLEAN : DirtState::DIRTY;
            
            if (!error && this->m_dirt_state == DirtState::DIRTY && (!o->waiting_flush_requests.isEmpty() || this->m_releasing)) {
                return startWriting(c);
            }
            
            if (this->m_releasing) {
                if (error) {
                    completeRelease(c);
                    report_allocation_event(c, true);
                } else {
                    AMBRO_ASSERT(this->m_dirt_state == DirtState::CLEAN)
                    m_state = State::INVALID;
                    schedule_allocations_check(c);
                }
            }
            
            if (!o->waiting_flush_requests.isEmpty()) {
                bool flush_error;
                if (is_flush_completed(c, false, &flush_error)) {
                    complete_flush(c, flush_error);
                }
            }
        }
        
        BlockAccessUser m_block_user;
        DoubleEndedList<CacheRef, &CacheRef::m_list_node, false> m_cache_users_list;
        BlockIndexType m_block;
        State m_state;
    };
    
    APRINTER_STRUCT_IF_TEMPLATE(CacheWritableMembers) {
        typename Context::EventLoop::QueuedEvent allocations_event;
        DirtTimeType current_dirt_time;
        DoubleEndedList<FlushRequest<>, &FlushRequest<>::m_waiting_flush_requests_node, false> waiting_flush_requests;
        DoubleEndedList<CacheRef, &CacheRef::m_list_node> pending_allocations;
        bool buffer_usage[NumBuffers];
    };
    
public:
    struct Object : public ObjBase<BlockCache, ParentObject, MakeTypeList<
        TheDebugObject
    >>, public CacheWritableMembers<Writable> {
        CacheEntry cache_entries[NumCacheEntries];
        char buffers[NumBuffers][BlockSize];
    };
};

#include <aprinter/EndNamespace.h>

#endif
