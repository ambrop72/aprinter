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
#include <inttypes.h>

#include <aprinter/meta/ChooseInt.h>
#include <aprinter/meta/StructIf.h>
#include <aprinter/meta/FunctionIf.h>
#include <aprinter/meta/BasicMetaUtils.h>
#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/Callback.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/TransferVector.h>
#include <aprinter/base/LoopUtils.h>
#include <aprinter/structure/DoubleEndedList.h>

namespace APrinter {

#define APRINTER_DEBUG_BLOCKCACHE 0

#if APRINTER_DEBUG_BLOCKCACHE
#define APRINTER_BLOCKCACHE_MSG(...) Context::Printer::get_msg_output(c)->println(c, __VA_ARGS__)
#else
#define APRINTER_BLOCKCACHE_MSG(...)
#endif

template <typename Arg>
class BlockCache {
    using Context      = typename Arg::Context;
    using ParentObject = typename Arg::ParentObject;
    
public:
    struct Object;
    
public:
    using TheBlockAccess             = typename Arg::TheBlockAccess;
    static int const NumCacheEntries = Arg::NumCacheEntries;
    static int const NumIoUnits      = Arg::NumIoUnits;
    static int const MaxIoBlocks     = Arg::MaxIoBlocks;
    static bool const Writable       = Arg::Writable;
    
private:
    static_assert(NumCacheEntries > 0, "");
    static_assert(NumCacheEntries <= 64, "");
    static_assert(NumIoUnits > 0 && NumIoUnits <= NumCacheEntries, "");
    static_assert(MaxIoBlocks > 0 && MaxIoBlocks <= NumCacheEntries, "");
    static_assert(MaxIoBlocks <= TheBlockAccess::MaxIoBlocks, "");
    static_assert(MaxIoBlocks <= TheBlockAccess::MaxIoDescriptors, "");
    
    class CacheEntry;
    class IoDispatcher;
    class IoUnit;
    
    using TheDebugObject = DebugObject<Context, Object>;
    
    using DataWordType = typename TheBlockAccess::DataWordType;
    static size_t const BlockSizeInWords = TheBlockAccess::BlockSize / sizeof(DataWordType);
    
    using BlockAccessUser = If<Writable, typename TheBlockAccess::UserFull, typename TheBlockAccess::User>;
    
    using DirtTimeType = uint32_t;
    
    using CacheEntryIndexType = ChooseIntForMax<NumCacheEntries, true>;
    using IoUnitIndexType = ChooseIntForMax<NumIoUnits, true>;
    using IoBlockIndexType = ChooseIntForMax<MaxIoBlocks, true>;
    
    static int const NumBuffers = NumCacheEntries + (Writable ? TheBlockAccess::MaxBufferLocks : 0);
    using BufferIndexType = ChooseIntForMax<NumBuffers, true>;
    
    using NumRefsType = uint8_t;
    static NumRefsType const MaxNumRefs = (NumRefsType)-1;
    
public:
    using BlockIndexType = typename TheBlockAccess::BlockIndexType;
    static size_t const BlockSize = TheBlockAccess::BlockSize;
    
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        o->io_queue.init();
        o->io_queue_event.init(c, APRINTER_CB_STATFUNC_T(&BlockCache::io_queue_event_handler));
        writable_init(c);
        
        for (CacheEntry &entry : o->cache_entries) {
            entry.init(c);
        }
        
        for (IoUnit &unit : o->io_units) {
            unit.init(c);
        }
        
        TheDebugObject::init(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::deinit(c);
        writable_deinit_assert(c);
        
        for (IoUnit &unit : o->io_units) {
            unit.deinit(c);
        }
        
        for (CacheEntry &entry : o->cache_entries) {
            entry.deinit(c);
        }
        
        writable_deinit(c);
        o->io_queue_event.deinit(c);
    }
    
    static BlockIndexType hintBlocks (Context c, BlockIndexType protect_block, BlockIndexType start_block, BlockIndexType end_block, BlockIndexType write_stride, uint8_t write_count)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(protect_block <= start_block)
        AMBRO_ASSERT(start_block <= end_block)
        
        // Create two lists, of:
        // 1) Block indices in the given range which are already in the cache.
        // 2) Indices of cache entries which we may use to assign the blocks.
        
        BlockIndexType cached_blocks[NumCacheEntries];
        CacheEntryIndexType num_cached_blocks = 0;
        
        CacheEntryIndexType free_entries[NumCacheEntries];
        CacheEntryIndexType num_free_entries = 0;
        
