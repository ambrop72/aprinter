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

#include <aprinter/meta/ChooseInt.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/Callback.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/structure/DoubleEndedList.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, typename TTheBlockAccess, int TNumCacheEntries>
class BlockCache {
public:
    struct Object;
    using TheBlockAccess = TTheBlockAccess;
    static int const NumCacheEntries = TNumCacheEntries;
    
private:
    class CacheEntry;
    using TheDebugObject = DebugObject<Context, Object>;
    using BlockAccessUser = typename TheBlockAccess::User;
    using DirtTimeType = uint32_t;
    static constexpr DirtTimeType DirtSignBit = ((DirtTimeType)-1 / 2) + 1;
    using CacheEntryIndexType = ChooseIntForMax<NumCacheEntries, true>;
    
    enum class CacheEntryEvent : uint8_t {READ_COMPLETED};
    
public:
    using BlockIndexType = typename TheBlockAccess::BlockIndexType;
    using BlockRange = typename TheBlockAccess::BlockRange;
    static size_t const BlockSize = TheBlockAccess::BlockSize;
    
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        o->allocations_event.init(c, APRINTER_CB_STATFUNC_T(&BlockCache::allocations_event_handler));
        o->current_dirt_time = 0;
        o->waiting_flush_requests.init();
        o->pending_allocations.init();
        for (int i = 0; i < NumCacheEntries; i++) {
            o->cache_entries[i].init(c);
        }
        
        TheDebugObject::init(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::deinit(c);
        AMBRO_ASSERT(o->pending_allocations.isEmpty())
        AMBRO_ASSERT(o->waiting_flush_requests.isEmpty())
        
        for (int i = 0; i < NumCacheEntries; i++) {
            o->cache_entries[i].deinit(c);
        }
        o->allocations_event.deinit(c);
    }
    
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
            auto *o = Object::self(c);
            this->debugAccess(c);
            AMBRO_ASSERT(m_state == State::IDLE)
            
