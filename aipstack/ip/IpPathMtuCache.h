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
#include <aprinter/structure/OperatorKeyCompare.h>
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
    APRINTER_USE_VALS(Params::PathMtuParams, (NumMtuEntries, MtuTimeoutMinutes))
    APRINTER_USE_TYPES1(Params::PathMtuParams, (MtuIndexService))
    
    APRINTER_USE_TYPES1(Context, (Clock))
    APRINTER_USE_TYPES1(Clock, (TimeType))
    APRINTER_USE_TYPES1(IpStack, (Iface, Ip4RouteInfo))
    APRINTER_USE_VALS(IpStack, (MinMTU))
    APRINTER_USE_ONEOF
    
    static_assert(NumMtuEntries > 0, "");
    static_assert(MtuTimeoutMinutes > 0, "");
    
    APRINTER_USE_TIMERS_CLASS(IpPathMtuCacheTimers<Arg>, (MtuTimer))
    
    struct MtuEntry;
    
    // Array index type for MTU entries and null value.
    using MtuIndexType = APrinter::ChooseIntForMax<NumMtuEntries, false>;
    static MtuIndexType const MtuIndexNull = std::numeric_limits<MtuIndexType>::max();
    
    // Link model for MTU entries: array indices.
    struct MtuEntriesAccessor;
    using MtuLinkModel = APrinter::ArrayLinkModelWithAccessor<
        MtuEntry, MtuIndexType, MtuIndexNull, IpPathMtuCache, MtuEntriesAccessor>;
    
    // Index data structure for MTU entries by remote address.
    struct MtuIndexAccessor;
    using MtuIndexLookupKeyArg = Ip4Addr;
    struct MtuIndexKeyFuncs;
    APRINTER_MAKE_INSTANCE(MtuIndex, (MtuIndexService::template Index<
        MtuIndexAccessor, MtuIndexLookupKeyArg, MtuIndexKeyFuncs, MtuLinkModel>))
    
    // Linked list data structure for unreferenced MTU entries.
    struct MtuFreeListAccessor;
    using MtuFreeList = APrinter::LinkedList<MtuFreeListAccessor, MtuLinkModel, true>;
    
public:
    class MtuRef;
    
private:
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
    
    // Each MtuEntry in Referenced states keeps a doubly-linked list
    // of MtuRef objects. We use a slightly special kind of list,
    // where the head of the list (MtuEntry.refs_list_first) points
    // to a different part of a node (to the next link) than a normal
    // node's next link does (to the prev link). This allows us to
    // efficiently determine whether the node is the first node.
    // This Link struct enables this data structure. It simply
    // contains a pointer the same type.
    struct Link {
        Link *link;
        
        // Convenience function for use when inherited.
        inline Link * self ()
        {
            return this;
        }
    };
    
    // These types allow MtuRef to inherit Link twice.
    struct PrevLink : public Link {};
    struct NextLink : public Link {};
    
    // MTU entry structure.
    struct MtuEntry {
        typename MtuIndex::Node index_node;
        union {
            APrinter::LinkedListNode<MtuLinkModel> free_list_node;
            Link refs_list_first;
        };
        uint16_t mtu;
        uint8_t state;
        uint8_t minutes_old;
        Ip4Addr remote_addr;
    };
    
    // Node accessors for the data structures.
    struct MtuIndexAccessor : public APRINTER_MEMBER_ACCESSOR(&MtuEntry::index_node) {};
    struct MtuFreeListAccessor : public APRINTER_MEMBER_ACCESSOR(&MtuEntry::free_list_node) {};
    
    struct MtuIndexKeyFuncs : public APrinter::OperatorKeyCompare {
        // Returns the key of an MTU entry for the index.
        inline static Ip4Addr GetKeyOfEntry (MtuEntry const &mtu_entry)
        {
            return mtu_entry.remote_addr;
        }
    };
    
    // Ref type of the link model.
    using MtuLinkModelRef = typename MtuLinkModel::Ref;
    
