/*
 * Copyright (c) 2017 Ambroz Bizjak
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

#ifndef APRINTER_IPSTACK_IP_PATH_MTU_CACHE_H
#define APRINTER_IPSTACK_IP_PATH_MTU_CACHE_H

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <type_traits>

#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/meta/ChooseInt.h>
#include <aprinter/meta/MinMax.h>
#include <aprinter/base/Preprocessor.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Accessor.h>
#include <aprinter/base/OneOf.h>
#include <aprinter/structure/LinkModel.h>
#include <aprinter/structure/LinkedList.h>
#include <aprinter/system/TimedEventWrapper.h>

#include <aipstack/proto/IpAddr.h>
#include <aipstack/proto/Ip4Proto.h>

#include <aipstack/BeginNamespace.h>

template <typename Arg>
class IpPathMtuCache;

template <typename Arg>
APRINTER_DECL_TIMERS_CLASS(IpPathMtuCacheTimers, typename Arg::Context, IpPathMtuCache<Arg>, (MtuTimer))

template <typename Arg>
class IpPathMtuCache :
    private IpPathMtuCacheTimers<Arg>::Timers
{
    APRINTER_USE_TYPES1(Arg, (Params, Context, IpStack))
    APRINTER_USE_VALS(Params::PathMtuParams, (NumMtuEntries, MinMtuEntryRefs, MtuTimeoutMinutes))
    APRINTER_USE_TYPES1(Params::PathMtuParams, (MtuIndexService))
    
    APRINTER_USE_TYPES1(Context, (Clock))
    APRINTER_USE_TYPES1(Clock, (TimeType))
    APRINTER_USE_TYPES1(IpStack, (Iface))
    APRINTER_USE_VALS(IpStack, (MinMTU))
    APRINTER_USE_ONEOF
    
    static_assert(NumMtuEntries > 0, "");
    static_assert(MinMtuEntryRefs >= NumMtuEntries, "");
    static_assert(MtuTimeoutMinutes > 0, "");
    
    // Integer type for MTU entry reference count.
    using MtuRefCntType = APrinter::ChooseIntForMax<MinMtuEntryRefs, false>;
    
    APRINTER_USE_TIMERS_CLASS(IpPathMtuCacheTimers<Arg>, (MtuTimer))
    
    struct MtuEntry;
    
    // Array index type for MTU entries and null value.
    using MtuIndexType = APrinter::ChooseIntForMax<NumMtuEntries, false>;
    static MtuIndexType const MtuIndexNull = std::numeric_limits<MtuIndexType>::max();
    
    // Link model for MTU entries: array indices.
    using MtuLinkModel = APrinter::ArrayLinkModel<MtuEntry, MtuIndexType, MtuIndexNull>;
    
    // Index data structure for MTU entries by remote address.
    struct MtuIndexAccessor;
    using MtuIndexLookupKeyArg = Ip4Addr;
    struct MtuIndexKeyFuncs;
    APRINTER_MAKE_INSTANCE(MtuIndex, (MtuIndexService::template Index<
        MtuIndexAccessor, MtuIndexLookupKeyArg, MtuIndexKeyFuncs, MtuLinkModel>))
    
    // Linked list data structure for unreferenced MTU entries.
    struct MtuFreeListAccessor;
    using MtuFreeList = APrinter::LinkedList<MtuFreeListAccessor, MtuLinkModel, true>;
    
    // Timeout period for the MTU timer (one minute).
    static TimeType const MtuTimerTicks = 60.0 * (TimeType)Clock::time_freq;
    
    // MTU entry states.
    struct EntryState {
        enum : uint8_t {
            // Entry is not valid (not in index, in free list).
            Invalid,
            // Entry is valid and referenced (in index, not in free list).
            Referenced,
            // Entry is valid but not referenced (in index, in free list).
            Unused,
        };
    };
    
    // MTU entry structure.
    struct MtuEntry {
        typename MtuIndex::Node index_node;
        union {
            APrinter::LinkedListNode<MtuLinkModel> free_list_node;
            MtuRefCntType num_refs;
        };
        uint16_t mtu;
        uint8_t state;
        uint8_t minutes_old;
        Ip4Addr remote_addr;
    };
    
    // Node accessors for the data structures.
    struct MtuIndexAccessor : public APRINTER_MEMBER_ACCESSOR(&MtuEntry::index_node) {};
    struct MtuFreeListAccessor : public APRINTER_MEMBER_ACCESSOR(&MtuEntry::free_list_node) {};
    
    struct MtuIndexKeyFuncs {
        // Returns the key of an MTU entry for the index.
        inline static Ip4Addr GetKeyOfEntry (MtuEntry const &mtu_entry)
        {
            return mtu_entry.remote_addr;
        }
    };
    
    // Ref type of the link model.
    using MtuLinkModelRef = typename MtuLinkModel::Ref;
    
    inline MtuLinkModelRef mtuRef (MtuEntry &mtu_entry)
    {
        return MtuLinkModelRef(mtu_entry, &mtu_entry - m_mtu_entries);
    }
    
    inline typename MtuLinkModel::State mtuState ()
    {
        return m_mtu_entries;
    }
    
private:
    IpStack *m_ip_stack;
    typename MtuIndex::Index m_mtu_index;
    MtuFreeList m_mtu_free_list;
    MtuEntry m_mtu_entries[NumMtuEntries];
    
public:
    void init (IpStack *ip_stack)
    {
        // Initialize things.
        tim(MtuTimer()).init(Context());
        m_ip_stack = ip_stack;
        m_mtu_index.init();
        m_mtu_free_list.init();
        
        // Start the timer.
        tim(MtuTimer()).appendAfter(Context(), MtuTimerTicks);
        
        // Initialize the MTU entries.
        for (MtuEntry &mtu_entry : m_mtu_entries) {
            mtu_entry.state = EntryState::Invalid;
            m_mtu_free_list.append(mtuRef(mtu_entry), mtuState());
        }
    }
    
    void deinit ()
    {
        tim(MtuTimer()).deinit(Context());
    }
    
    class MtuRef
    {
        friend IpPathMtuCache;
        
    private:
        MtuIndexType m_entry_idx;
        
    public:
        inline void init ()
        {
            m_entry_idx = MtuIndexNull;
        }
        
        inline void deinit (IpPathMtuCache *cache)
        {
            reset(cache);
        }
        
        void reset (IpPathMtuCache *cache)
        {
            if (m_entry_idx != MtuIndexNull) {
                MtuEntry &mtu_entry = cache->get_entry(m_entry_idx);
                assert_entry_referenced(mtu_entry);
                
                if (mtu_entry.num_refs > 1) {
                    mtu_entry.num_refs--;
                } else {
                    mtu_entry.state = EntryState::Unused;
                    cache->m_mtu_free_list.append(cache->mtuRef(mtu_entry), cache->mtuState());
                }
                
                m_entry_idx = MtuIndexNull;
            }
        }
        
        inline bool isSetup ()
        {
            return m_entry_idx != MtuIndexNull;
        }
        
        bool setup (IpPathMtuCache *cache, Ip4Addr remote_addr, Iface *iface)
        {
            AMBRO_ASSERT(!isSetup())
            
            MtuLinkModelRef mtu_ref = cache->m_mtu_index.findEntry(cache->mtuState(), remote_addr);
            
            if (!mtu_ref.isNull()) {
                MtuEntry &mtu_entry = *mtu_ref;
                
                if (mtu_entry.state == EntryState::Referenced) {
                    AMBRO_ASSERT(mtu_entry.num_refs > 0)
                    
                    if (mtu_entry.num_refs == std::numeric_limits<MtuRefCntType>::max()) {
                        return false;
                    }
                    
                    mtu_entry.num_refs++;
                } else {
                    AMBRO_ASSERT(mtu_entry.state == EntryState::Unused)
                    
                    cache->m_mtu_free_list.remove(mtu_ref, cache->mtuState());
                    
                    mtu_entry.state = EntryState::Referenced;
                    mtu_entry.num_refs = 1;
                }
            } else {
                if (iface == nullptr) {
                    Ip4Addr route_addr;
                    if (!cache->m_ip_stack->routeIp4(remote_addr, nullptr, &iface, &route_addr)) {
                        return false;
                    }
                }
                
                mtu_ref = cache->m_mtu_free_list.first(cache->mtuState());
                if (mtu_ref.isNull()) {
                    return false;
                }
                
                MtuEntry &mtu_entry = *mtu_ref;
                AMBRO_ASSERT(mtu_entry.state == OneOf(EntryState::Invalid, EntryState::Unused))
                
                cache->m_mtu_free_list.removeFirst(cache->mtuState());
                
                if (mtu_entry.state == EntryState::Unused) {
                    AMBRO_ASSERT(mtu_entry.remote_addr != remote_addr)
                    cache->m_mtu_index.removeEntry(cache->mtuState(), mtu_ref);
                }
                
                mtu_entry.state = EntryState::Referenced;
                mtu_entry.num_refs = 1;
                mtu_entry.remote_addr = remote_addr;
                mtu_entry.mtu = iface->getMtu();
                mtu_entry.minutes_old = 0;
                
                cache->m_mtu_index.addEntry(cache->mtuState(), mtu_ref);
            }
            
            m_entry_idx = mtu_ref.getIndex();
            
            assert_entry_referenced(cache->get_entry(m_entry_idx));
            
            return true;
        }
        
        inline uint16_t getPmtu (IpPathMtuCache *cache)
        {
            AMBRO_ASSERT(isSetup())
            
            MtuEntry &mtu_entry = cache->get_entry(m_entry_idx);
            assert_entry_referenced(mtu_entry);
            
            return mtu_entry.mtu;
        }
        
        bool handleIcmpPacketTooBig (IpPathMtuCache *cache, uint16_t mtu_info)
        {
            AMBRO_ASSERT(isSetup())
            
            MtuEntry &mtu_entry = cache->get_entry(m_entry_idx);
            assert_entry_referenced(mtu_entry);
            
            // If the ICMP message does not include an MTU (mtu_info==0),
            // we assume the minimum PMTU that we allow. Generally we bump
            // up the reported next link MTU to be no less than our MinMTU.
            // This is what Linux does, it must be good enough for us too.
            uint16_t bump_mtu = APrinter::MaxValue(MinMTU, mtu_info);
            
            // If the PMTU would not have changed, don't do anything but let
            // the caller know.
            if (bump_mtu >= mtu_entry.mtu) {
                return false;
            }
            
            // Update PMTU, reset timeout.
            mtu_entry.mtu = bump_mtu;
            mtu_entry.minutes_old = 0;
            
            return true;
        }
    };
    
private:
    inline MtuEntry & get_entry (MtuIndexType index)
    {
        AMBRO_ASSERT(index < NumMtuEntries)
        
        return m_mtu_entries[index];
    }
    
    inline static void assert_entry_referenced (MtuEntry &mtu_entry)
    {
        AMBRO_ASSERT(mtu_entry.state == EntryState::Referenced)
        AMBRO_ASSERT(mtu_entry.mtu >= MinMTU)
        AMBRO_ASSERT(mtu_entry.num_refs > 0)
    }
    
    void timerExpired (MtuTimer, Context)
    {
        // Restart the timer.
        tim(MtuTimer()).appendAfter(Context(), MtuTimerTicks);
        
        // Update MTU entries.
        for (MtuEntry &mtu_entry : m_mtu_entries) {
            if (mtu_entry.state != EntryState::Invalid) {
                update_mtu_entry_expiry(mtu_entry);
            }
        }
    }
    
    void update_mtu_entry_expiry (MtuEntry &mtu_entry)
    {
        AMBRO_ASSERT(mtu_entry.state == OneOf(EntryState::Referenced, EntryState::Unused))
        AMBRO_ASSERT(mtu_entry.minutes_old <= MtuTimeoutMinutes)
        
        // If the entry is not expired yet, just increment minutes_old.
        if (mtu_entry.minutes_old < MtuTimeoutMinutes) {
            mtu_entry.minutes_old++;
            return;
        }
        
        // If the entry is unused, invalidate it.
        if (mtu_entry.state == EntryState::Unused) {
            m_mtu_index.removeEntry(mtuState(), mtuRef(mtu_entry));
            mtu_entry.state = EntryState::Invalid;
            // Move to the front of the free list.
            m_mtu_free_list.remove(mtuRef(mtu_entry), mtuState());
            m_mtu_free_list.prepend(mtuRef(mtu_entry), mtuState());
            return;
        }
        
        // Find the route to the destination.
        Iface *iface;
        Ip4Addr route_addr;
        if (!m_ip_stack->routeIp4(mtu_entry.remote_addr, nullptr, &iface, &route_addr)) {
            // Couldn't find an interface, will try again next timeout.
        } else {
            // Reset the PMTU to that of the interface.
            mtu_entry.mtu = iface->getMtu();
        }
        
        // Reset the timeout.
        // Here minutes_old is set to 1 not 0, this will allow the next timeout
        // to occur after exactly MtuTimeoutMinutes. But elsewhere we set
        // minutes_old to zero to ensure that a timeout does not occurs before
        // MtuTimeoutMinutes.
        mtu_entry.minutes_old = 1;
        
        assert_entry_referenced(mtu_entry);
    }
};

APRINTER_ALIAS_STRUCT(IpPathMtuParams, (
    APRINTER_AS_VALUE(size_t, NumMtuEntries),
    APRINTER_AS_VALUE(size_t, MinMtuEntryRefs),
    APRINTER_AS_VALUE(uint8_t, MtuTimeoutMinutes),
    APRINTER_AS_TYPE(MtuIndexService)
))

APRINTER_ALIAS_STRUCT_EXT(IpPathMtuCacheService, (
    APRINTER_AS_TYPE(PathMtuParams)
), (
    APRINTER_ALIAS_STRUCT_EXT(Compose, (
        APRINTER_AS_TYPE(Context),
        APRINTER_AS_TYPE(IpStack)
    ), (
        using Params = IpPathMtuCacheService;
        APRINTER_DEF_INSTANCE(Compose, IpPathMtuCache)
    ))
))

#include <aipstack/EndNamespace.h>

#endif