        for (auto i : LoopRange<CacheEntryIndexType>(NumCacheEntries)) {
            CacheEntry *e = &o->cache_entries[i];
            if (e->isAssigned(c)) {
                BlockIndexType block = e->getBlock(c);
                if (block >= protect_block && block < end_block) {
                    if (block >= start_block) {
                        cached_blocks[num_cached_blocks++] = block;
                    }
                    // This entry is assigned with a block in the while protected range,
                    // so prevent it from being reassigned now to another hinted block.
                    continue;
                }
            }
            if (!e->isBeingReleased(c) && !e->isReferencedIncludingWeak(c) && (!e->isAssigned(c) || e->canReassign(c))) {
                free_entries[num_free_entries++] = i;
            }
        }
        
        // Assign blocks based on information in these lists.
        
        BlockIndexType block = start_block;
        while (block < end_block && num_free_entries > 0) {
            // Skip this block if it is already in the cache.
            bool already = false;
            for (auto i : LoopRange<CacheEntryIndexType>(num_cached_blocks)) {
                if (cached_blocks[i] == block) {
                    already = true;
                    break;
                }
            }
            
            if (!already) {
                // Pop a free entry from the list.
                CacheEntryIndexType free_entry_index = free_entries[--num_free_entries];
                CacheEntry *free_entry = &o->cache_entries[free_entry_index];
                
                // Assign this block to this entry.
                free_entry->assignBlockAndAttachUser(c, block, write_stride, write_count, false, nullptr);
            }
            
            block++;
        }
        
        // Inform them at which block we ran out of free entries, so they may
        // start the next hint at that block.
        return block;
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
        bool m_no_need_to_read;
    };
    
    class CacheRef : private SimpleDebugObject<Context>, private CacheRefWritableMemebers<Writable> {
        friend BlockCache;
        friend class CacheEntry;
        
        enum class State : uint8_t {INVALID, ALLOCATING_ENTRY, WAITING_READ, INIT_COMPL_EVENT, AVAILABLE, WEAK_REF};
        
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
        
        void resetWeak (Context c)
        {
            this->debugAccess(c);
            
            if (m_state == State::AVAILABLE) {
                m_state = State::WEAK_REF;
                get_entry(c)->detachUser(c, this, CacheEntry::DetachMode::HARD_TO_WEAK);
            } else {
                reset_internal(c);
            }
        }
        
        bool requestBlock (Context c, BlockIndexType block, BlockIndexType write_stride, uint8_t write_count, uint8_t flags)
        {
            this->debugAccess(c);
            TheDebugObject::access(c);
            
            if (!(flags & FLAG_NO_IMMEDIATE_COMPLETION) && (is_allocating_block(block) || is_attached_to_block(c, block))) {
                check_write_params(write_stride, write_count);
                if (m_state == State::WEAK_REF) {
                    if (!get_entry(c)->canIncrementRefCnt(c)) {
                        goto fallback;
                    }
                    m_state = State::AVAILABLE;
                    get_entry(c)->hardenWeakUser(c, this);
                }
                return m_state == State::AVAILABLE;
            }
            
        fallback:
            reset_internal(c);
            
            set_write_params(write_stride, write_count, flags);
            
            CacheEntryIndexType alloc_result = get_entry_for_block(c, block);
            if (alloc_result >= 0) {
                attach_to_entry(c, alloc_result, block, write_stride, write_count);
            }
            else if (Writable && alloc_result == -1) {
                // There's an entry being released, register for notification to retry then.
                register_allocation(c, block);
            }
            else {
                AMBRO_ASSERT(alloc_result == -2)
                // Fail.
                complete_init(c);
            }
            
            return false; // never do we end up in State::AVAILABLE in this branch
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
            
            return get_entry(c)->getBlock(c);
        }
        
        char const * getData (Context c, WrapBool<false> for_reading)
        {
            this->debugAccess(c);
            AMBRO_ASSERT(isAvailable(c))
            
            return get_entry(c)->getDataForReading(c);
        }
        
        APRINTER_FUNCTION_IF(Writable, char *, getData (Context c, WrapBool<true> for_writing))
        {
            this->debugAccess(c);
            AMBRO_ASSERT(isAvailable(c))
            
            return get_entry(c)->getDataForWriting(c);
        }
        