private:
    IpStack *m_ip_stack;
    typename MtuIndex::Index m_mtu_index;
    MtuFreeList m_mtu_free_list;
    MtuEntry m_mtu_entries[NumMtuEntries];
    
    // Accessor for the m_mtu_entries array.
    struct MtuEntriesAccessor : public APRINTER_MEMBER_ACCESSOR(&IpPathMtuCache::m_mtu_entries) {};
    
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
            m_mtu_free_list.append({mtu_entry, *this}, *this);
        }
    }
    
    void deinit ()
    {
        tim(MtuTimer()).deinit(Context());
    }
    
    bool handleIcmpPacketTooBig (Ip4Addr remote_addr, uint16_t mtu_info)
    {
        MtuLinkModelRef mtu_ref = m_mtu_index.findEntry(*this, remote_addr);
        if (mtu_ref.isNull()) {
            return false;
        }
        
        MtuEntry &mtu_entry = *mtu_ref;
        AMBRO_ASSERT(mtu_entry.state == OneOf(EntryState::Referenced, EntryState::Unused))
        
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
        
        // Notify all MtuRef referencing this entry.
        if (mtu_entry.state == EntryState::Referenced) {
            notify_pmtu_changed(mtu_entry);
        }
        
        return true;
    }
    
    class MtuRef :
        private PrevLink,
        private NextLink
    {
        friend IpPathMtuCache;
        
    public:
        inline void init ()
        {
            // Clear prev link to indicate unused MtuRef.
            PrevLink::link = nullptr;
        }
        
        void reset (IpPathMtuCache *cache)
        {
            // If PrevLink is null we are unused (not in a list).
            if (PrevLink::link == nullptr) {
                return;
            }
            
            // Check if we are the first node by seeing if the destination
            // of PrevLink points back to NextLink (rather than to PrevLink).
            bool is_first = PrevLink::link->link == NextLink::self();
            
            if (is_first && NextLink::link == nullptr) {
                // We are the only node, transition entry to Unused state.
                MtuEntry &mtu_entry = get_entry_from_first(PrevLink::link);
                assert_entry_referenced(mtu_entry);
                mtu_entry.state = EntryState::Unused;
                cache->m_mtu_free_list.append({mtu_entry, *cache}, *cache);
            } else {
                // We are not the only node, remove ourselves.
                // Setup the link from the previous to the next node.
                Link *prev_link_dst;
                if (is_first) {
                    MtuRef &next_ref = get_ref_from_prev_link(NextLink::link);
                    prev_link_dst = next_ref.NextLink::self();
                } else {
                    AMBRO_ASSERT(PrevLink::link->link == PrevLink::self())
                    prev_link_dst = NextLink::link;
                }
                PrevLink::link->link = prev_link_dst;
                
                // Setup the link from the next to the previous node, if any.
                if (NextLink::link != nullptr) {
                    AMBRO_ASSERT(NextLink::link->link == NextLink::self())
                    NextLink::link->link = PrevLink::link;
                }
            }
            
            // Clear prev link to indicate unused MtuRef.
            PrevLink::link = nullptr;
        }
        
        inline bool isSetup ()
        {
            return PrevLink::link != nullptr;
        }
        
        bool setup (IpPathMtuCache *cache, Ip4Addr remote_addr, Iface *iface, uint16_t &out_pmtu)
        {
            AMBRO_ASSERT(!isSetup())
            
            // Lookup this address in the index.
            MtuLinkModelRef mtu_ref = cache->m_mtu_index.findEntry(*cache, remote_addr);
            
            if (!mtu_ref.isNull()) {
                // Got an existing MtuEntry for this address.
                MtuEntry &mtu_entry = *mtu_ref;
                
                if (mtu_entry.state == EntryState::Unused) {
                    // Unused entry, change to Referenced and insert ourselves.
                    
                    cache->m_mtu_free_list.remove(mtu_ref, *cache);
                    
                    mtu_entry.state = EntryState::Referenced;
                    NextLink::link = nullptr;
                } else {
                    // Referenced entry, insert ourselves.
                    AMBRO_ASSERT(mtu_entry.state == EntryState::Referenced)
                    AMBRO_ASSERT(mtu_entry.refs_list_first.link != nullptr)
                    
                    MtuRef &next_ref = get_ref_from_next_link(mtu_entry.refs_list_first.link);
                    AMBRO_ASSERT(next_ref.PrevLink::link == mtu_entry.refs_list_first.self())
                    
                    NextLink::link = next_ref.PrevLink::self();
                    next_ref.PrevLink::link = NextLink::self();
                }
            } else {
                // There is MtuEntry for this address, we will try to add one.
                
                // If no interface is provided, find the interface for the initial PMTU.
                if (iface == nullptr) {
                    Ip4RouteInfo route_info;
                    if (!cache->m_ip_stack->routeIp4(remote_addr, route_info)) {
                        return false;
                    }
                    iface = route_info.iface;
                }
                
                // Get an MtuEntry from the free list.
                mtu_ref = cache->m_mtu_free_list.first(*cache);
                if (mtu_ref.isNull()) {
                    return false;
                }
                
                MtuEntry &mtu_entry = *mtu_ref;
                AMBRO_ASSERT(mtu_entry.state == OneOf(EntryState::Invalid, EntryState::Unused))
                
                // Remove it from the free list.
                cache->m_mtu_free_list.removeFirst(*cache);
                
                // If it is Unused it is in the index from which it needs to
                // be removed before being re-added with a different address.
                if (mtu_entry.state == EntryState::Unused) {
                    AMBRO_ASSERT(mtu_entry.remote_addr != remote_addr)
                    cache->m_mtu_index.removeEntry(*cache, mtu_ref);
                }
                
                // Setup some fields.
                mtu_entry.state = EntryState::Referenced;
                mtu_entry.remote_addr = remote_addr;
                mtu_entry.mtu = iface->getMtu();
                mtu_entry.minutes_old = 0;
                
                // Add the MtuRef to the index with the new address.
                cache->m_mtu_index.addEntry(*cache, mtu_ref);
                
                // Clear the next link since we are the first node.
                NextLink::link = nullptr;
            }
            
            MtuEntry &mtu_entry = *mtu_ref;
            
            // In all cases link ourselves with the head of the list,
            // so do it here for better code size.
            mtu_entry.refs_list_first.link = NextLink::self();
            PrevLink::link = mtu_entry.refs_list_first.self();
            
            assert_entry_referenced(mtu_entry);
            
            // Return the PMTU and success.
            out_pmtu = mtu_entry.mtu;
            return true;
        }
        
        void moveFrom (MtuRef &src)
        {
            AMBRO_ASSERT(!isSetup())
            
            // If the source is not setup, nothing needs to be done.
            if (!src.isSetup()) {
                return;
            }
            
            // Copy the prev and next links.
            PrevLink::link = src.PrevLink::link;
            NextLink::link = src.NextLink::link;
            
            // Fixup the link from prev (different for first and non-first node).
            if (PrevLink::link->link == src.NextLink::self()) {
                PrevLink::link->link = NextLink::self();
            } else {
                AMBRO_ASSERT(PrevLink::link->link == src.PrevLink::self())
                PrevLink::link->link = PrevLink::self();
            }
            
            // Fixup the link from next if any.
            if (NextLink::link != nullptr) {
                AMBRO_ASSERT(NextLink::link->link == src.NextLink::self())
                NextLink::link->link = NextLink::self();
            }
            
            // Reset the PrevLink in the source make it not-setup.
            src.PrevLink::link = nullptr;
        }
        
    protected:
        // This is called when the PMTU changes.
        // It MUST NOT reset/deinit this or any other MtuRef object!
        // This is because the caller is iterating the linked list
        // of references without considerations for its modification.
        virtual void pmtuChanged (uint16_t pmtu) = 0;
    };
    