            if (is_flush_completed(c, false, nullptr)) {
                return complete(c, false);
            }
            o->waiting_flush_requests.add(this);
            m_state = State::WAITING;
            start_writing_for_flush(c);
        }
        
    private:
        void reset_internal (Context c)
        {
            auto *o = Object::self(c);
            if (m_state == State::WAITING) {
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
            auto *o = Object::self(c);
            this->debugAccess(c);
            AMBRO_ASSERT(m_state == State::WAITING)
            
            o->waiting_flush_requests.remove(this);
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
    
    class CacheRef : private SimpleDebugObject<Context> {
        friend BlockCache;
        friend class CacheEntry;
        
        enum class State : uint8_t {INVALID, ALLOCATING_ENTRY, WAITING_READ, INIT_COMPL_EVENT, AVAILABLE};
        
    public:
        using CacheHandler = Callback<void(Context c, bool error)>;
        
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
        
        bool requestBlock (Context c, BlockIndexType block, BlockIndexType write_stride, uint8_t write_count, bool disable_immediate_completion=false)
        {
            auto *o = Object::self(c);
            this->debugAccess(c);
            
            if (!disable_immediate_completion && m_state != State::INVALID && block == m_block) {
                AMBRO_ASSERT(write_stride == m_write_stride)
                AMBRO_ASSERT(write_count == m_write_count)
            } else {
                if (m_state != State::INVALID) {
                    reset_internal(c);
                }
                
                AMBRO_ASSERT(m_entry_index == -1)
                m_block = block;
                m_write_stride = write_stride;
                m_write_count = write_count;
                
                CacheEntryIndexType entry_index = get_entry_for_block(c, m_block);
                if (entry_index != -1) {
                    m_entry_index = entry_index;
                    get_entry(c)->assignBlockAndAttachUser(c, m_block, m_write_stride, m_write_count, this);
                    if (!get_entry(c)->isInitialized(c)) {
                        m_state = State::WAITING_READ;
                    } else {
                        if (disable_immediate_completion) {
                            complete_init(c);
                        } else {
                            m_state = State::AVAILABLE;
                        }
                    }
                } else {
                    m_state = State::ALLOCATING_ENTRY;
                    o->pending_allocations.append(this);
                    
                    schedule_allocations_check(c);
                }
            }
            
            return m_state == State::AVAILABLE;
        }
        
        bool isAvailable (Context c)
        {
            this->debugAccess(c);
            return m_state == State::AVAILABLE;
        }
        
        char * getData (Context c)
        {
            this->debugAccess(c);
            AMBRO_ASSERT(isAvailable(c))
            
            return get_entry(c)->getData(c);
        }
        
        void markDirty (Context c)
        {
            this->debugAccess(c);
            AMBRO_ASSERT(isAvailable(c))
            
            get_entry(c)->markDirty(c);
        }
        
    private:
        CacheEntry * get_entry (Context c)
        {
            auto *o = Object::self(c);
            return &o->cache_entries[m_entry_index];
        }
        
        void reset_internal (Context c)
        {
            auto *o = Object::self(c);
            if (m_entry_index != -1) {
                get_entry(c)->detachUser(c, this);
                m_entry_index = -1;
            }
            if (m_state == State::ALLOCATING_ENTRY) {
                o->pending_allocations.remove(this);
            }
            m_event.unset(c);
            m_state = State::INVALID;
        }
        
        void complete_init (Context c)
        {
            m_state = State::INIT_COMPL_EVENT;
            m_event.prependNowNotAlready(c);
        }
        
        void event_handler (Context c)
        {
            this->debugAccess(c);
            AMBRO_ASSERT(m_state == State::INIT_COMPL_EVENT)
            
            bool error = (m_entry_index == -1);
            if (error) {
                m_state = State::INVALID;
            } else {
                m_state = State::AVAILABLE;
            }
            return m_handler(c, error);
        }
        
        void allocation_event (Context c, bool error)
        {
            auto *o = Object::self(c);
            this->debugAccess(c);
            AMBRO_ASSERT(m_state == State::ALLOCATING_ENTRY)
            AMBRO_ASSERT(m_entry_index == -1)
            
            if (error) {
                o->pending_allocations.remove(this);
                return complete_init(c);
            }
            
            CacheEntryIndexType entry_index = get_entry_for_block(c, m_block);
            if (entry_index != -1) {
                o->pending_allocations.remove(this);
                m_entry_index = entry_index;
                get_entry(c)->assignBlockAndAttachUser(c, m_block, m_write_stride, m_write_count, this);
                if (!get_entry(c)->isInitialized(c)) {
                    m_state = State::WAITING_READ;
                    return;
                }
                complete_init(c);
            }
        }
        
        void cache_event (Context c, CacheEntryEvent event, bool error)
        {
            this->debugAccess(c);
            AMBRO_ASSERT(m_entry_index != -1)
            AMBRO_ASSERT(event == CacheEntryEvent::READ_COMPLETED)
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
        BlockIndexType m_block;
        BlockIndexType m_write_stride;
        CacheEntryIndexType m_entry_index;
        State m_state;
        uint8_t m_write_count;
    };
    
private:
    static bool is_flush_completed (Context c, bool accept_errors, bool *out_error)
    {
        auto *o = Object::self(c);
        bool error = false;
        for (int i = 0; i < NumCacheEntries; i++) {
            CacheEntry *ce = &o->cache_entries[i];
            if (ce->isDirty(c)) {
                if (accept_errors && ce->hasLastWriteFailed(c)) {
                    error = true;
                } else {
                    return false;
                }
            }
        }
        if (out_error) {
            *out_error = error;
        }
        return true;
    }
    
    static void start_writing_for_flush (Context c)
    {
        auto *o = Object::self(c);
        for (int i = 0; i < NumCacheEntries; i++) {
            CacheEntry *ce = &o->cache_entries[i];
            if (ce->canStartWrite(c)) {
                ce->startWriting(c);
            }
        }
    }
    
    static void complete_flush (Context c, bool error)
    {
        auto *o = Object::self(c);
        FlushRequest *req = o->waiting_flush_requests.first();
        while (req) {
            FlushRequest *next = o->waiting_flush_requests.next(req);
            req->flush_request_result(c, error);
            req = next;
        }
        AMBRO_ASSERT(o->waiting_flush_requests.isEmpty())
    }
    
    static CacheEntryIndexType get_entry_for_block (Context c, BlockIndexType block)
    {
        auto *o = Object::self(c);
        
        CacheEntryIndexType invalid_entry = -1;
        CacheEntryIndexType recyclable_entry = -1;
        
        for (CacheEntryIndexType entry_index = 0; entry_index < NumCacheEntries; entry_index++) {
            CacheEntry *ce = &o->cache_entries[entry_index];
            
            if (ce->isAssigned(c) && ce->getBlock(c) == block) {
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
    
    static void report_allocation_event (Context c, bool error)
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
    
    static void schedule_allocations_check (Context c)
    {
        auto *o = Object::self(c);
        if (!o->allocations_event.isSet(c)) {
            o->allocations_event.prependNowNotAlready(c);
        }
    }
    
    static void allocations_event_handler (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        for (int i = 0; i < NumCacheEntries; i++) {
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
    
    static bool assure_release_in_progress (Context c)
    {
        auto *o = Object::self(c);
        
        CacheEntry *release_entry = nullptr;
        
        for (int i = 0; i < NumCacheEntries; i++) {
            CacheEntry *ce = &o->cache_entries[i];
            if (ce->isBeingReleased(c)) {
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
    
    static void entry_release_result (Context c, CacheEntry *entry, bool error)
    {
        AMBRO_ASSERT(entry->isBeingReleased(c))
        AMBRO_ASSERT(entry->isAssigned(c) == error)
        
        if (error) {
            entry->completeRelease(c);
            report_allocation_event(c, true);
        } else {
            schedule_allocations_check(c);
        }
    }
    
    static bool dirt_times_less (DirtTimeType t1, DirtTimeType t2)
    {
        return ((DirtTimeType)(t1 - t2) >= DirtSignBit);
    }
    
    class CacheEntry {
        enum class State : uint8_t {INVALID, READING, IDLE, WRITING};
        enum class DirtState : uint8_t {CLEAN, DIRTY, WRITING};
        
    public:
        void init (Context c)
        {
            m_block_user.init(c, APRINTER_CB_OBJFUNC_T(&CacheEntry::block_user_handler, this));
            m_cache_users_list.init();
            m_state = State::INVALID;
            m_releasing = false;
            m_last_write_failed = false;
        }
        
        void deinit (Context c)
        {
            AMBRO_ASSERT(m_cache_users_list.isEmpty())
            
            m_block_user.deinit(c);
        }
        
        bool isAssigned (Context c)
        {
            return m_state != State::INVALID;
        }
        
        bool isInitialized (Context c)
        {
            return m_state != State::INVALID && m_state != State::READING;
        }
        
        bool canReassign (Context c)
        {
            return m_state == State::IDLE && m_dirt_state == DirtState::CLEAN && m_cache_users_list.isEmpty();
        }
        
        bool isDirty (Context c)
        {
            return isInitialized(c) && m_dirt_state != DirtState::CLEAN;
        }
        
        bool canStartWrite (Context c)
        {
            return m_state == State::IDLE && m_dirt_state != DirtState::CLEAN;
        }
        
        bool isReferenced (Context c)
        {
            return !m_cache_users_list.isEmpty();
        }
        
        bool isBeingReleased (Context c)
        {
            return m_releasing;
        }
        
        bool canStartRelease (Context c)
        {
            return m_state != State::INVALID && (m_state != State::IDLE || m_dirt_state != DirtState::CLEAN) &&
                   m_cache_users_list.isEmpty() && !m_releasing;
        }
        
        bool hasLastWriteFailed (Context c)
        {
            return m_last_write_failed;
        }
        
        BlockIndexType getBlock (Context c)
        {
            AMBRO_ASSERT(isAssigned(c))
            
            return m_block;
        }
        
        char * getData (Context c)
        {
            AMBRO_ASSERT(isInitialized(c))
            
            return m_buffer;
        }
        
        DirtTimeType getDirtTime (Context c)
        {
            AMBRO_ASSERT(isDirty(c))
            
            return m_dirt_time;
        }
        
        void assignBlockAndAttachUser (Context c, BlockIndexType block, BlockIndexType write_stride, uint8_t write_count, CacheRef *user)
        {
            AMBRO_ASSERT(write_count >= 1)
            AMBRO_ASSERT(!isBeingReleased(c))
            
            if (m_state != State::INVALID && block == m_block) {
                AMBRO_ASSERT(write_stride == m_write_stride)
                AMBRO_ASSERT(write_count == m_write_count)
            } else {
                AMBRO_ASSERT(m_cache_users_list.isEmpty())
                AMBRO_ASSERT(m_state == State::INVALID || m_state == State::IDLE)
                AMBRO_ASSERT(m_state == State::INVALID || m_dirt_state == DirtState::CLEAN)
                
                m_state = State::READING;
                m_block = block;
                m_write_stride = write_stride;
                m_write_count = write_count;
                m_block_user.startRead(c, m_block, WrapBuffer::Make(m_buffer));
            }
            
            m_cache_users_list.append(user);
        }
        
        void detachUser (Context c, CacheRef *user)
        {
            m_cache_users_list.remove(user);
        }
        
        void markDirty (Context c)
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(isInitialized(c))
            AMBRO_ASSERT(isReferenced(c))
            AMBRO_ASSERT(!isBeingReleased(c))
            
            if (m_dirt_state != DirtState::DIRTY) {
                m_dirt_state = DirtState::DIRTY;
                m_dirt_time = o->current_dirt_time++;
            }
            
            if (!o->waiting_flush_requests.isEmpty() && m_state == State::IDLE) {
                startWriting(c);
            }
        }
        
        void startWriting (Context c)
        {
            AMBRO_ASSERT(m_state == State::IDLE)
            AMBRO_ASSERT(m_dirt_state == DirtState::DIRTY)
            
            m_state = State::WRITING;
            m_last_write_failed = false;
            m_dirt_state = DirtState::WRITING;
            m_write_index = 1;
            
            m_block_user.startWrite(c, m_block, WrapBuffer::Make(m_buffer));
            
            // TBD: Figure out the problem with modifying the block while it's being written out.
        }
        
        void startRelease (Context c)
        {
            AMBRO_ASSERT(isAssigned(c))
            AMBRO_ASSERT(!canReassign(c))
            AMBRO_ASSERT(!isReferenced(c))
            AMBRO_ASSERT(!isBeingReleased(c))
            
            m_releasing = true;
            
            if (m_state == State::IDLE) {
                startWriting(c);
            }
        }
        
        void completeRelease (Context c)
        {
            AMBRO_ASSERT(m_releasing)
            
            m_releasing = false;
        }
        
    private:
        void raise_cache_event (Context c, CacheEntryEvent event, bool error)
        {
            CacheRef *ref = m_cache_users_list.first();
            while (ref) {
                CacheRef *next = m_cache_users_list.next(ref);
                ref->cache_event(c, event, error);
                ref = next;
            }
        }
        
        void block_user_handler (Context c, bool error)
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(!m_releasing || !isReferenced(c))
            
            switch (m_state) {
                case State::READING: {
                    if (error || m_releasing) {
                        m_state = State::INVALID;
                        if (m_releasing) {
                            entry_release_result(c, this, false);
                            return;
                        }
                        raise_cache_event(c, CacheEntryEvent::READ_COMPLETED, error);
                        AMBRO_ASSERT(!isReferenced(c))
                        return;
                    }
                    
                    m_state = State::IDLE;
                    m_dirt_state = DirtState::CLEAN;
                    raise_cache_event(c, CacheEntryEvent::READ_COMPLETED, error);
                } break;
                
                case State::WRITING: {
                    AMBRO_ASSERT(m_dirt_state != DirtState::CLEAN)
                    AMBRO_ASSERT(!m_last_write_failed)
                    AMBRO_ASSERT(m_write_index <= m_write_count)
                    
                    if (!error && m_write_index < m_write_count) {
                        BlockIndexType write_block = m_block + m_write_index * m_write_stride;
                        m_write_index++;
                        m_block_user.startWrite(c, write_block, WrapBuffer::Make(m_buffer));
                        return;
                    }
                    
                    m_state = State::IDLE;
                    m_last_write_failed = error;
                    m_dirt_state = (!error && m_dirt_state == DirtState::WRITING) ? DirtState::CLEAN : DirtState::DIRTY;
                    
                    if (!error && m_dirt_state == DirtState::DIRTY && (!o->waiting_flush_requests.isEmpty() || m_releasing)) {
                        startWriting(c);
                        return;
                    }
                    
                    if (m_releasing) {
                        if (!error) {
                            AMBRO_ASSERT(m_dirt_state == DirtState::CLEAN)
                            m_state = State::INVALID;
                        }
                        entry_release_result(c, this, error);
                    }
                    
                    if (!o->waiting_flush_requests.isEmpty()) {
                        bool flush_error;
                        if (is_flush_completed(c, true, &flush_error)) {
                            complete_flush(c, flush_error);
                        }
                    }
                } break;
                
                default:
                    AMBRO_ASSERT(false);
            }
        }
        
        BlockAccessUser m_block_user;
        DoubleEndedList<CacheRef, &CacheRef::m_list_node> m_cache_users_list;
        State m_state;
        bool m_releasing;
        bool m_last_write_failed;
        DirtState m_dirt_state;
        DirtTimeType m_dirt_time;
        BlockIndexType m_block;
        BlockIndexType m_write_stride;
        uint8_t m_write_count;
        uint8_t m_write_index;
        char m_buffer[BlockSize];
    };
    
public:
    struct Object : public ObjBase<BlockCache, ParentObject, MakeTypeList<
        TheDebugObject
    >> {
        typename Context::EventLoop::QueuedEvent allocations_event;
        DirtTimeType current_dirt_time;
        DoubleEndedList<FlushRequest, &FlushRequest::m_waiting_flush_requests_node> waiting_flush_requests;
        DoubleEndedList<CacheRef, &CacheRef::m_list_node> pending_allocations;
        CacheEntry cache_entries[NumCacheEntries];
    };
};

#include <aprinter/EndNamespace.h>

#endif