        APRINTER_FUNCTION_IF(Writable, void, markDirty (Context c))
        {
            this->debugAccess(c);
            AMBRO_ASSERT(isAvailable(c))
            
            get_entry(c)->markDirty(c);
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
        
        APRINTER_FUNCTION_IF_OR_EMPTY(Writable, void, register_allocation (Context c, BlockIndexType block))
        {
            auto *o = Object::self(c);
            
            m_state = State::ALLOCATING_ENTRY;
            this->m_allocating_block = block;
            o->pending_allocations.append(this);
        }
        
        CacheEntry * get_entry (Context c)
        {
            auto *o = Object::self(c);
            return &o->cache_entries[m_entry_index];
        }
        
        bool is_attached_to_block (Context c, BlockIndexType block)
        {
            return m_entry_index != -1 && block == get_entry(c)->getBlock(c);
        }
        
        APRINTER_FUNCTION_IF_ELSE(Writable, bool, is_allocating_block (BlockIndexType block), {
            return (m_state == State::ALLOCATING_ENTRY && this->m_allocating_block == block);
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
                auto mode = (m_state == State::WEAK_REF) ? CacheEntry::DetachMode::DETACH_WEAK : CacheEntry::DetachMode::DETACH_HARD;
                get_entry(c)->detachUser(c, this, mode);
                m_entry_index = -1;
            }
            if (Writable && m_state == State::ALLOCATING_ENTRY) {
                remove_from_pending_allocations(c);
            }
            m_event.unset(c);
            m_state = State::INVALID;
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
        
        void attach_to_entry (Context c, CacheEntryIndexType entry_index, BlockIndexType block, BlockIndexType write_stride, uint8_t write_count)
        {
            m_entry_index = entry_index;
            get_entry(c)->assignBlockAndAttachUser(c, block, write_stride, write_count, get_no_need_to_read(), this);
            if (!get_entry(c)->isInitialized(c)) {
                m_state = State::WAITING_READ;
            } else {
                complete_init(c);
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
            
            CacheEntryIndexType alloc_result;
            
            if (error) {
                goto fail;
            }
            
            alloc_result = get_entry_for_block(c, this->m_allocating_block);
            if (alloc_result >= 0) {
                o->pending_allocations.remove(this);
                attach_to_entry(c, alloc_result, this->m_allocating_block, this->m_write_stride, this->m_write_count);
            }
            else if (alloc_result == -1) {
                // There's an entry being released, so remain registered for notification.
                // Note that it cannot happen that another CacheRef then steals this entry
                // in that same report_allocation_event call, because get_entry_for_block
                // does not touch entries that are being released.
            }
            else {
                AMBRO_ASSERT(alloc_result == -2)
                goto fail;
            }
            
            return;
            
        fail:
            o->pending_allocations.remove(this);
            return complete_init(c);
        }
        
        void cache_read_completed (Context c, bool error)
        {
            this->debugAccess(c);
            AMBRO_ASSERT(m_entry_index != -1)
            AMBRO_ASSERT(m_state == State::WAITING_READ)
            
            if (error) {
                get_entry(c)->detachUser(c, this, CacheEntry::DetachMode::DETACH_HARD);
                m_entry_index = -1;
            }
            complete_init(c);
        }
        
        void weak_ref_broken (Context c)
        {
            this->debugAccess(c);
            AMBRO_ASSERT(m_state == State::WEAK_REF)
            AMBRO_ASSERT(m_entry_index != -1)
            AMBRO_ASSERT(!m_event.isSet(c))
            
            m_state = State::INVALID;
            m_entry_index = -1;
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
        for (auto i : LoopRange<BufferIndexType>(NumBuffers)) {
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
        for (CacheEntry &ce : o->cache_entries) {
            if (ce.isDirty(c)) {
                if (!for_new_request && ce.hasFlushWriteFailed(c)) {
                    error = true;
                } else {
                    AMBRO_ASSERT(for_new_request || ce.isWriteScheduledOrActive(c))
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
        
        for (CacheEntry &ce : o->cache_entries) {
            if (ce.canStartWrite(c)) {
                ce.scheduleWriting(c);
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
    
    // Meaning of the return value:
    // >=0 - Can use this entry right away. It could be either free, assigned to the requested block,
    //       or assigned to another block but suitable for immediate reassignment.
    // -1  - No entry available, but you should wait. An entry is being released.
    //       This function itself might have started a release of an entry.
    // -2  - No entry available, and do not wait. This is an error.
    static CacheEntryIndexType get_entry_for_block (Context c, BlockIndexType block)
    {
        auto *o = Object::self(c);
        
        CacheEntryIndexType res = get_entry_for_block_core(c, block);
        if (res >= 0 && !o->cache_entries[res].canIncrementRefCnt(c)) {
            res = -2;
        }
        return res;
    }
    
    static CacheEntryIndexType get_entry_for_block_core (Context c, BlockIndexType block)
    {
        auto *o = Object::self(c);
        
        CacheEntryIndexType free_entry = -1;
        CacheEntryIndexType evictable_entry = -1;
        CacheEntryIndexType releasing_entry = -1;
        
        for (auto entry_index : LoopRange<CacheEntryIndexType>(NumCacheEntries)) {
            CacheEntry *ce = &o->cache_entries[entry_index];
            
            if (ce->isAssigned(c) && ce->getBlock(c) == block) {
                return ce->isBeingReleased(c) ? -1 : entry_index;
            }
            
            if (!ce->isBeingReleased(c)) {
                if (!ce->isAssigned(c)) {
                    free_entry = entry_index;
                }
                else if (!Writable ? ce->canReassign(c) : (ce->isAssigned(c) && !ce->isReferenced(c))) {
                    if (evictable_entry == -1 || eviction_lesser_than(c, ce, &o->cache_entries[evictable_entry])) {
                        evictable_entry = entry_index;
                    }
                }
            } else {
                releasing_entry = entry_index;
            }
        }
        
        if (free_entry != -1) {
            return free_entry;
        }
        
        if (evictable_entry != -1) {
            CacheEntry *ee = &o->cache_entries[evictable_entry];
            
            if (!Writable) {
                AMBRO_ASSERT(ee->canReassign(c))
                return evictable_entry;
            }
            
            if (ee->canReassign(c) && (releasing_entry == -1 || !eviction_lesser_than(c, &o->cache_entries[releasing_entry], ee))) {
                return evictable_entry;
            }
            
            if (releasing_entry == -1) {
                ee->startRelease(c);
                releasing_entry = evictable_entry;
            }
        }
        
        if (Writable && releasing_entry != -1) {
            return -1;
        }
        
        return -2;
    }
    
    /**
     * Determines if eviction of e1 is preferred to eviction of e2.
     * 
     * We want, in order:
     * (1) Eviction of completely unreferenced entries is preferred to eviction of
     *     weak-only referenced entries.
     * (2) Eviction of non-dirty entries is preferred to eviction of dirty entries.
     * (3) Eviction of entries which have been dirty longer is preferred.
     * 
     * The purpose of (1) is to minimize writing of FAT table entries.
     * The purpose of (2) and (3) is to take advantage of multi-block writes.
     */
    APRINTER_FUNCTION_IF_ELSE_EXT(Writable, static, bool, eviction_lesser_than (Context c, CacheEntry *e1, CacheEntry *e2), {
        auto *o = Object::self(c);
        bool r1 = e1->isReferencedIncludingWeak(c);
        bool r2 = e2->isReferencedIncludingWeak(c);
        if (r1 < r2) {
            return true;
        }
        if (r1 == r2) {
            DirtTimeType current = o->current_dirt_time;
            DirtTimeType t1 = e1->isDirty(c) ? e1->getDirtTime(c) : (current + 1);
            DirtTimeType t2 = e2->isDirty(c) ? e2->getDirtTime(c) : (current + 1);
            return (DirtTimeType)(current - t1) > (DirtTimeType)(current - t2);
        }
        return false;
    }, {
        return e1->isReferencedIncludingWeak(c) < e2->isReferencedIncludingWeak(c);
    })
    
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
        o->allocations_event.prependNow(c);
    }
    
    APRINTER_FUNCTION_IF_EXT(Writable, static, void, allocations_event_handler (Context c))
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        for (CacheEntry &ce : o->cache_entries) {
            if (ce.isBeingReleased(c) && !ce.isAssigned(c)) {
                ce.completeRelease(c);
            }
        }
        
        report_allocation_event(c, false);
    }
    
    APRINTER_FUNCTION_IF_EXT(Writable, static, BufferIndexType, find_free_buffer (Context c))
    {
        auto *o = Object::self(c);
        
        for (auto i : LoopRange<BufferIndexType>(NumBuffers)) {
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
        bool m_flush_write_failed;
        DirtState m_dirt_state;
        uint8_t m_write_count;
        uint8_t m_write_index;
        DirtTimeType m_dirt_time;
        BlockIndexType m_write_stride;
        BufferIndexType m_active_buffer;
        BufferIndexType m_writing_buffer;
    };
    
    class CacheEntry : private CacheEntryWritableMemebers<Writable> {
        friend class IoDispatcher;
        friend class IoUnit;
        
        enum class State : uint8_t {INVALID, READING, IDLE, WRITING};
        
    public:
        void init (Context c)
        {
            m_cache_users_list.init();
            m_num_hard_refs = 0;
            m_state = State::INVALID;
            IoQueue::markRemoved(this);
            writable_entry_init(c);
        }
        
        void deinit (Context c)
        {
            AMBRO_ASSERT(m_cache_users_list.isEmpty())
            AMBRO_ASSERT(m_num_hard_refs == 0)
            
            writable_entry_deinit(c);
        }
        
        bool isAssigned (Context c)
        {
            return m_state != State::INVALID;
        }
        
        bool isInitialized (Context c)
        {
            return m_state == State::IDLE || m_state == State::WRITING;
        }
        
        bool isIoActive (Context c)
        {
            return m_state == State::READING || (Writable && m_state == State::WRITING);
        }
        
        bool canReassign (Context c)
        {
            return m_state == State::IDLE && get_dirt_state() == DirtState::CLEAN && m_num_hard_refs == 0;
        }
        
        APRINTER_FUNCTION_IF(Writable, bool, isDirty (Context c))
        {
            return isInitialized(c) && this->m_dirt_state != DirtState::CLEAN;
        }
        
        APRINTER_FUNCTION_IF_ELSE(Writable, bool, canStartWrite (Context c), {
            return m_state == State::IDLE && this->m_dirt_state != DirtState::CLEAN;
        }, {
            return false;
        })
        
        bool isReferenced (Context c)
        {
            return m_num_hard_refs != 0;
        }
        
        bool isReferencedIncludingWeak (Context c)
        {
            AMBRO_ASSERT(!m_cache_users_list.isEmpty() || m_num_hard_refs == 0)
            
            return !m_cache_users_list.isEmpty();
        }
        
        APRINTER_FUNCTION_IF_ELSE(Writable, bool, isBeingReleased (Context c), {
            return this->m_releasing;
        }, {
            return false;
        })
        
        APRINTER_FUNCTION_IF_ELSE(Writable, bool, hasLastWriteFailed (Context c), {
            AMBRO_ASSERT(isAssigned(c))
            return this->m_last_write_failed;
        }, {
            return false;
        })
        
        APRINTER_FUNCTION_IF(Writable, bool, hasFlushWriteFailed (Context c))
        {
            AMBRO_ASSERT(isAssigned(c))
            return this->m_flush_write_failed;
        }
        
        APRINTER_FUNCTION_IF(Writable, bool, isWriteScheduledOrActive (Context c))
        {
            return (this->m_write_event.isSet(c) || m_state == State::WRITING);
        }
        
        BlockIndexType getBlock (Context c)
        {
            AMBRO_ASSERT(isAssigned(c))
            return m_block;
        }
        
        char const * getDataForReading (Context c)
        {
            AMBRO_ASSERT(isInitialized(c))
            return (char const *)get_buffer(c);
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
            
            return (char *)get_buffer(c);
        }
        
        APRINTER_FUNCTION_IF(Writable, DirtTimeType, getDirtTime (Context c))
        {
            AMBRO_ASSERT(isDirty(c))
            return this->m_dirt_time;
        }
        
        bool canIncrementRefCnt (Context c)
        {
            return m_num_hard_refs < MaxNumRefs;
        }
        
        void assignBlockAndAttachUser (Context c, BlockIndexType block, BlockIndexType write_stride, uint8_t write_count, bool no_need_to_read, CacheRef *user)
        {
            AMBRO_ASSERT(write_count >= 1)
            AMBRO_ASSERT(!isBeingReleased(c))
            AMBRO_ASSERT(!user || canIncrementRefCnt(c))
            
            if (isAssigned(c) && block == m_block) {
                check_write_params(write_stride, write_count);
            } else {
                AMBRO_ASSERT(m_num_hard_refs == 0)
                AMBRO_ASSERT(m_state == State::INVALID || m_state == State::IDLE)
                AMBRO_ASSERT(m_state == State::INVALID || get_dirt_state() == DirtState::CLEAN)
                
                break_weak_refs(c);
                
                m_block = block;
                writable_assign(c, write_stride, write_count);
                
                if (Writable && no_need_to_read) {
                    m_state = State::IDLE;
                    memset(get_buffer(c), 0, BlockSize);
                    // No implicit markDirty.
                } else {
                    APRINTER_BLOCKCACHE_MSG("c RS %" PRIu32, (uint32_t)block);
                    m_state = State::READING;
                    IoDispatcher::dispatch(c, this);
                }
            }
            
            if (user) {
                m_cache_users_list.prepend(user);
                m_num_hard_refs++;
            }
        }
        
        enum class DetachMode {HARD_TO_WEAK, DETACH_HARD, DETACH_WEAK};
        
        void detachUser (Context c, CacheRef *user, DetachMode mode)
        {
            AMBRO_ASSERT(mode == DetachMode::DETACH_WEAK || m_num_hard_refs > 0)
            
            if (mode != DetachMode::HARD_TO_WEAK) {
                m_cache_users_list.remove(user);
            }
            if (mode != DetachMode::DETACH_WEAK) {
                m_num_hard_refs--;
            }
        }
        
        void hardenWeakUser (Context c, CacheRef *user)
        {
            AMBRO_ASSERT(canIncrementRefCnt(c))
            AMBRO_ASSERT(!m_cache_users_list.isEmpty())
            AMBRO_ASSERT(!isBeingReleased(c))
            
            m_num_hard_refs++;
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
            
            this->m_write_event.prependNow(c);
        }
        
        APRINTER_FUNCTION_IF_OR_EMPTY(Writable, void, startRelease (Context c))
        {
            AMBRO_ASSERT(isAssigned(c))
            AMBRO_ASSERT(!canReassign(c))
            AMBRO_ASSERT(!isReferenced(c))
            AMBRO_ASSERT(!isBeingReleased(c))
            
            break_weak_refs(c);
            
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
            this->m_flush_write_failed = false;
            this->m_dirt_state = DirtState::CLEAN;
            this->m_write_stride = write_stride;
            this->m_write_count = write_count;
        }
        
        APRINTER_FUNCTION_IF_ELSE(Writable, DataWordType *, get_buffer (Context c), {
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
        
        void break_weak_refs (Context c)
        {
            AMBRO_ASSERT(m_num_hard_refs == 0)
            
            for (CacheRef *ref = m_cache_users_list.first(); ref; ref = m_cache_users_list.next(ref)) {
                ref->weak_ref_broken(c);
            }
            m_cache_users_list.init();
        }
        
        // In READING or WRITING state we return the block index we wish to read/write.
        // In IDLE state we return the first block index we would write if we started writing now.
        APRINTER_FUNCTION_IF_ELSE(Writable, BlockIndexType, get_io_block_index (), {
            BlockIndexType block_offset = (m_state == State::WRITING) ? (this->m_write_index * this->m_write_stride) : 0;
            return m_block + block_offset;
        }, {
            return m_block;
        })
        
        void block_user_handler (Context c, bool error)
        {
            auto *o = Object::self(c);
            TheDebugObject::access(c);
            AMBRO_ASSERT(!isBeingReleased(c) || !isReferencedIncludingWeak(c))
            
            if (m_state == State::READING) {
                APRINTER_BLOCKCACHE_MSG("c RD %" PRIu32 " e%d", (uint32_t)m_block, (int)error);
                if (isBeingReleased(c)) {
                    m_state = State::INVALID;
                    return schedule_allocations_check(c);
                }
                m_state = error ? State::INVALID : State::IDLE;
                raise_read_completed(c, error);
                AMBRO_ASSERT(!error || !isReferencedIncludingWeak(c))
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
            write_starting(c);
            IoDispatcher::dispatch(c, this);
        }
        
        APRINTER_FUNCTION_IF_OR_EMPTY(Writable, void, write_starting (Context c))
        {
            AMBRO_ASSERT(m_state == State::IDLE)
            AMBRO_ASSERT(this->m_dirt_state == DirtState::DIRTY)
            AMBRO_ASSERT(this->m_writing_buffer == -1)
            
            m_state = State::WRITING;
            this->m_flush_write_failed = false;
            this->m_dirt_state = DirtState::WRITING;
            this->m_write_index = 0;
            this->m_write_event.unset(c);
            
            APRINTER_BLOCKCACHE_MSG("c WS %" PRIu32 " 1/%d", (uint32_t)m_block, (int)this->m_write_count);
        }
        
        APRINTER_FUNCTION_IF_OR_EMPTY(Writable, void, block_write_completed (Context c, bool error))
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(this->m_dirt_state != DirtState::CLEAN)
            AMBRO_ASSERT(!this->m_flush_write_failed)
            AMBRO_ASSERT(this->m_write_index < this->m_write_count)
            AMBRO_ASSERT(this->m_writing_buffer == -1)
            
            if (!error && this->m_write_index + 1 < this->m_write_count) {
                this->m_write_index++;
                APRINTER_BLOCKCACHE_MSG("c WS %" PRIu32 " %d/%d (%" PRIu32 ")", (uint32_t)m_block, (int)(this->m_write_index + 1), (int)this->m_write_count, (uint32_t)get_io_block_index());
                IoDispatcher::dispatch(c, this);
                return;
            }
            
            APRINTER_BLOCKCACHE_MSG("c WD %" PRIu32 " e%d", (uint32_t)m_block, (int)error);
            
            m_state = State::IDLE;
            this->m_last_write_failed = error;
            this->m_flush_write_failed = error;
            this->m_dirt_state = (!error && this->m_dirt_state == DirtState::WRITING) ? DirtState::CLEAN : DirtState::DIRTY;
            
            if (!error && this->m_dirt_state == DirtState::DIRTY && (!o->waiting_flush_requests.isEmpty() || this->m_releasing)) {
                return write_event_handler(c);
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
        
        DoubleEndedList<CacheRef, &CacheRef::m_list_node, false> m_cache_users_list;
        DoubleEndedListNode<CacheEntry> m_queue_node;
        BlockIndexType m_block;
        NumRefsType m_num_hard_refs;
        State m_state;
        
    public:
        using IoQueue = DoubleEndedList<CacheEntry, &CacheEntry::m_queue_node>;
    };
    
    class IoDispatcher {
    public:
        static void dispatch (Context c, CacheEntry *e)
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(e->isIoActive(c))
            AMBRO_ASSERT(CacheEntry::IoQueue::isRemoved(e))
            
            o->io_queue.append(e);
            if (!o->io_queue_event.isSet(c)) {
                o->io_queue_event.appendNowNotAlready(c);
            }
        }
        
        static void workQueue (Context c)
        {
            auto *o = Object::self(c);
            
            CacheEntry *e;
            while ((e = o->io_queue.first())) {
                AMBRO_ASSERT(!CacheEntry::IoQueue::isRemoved(e))
                
                IoUnit *unit = find_empty_unit(c);
                if (!unit) {
                    break;
                }
                
                o->io_queue.remove(e);
                CacheEntry::IoQueue::markRemoved(e);
                
                unit->acceptJob(c, e);
            }
        }
        
    private:
        static IoUnit * find_empty_unit (Context c)
        {
            auto *o = Object::self(c);
            
            for (IoUnit &unit : o->io_units) {
                if (unit.isIdle(c)) {
                    return &unit;
                }
            }
            return nullptr;
        }
    };
    
    static void io_queue_event_handler (Context c)
    {
        IoDispatcher::workQueue(c);
    }
    
    class IoUnit {
        enum class State : uint8_t {IDLE, READING, WRITING};
        
    public:
        void init (Context c)
        {
            m_block_user.init(c, APRINTER_CB_OBJFUNC_T(&IoUnit::block_user_handler, this));
            m_state = State::IDLE;
            writable_unit_init(c);
        }
        
        void deinit (Context c)
        {
            m_block_user.deinit(c);
        }
        
        bool isIdle (Context c)
        {
            return (m_state == State::IDLE);
        }
        
        void acceptJob (Context c, CacheEntry *first_e)
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(m_state == State::IDLE)
            AMBRO_ASSERT(first_e->isIoActive(c))
            AMBRO_ASSERT(CacheEntry::IoQueue::isRemoved(first_e))
            
            // Look at what IO we will be doing (for the requested block).
            BlockIndexType start_block = first_e->get_io_block_index();
            bool is_write = (Writable && first_e->m_state == CacheEntry::State::WRITING);
            
            // The first entry in the chain will be the one requesting this I/O.
            m_num_blocks = 1;
            m_entry_indices[0] = (first_e - o->cache_entries);
            
            // Try to extend the I/O operation into the blocks that follow the requested block.
            if (MaxIoBlocks > 1) {
                extend_io(c, first_e, start_block);
            }
            
            // Build transfer descriptors.
            for (auto i : LoopRange<IoBlockIndexType>(m_num_blocks)) {
                CacheEntry *this_e = &o->cache_entries[m_entry_indices[i]];
                m_descriptors[i] = TransferDescriptor<DataWordType>{this_e->get_buffer(c), BlockSizeInWords};
            }
            
            APRINTER_BLOCKCACHE_MSG("c I%c %" PRIu32 " %d", (is_write?'W':'R'), start_block, (int)m_num_blocks);
            
            // Finally start this I/O.
            m_state = is_write ? State::WRITING : State::READING;
            m_block_user.startReadOrWrite(c, is_write, start_block, m_num_blocks, TransferVector<DataWordType>{m_descriptors, m_num_blocks});
        }
        
    private:
        APRINTER_FUNCTION_IF_OR_EMPTY(Writable, void, writable_unit_init (Context c))
        {
            m_block_user.setLocker(c, APRINTER_CB_OBJFUNC_T(&IoUnit::block_user_locker<>, this));
        }
        
        void extend_io (Context c, CacheEntry *first_e, BlockIndexType start_block)
        {
            auto *o = Object::self(c);
            
            // Make sure that after a failed multi-block write, the next write attempt for all
            // involved entries will be a single-block write (see also extension check below).
            // The rationale is that a multi-block write may have failed due to a specific block.
            if (first_e->hasLastWriteFailed(c)) {
                return;
            }
            
            // Clear entry indices for the remainder of the block sequence (needed for next step).
            for (auto i : LoopRange<IoBlockIndexType>(1, MaxIoBlocks)) {
                m_entry_indices[i] = -1;
            }
            
            // Find candidate blocks to add to the sequence.
            for (CacheEntry &this_e : o->cache_entries) {
                // Ignore unassigned entries (they do not have a block index).
                if (!this_e.isAssigned(c)) {
                    continue;
                }
                
                // Check if the entry has a place in the sequence.
                BlockIndexType block_index = this_e.get_io_block_index();
                if (!(block_index > start_block && block_index - start_block < MaxIoBlocks)) {
                    continue;
                }
                
                // See above...
                if (this_e.hasLastWriteFailed(c)) {
                    continue;
                }
                
                if (this_e.isIoActive(c)) {
                    // Active I/O - we can take the entry if the I/O direction matches and it is still in the queue.
                    if (!((!Writable || first_e->m_state == this_e.m_state) && !CacheEntry::IoQueue::isRemoved(&this_e))) {
                        continue;
                    }
                } else {
                    // Inactive I/O - we can take the entry if we are writing and the entry is ready for writing.
                    if (!(Writable && first_e->m_state == CacheEntry::State::WRITING && this_e.canStartWrite(c))) {
                        continue;
                    }
                }
                
                // The entry is a candidate, add it to the list.
                // Unless some other entry is already in this place - but the only way this can
                // happen if the user caused a conflict with the write strides.
                IoBlockIndexType io_index = block_index - start_block;
                if (m_entry_indices[io_index] == -1) {
                    m_entry_indices[io_index] = (&this_e - o->cache_entries);
                }
            }
            
            // Extend the chain into the candidate entries as much as possible,
            // keeping it contiguous. Update these entries to reflect start of I/O.
            while (m_num_blocks < MaxIoBlocks && m_entry_indices[m_num_blocks] != -1) {
                CacheEntry *this_e = &o->cache_entries[m_entry_indices[m_num_blocks]];
                
                if (this_e->isIoActive(c)) {
                    // It was queued, so remove it from the I/O queue.
                    o->io_queue.remove(this_e);
                    CacheEntry::IoQueue::markRemoved(this_e);
                } else {
                    // It was idle, notify it that writing has started.
                    AMBRO_ASSERT(Writable)
                    this_e->write_starting(c);
                }
                
                m_num_blocks++;
            }
        }
        
        APRINTER_FUNCTION_IF(Writable, void, block_user_locker (Context c, bool lock_else_unlock))
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(m_state == State::WRITING)
            
            for (auto i : LoopRange<IoBlockIndexType>(m_num_blocks)) {
                CacheEntry *this_e = &o->cache_entries[m_entry_indices[i]];
                this_e->block_user_locker(c, lock_else_unlock);
            }
        }
        
        void block_user_handler (Context c, bool error)
        {
            auto *o = Object::self(c);
            TheDebugObject::access(c);
            AMBRO_ASSERT(m_state == State::READING || (Writable && m_state == State::WRITING))
            
            if (error) {
                APRINTER_BLOCKCACHE_MSG("I/O FAILED for %d blocks", (int)m_num_blocks);
            }
            
            // Dispatch the results while the unit still appears busy.
            // We wouldn't want jobs to be dispatched to this unit in this state.
            for (auto i : LoopRange<IoBlockIndexType>(m_num_blocks)) {
                CacheEntry *this_e = &o->cache_entries[m_entry_indices[i]];
                this_e->block_user_handler(c, error);
            }
            
            // Only now mark the unit as idle.
            m_state = State::IDLE;
            
            // Dispatch jobs now that unit is idle.
            IoDispatcher::workQueue(c);
        }
        
        BlockAccessUser m_block_user;
        TransferDescriptor<DataWordType> m_descriptors[MaxIoBlocks];
        CacheEntryIndexType m_entry_indices[MaxIoBlocks];
        IoBlockIndexType m_num_blocks;
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
        IoUnit io_units[NumIoUnits];
        typename CacheEntry::IoQueue io_queue;
        typename Context::EventLoop::QueuedEvent io_queue_event;
        DataWordType buffers[NumBuffers][BlockSizeInWords];
    };
};

APRINTER_ALIAS_STRUCT_EXT(BlockCacheArg, (
    APRINTER_AS_TYPE(Context),
    APRINTER_AS_TYPE(ParentObject),
    APRINTER_AS_TYPE(TheBlockAccess),
    APRINTER_AS_VALUE(int, NumCacheEntries),
    APRINTER_AS_VALUE(int, NumIoUnits),
    APRINTER_AS_VALUE(int, MaxIoBlocks),
    APRINTER_AS_VALUE(bool, Writable)
), (
    APRINTER_DEF_INSTANCE(BlockCacheArg, BlockCache)
))

}

#endif