private:
    inline static MtuEntry & get_entry_from_first (Link *link)
    {
        return *(MtuEntry *)((char *)link - offsetof(MtuEntry, refs_list_first));
    }
    
    inline static MtuRef & get_ref_from_prev_link (Link *link)
    {
        return static_cast<MtuRef &>(static_cast<PrevLink &>(*link));
    }
    
    inline static MtuRef & get_ref_from_next_link (Link *link)
    {
        return static_cast<MtuRef &>(static_cast<NextLink &>(*link));
    }
    
    inline static void assert_entry_referenced (MtuEntry &mtu_entry)
    {
        AMBRO_ASSERT(mtu_entry.state == EntryState::Referenced)
        AMBRO_ASSERT(mtu_entry.mtu >= MinMTU)
        AMBRO_ASSERT(mtu_entry.refs_list_first.link != nullptr)
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
            m_mtu_index.removeEntry(*this, {mtu_entry, *this});
            mtu_entry.state = EntryState::Invalid;
            // Move to the front of the free list.
            m_mtu_free_list.remove({mtu_entry, *this}, *this);
            m_mtu_free_list.prepend({mtu_entry, *this}, *this);
            return;
        }
        
        // Reset the timeout.
        // Here minutes_old is set to 1 not 0, this will allow the next timeout
        // to occur after exactly MtuTimeoutMinutes. But elsewhere we set
        // minutes_old to zero to ensure that a timeout does not occurs before
        // MtuTimeoutMinutes.
        mtu_entry.minutes_old = 1;
        
        // Find the route to the destination.
        Ip4RouteInfo route_info;
        if (!m_ip_stack->routeIp4(mtu_entry.remote_addr, route_info)) {
            // Couldn't find an interface, will try again next timeout.
        } else {
            // Reset the PMTU to that of the interface.
            mtu_entry.mtu = route_info.iface->getMtu();
            
            // Notify all MtuRef referencing this entry.
            notify_pmtu_changed(mtu_entry);
        }
        
        assert_entry_referenced(mtu_entry);
    }
    
    void notify_pmtu_changed (MtuEntry &mtu_entry)
    {
        AMBRO_ASSERT(mtu_entry.state == EntryState::Referenced)
        
        uint16_t pmtu = mtu_entry.mtu;
        
        MtuRef *ref = &get_ref_from_next_link(mtu_entry.refs_list_first.link);
        while (true) {
            AMBRO_ASSERT(ref->PrevLink::link != nullptr)
            ref->pmtuChanged(pmtu);
            AMBRO_ASSERT(mtu_entry.state == EntryState::Referenced)
            AMBRO_ASSERT(ref->PrevLink::link != nullptr)
            if (ref->NextLink::link == nullptr) {
                break;
            }
            ref = &get_ref_from_prev_link(ref->NextLink::link);
        }
    }
};

APRINTER_ALIAS_STRUCT(IpPathMtuParams, (
    APRINTER_AS_VALUE(size_t, NumMtuEntries),
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
