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

#include <aprinter/meta/Instance.h>
#include <aprinter/meta/ChooseInt.h>
#include <aprinter/base/Preprocessor.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Accessor.h>
#include <aprinter/structure/LinkModel.h>
#include <aprinter/structure/LinkedList.h>
#include <aprinter/structure/OperatorKeyCompare.h>

#include <aipstack/common/Options.h>
#include <aipstack/misc/MinMax.h>
#include <aipstack/misc/NonCopyable.h>
#include <aipstack/misc/OneOf.h>
#include <aipstack/structure/StructureRaiiWrapper.h>
#include <aipstack/proto/IpAddr.h>
#include <aipstack/proto/Ip4Proto.h>
#include <aipstack/platform/PlatformFacade.h>
#include <aipstack/platform/TimerWrapper.h>

namespace AIpStack {

#ifndef DOXYGEN_SHOULD_SKIP_THIS

template <typename Arg>
class IpPathMtuCache;

template <typename Arg>
AIPSTACK_DECL_TIMERS_CLASS(IpPathMtuCacheTimers, typename Arg::PlatformImpl,
                           IpPathMtuCache<Arg>, (MtuTimer))

/**
 * Path MTU cache implementation supporting Path MTU Discovery in @ref IpStack.
 * 
 * The @ref IpStack provides wrappers around the functionality of this class,
 * as such this class and @ref IpPathMtuCache::MtuRef are not directly visible
 * to protocol handlers and other users of @ref IpStack.
 * 
 * @tparam Arg An instantiated @ref IpPathMtuCacheService::Compose template
 *         or a type derived from such. Note that the @ref IpStack actually
 *         performs this instantiation.
 */
template <typename Arg>
class IpPathMtuCache :
    private IpPathMtuCacheTimers<Arg>::Timers,
    private NonCopyable<IpPathMtuCache<Arg>>
{
    APRINTER_USE_TYPES1(Arg, (Params, PlatformImpl, IpStack))
    APRINTER_USE_VALS(Params, (NumMtuEntries, MtuTimeoutMinutes))
    APRINTER_USE_TYPES1(Params, (MtuIndexService))
    
    using Platform = PlatformFacade<PlatformImpl>;    
    APRINTER_USE_TYPES1(Platform, (TimeType))
    
    APRINTER_USE_TYPES1(IpStack, (Iface, Ip4RouteInfo))
    APRINTER_USE_VALS(IpStack, (MinMTU))
    
    static_assert(NumMtuEntries > 0, "");
    static_assert(MtuTimeoutMinutes > 0, "");
    
    AIPSTACK_USE_TIMERS_CLASS(IpPathMtuCacheTimers<Arg>, (MtuTimer))
    
    struct MtuEntry;
    
    // Array index type for MTU entries and null value.
    using MtuIndexType = APrinter::ChooseIntForMax<NumMtuEntries, false>;
    static MtuIndexType const MtuIndexNull = std::numeric_limits<MtuIndexType>::max();
    
    // Link model for MTU entries: array indices.
    struct MtuEntriesAccessor;
    using MtuLinkModel = APrinter::ArrayLinkModelWithAccessor<
        MtuEntry, MtuIndexType, MtuIndexNull, IpPathMtuCache, MtuEntriesAccessor>;
    using MtuLinkModelRef = typename MtuLinkModel::Ref;
    
    // Index data structure for MTU entries by remote address.
    struct MtuIndexAccessor;
    using MtuIndexLookupKeyArg = Ip4Addr;
    struct MtuIndexKeyFuncs;
    APRINTER_MAKE_INSTANCE(MtuIndex, (MtuIndexService::template Index<
        MtuIndexAccessor, MtuIndexLookupKeyArg, MtuIndexKeyFuncs, MtuLinkModel>))
    
    // Linked list data structure for keeping MTU entries in Invalid or Unused,
    // states. The former kind are maintained to be before the latter kind.
    struct MtuFreeListAccessor;
    using MtuFreeList = APrinter::LinkedList<MtuFreeListAccessor, MtuLinkModel, true>;
    
public:
    class MtuRef;
    
private:
    // Timeout period for the MTU timer (one minute).
    static TimeType const MtuTimerTicks = 60.0 * (TimeType)Platform::TimeFreq;
    
    // MTU entry states.
    enum class EntryState {
        // Entry is not valid (not in index, in free list).
        Invalid,
        // Entry is valid and referenced (in index, not in free list).
        Referenced,
        // Entry is valid but not referenced (in index, in free list).
        Unused,
    };
    
    // Each MtuEntry in Referenced states keeps a doubly-linked list
    // of MtuRef objects. We use a slightly special kind of list,
    // where the head of the list (MtuEntry.first_ref) points
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
            Link first_ref;
        };
        uint16_t mtu;
        EntryState state;
        uint8_t minutes_old;
        Ip4Addr remote_addr;
    };
    
    // Node accessors for the data structures.
    struct MtuIndexAccessor : public APRINTER_MEMBER_ACCESSOR(&MtuEntry::index_node) {};
    struct MtuFreeListAccessor :
        public APRINTER_MEMBER_ACCESSOR(&MtuEntry::free_list_node) {};
    
    struct MtuIndexKeyFuncs : public APrinter::OperatorKeyCompare {
        // Returns the key of an MTU entry for the index.
        inline static Ip4Addr GetKeyOfEntry (MtuEntry const &mtu_entry)
        {
            return mtu_entry.remote_addr;
        }
    };
    
private:
    IpStack *m_ip_stack;
    StructureRaiiWrapper<typename MtuIndex::Index> m_mtu_index;
    StructureRaiiWrapper<MtuFreeList> m_mtu_free_list;
    MtuEntry m_mtu_entries[NumMtuEntries];
    
    // Accessor for the m_mtu_entries array.
    struct MtuEntriesAccessor :
        public APRINTER_MEMBER_ACCESSOR(&IpPathMtuCache::m_mtu_entries) {};
    
public:
    IpPathMtuCache (Platform platform, IpStack *ip_stack) :
        IpPathMtuCacheTimers<Arg>::Timers(platform),
        m_ip_stack(ip_stack)
    {
        // Initialize the MTU entries.
        for (MtuEntry &mtu_entry : m_mtu_entries) {
            mtu_entry.state = EntryState::Invalid;
            m_mtu_free_list.append({mtu_entry, *this}, *this);
        }
    }
    
    bool handlePacketTooBig (Ip4Addr remote_addr, uint16_t mtu_info)
    {
        // Find the entry of this address. If it there is none, do nothing.
        MtuLinkModelRef mtu_ref = m_mtu_index.findEntry(remote_addr, *this);
        if (mtu_ref.isNull()) {
            return false;
        }
        
        MtuEntry &mtu_entry = *mtu_ref;
        AMBRO_ASSERT(mtu_entry.state == OneOf(EntryState::Referenced, EntryState::Unused))
        AMBRO_ASSERT(mtu_entry.remote_addr == remote_addr)
        
        // If the ICMP message does not include an MTU (mtu_info==0),
        // we assume the minimum PMTU that we allow. Generally we bump
        // up the reported next link MTU to be no less than our MinMTU.
        // This is what Linux does, it must be good enough for us too.
        uint16_t bump_mtu = MaxValue(MinMTU, mtu_info);
        
        // Make sure the PMTU will not exceed the interface MTU.
        Ip4RouteInfo route_info;
        if (m_ip_stack->routeIp4(remote_addr, route_info)) {
            bump_mtu = MinValue(bump_mtu, route_info.iface->getMtu());
        }
        
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
        private NextLink,
        private NonCopyable<MtuRef>
    {
        friend IpPathMtuCache;
        
    public:
        inline MtuRef ()
        {
            // Clear prev link to indicate unused MtuRef.
            PrevLink::link = nullptr;
        }
        
        inline ~MtuRef ()
        {
            AMBRO_ASSERT(PrevLink::link == nullptr)
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
                // We are the only node in this entry.
                MtuEntry &mtu_entry = get_entry_from_first(PrevLink::link);
                assert_entry_referenced(mtu_entry);
                
                // Transition entry to Unused state.
                mtu_entry.state = EntryState::Unused;
                
                // Add the entry to the end of the Unused list. It is important
                // to add to the end to keep Invalid entries at the front.
                cache->m_mtu_free_list.append({mtu_entry, *cache}, *cache);
            } else {
                // We are not the only node and need to remove ourselves.
                
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
        
        inline bool isSetup () const
        {
            return PrevLink::link != nullptr;
        }
        
        bool setup (IpPathMtuCache *cache, Ip4Addr remote_addr, Iface *iface,
                    uint16_t &out_pmtu)
        {
            AMBRO_ASSERT(!isSetup())
            
            // Lookup this address in the index.
            MtuLinkModelRef mtu_ref = cache->m_mtu_index.findEntry(remote_addr, *cache);
            
            if (!mtu_ref.isNull()) {
                // Got an existing MtuEntry for this address.
                MtuEntry &mtu_entry = *mtu_ref;
                
                if (mtu_entry.state == EntryState::Unused) {
                    // Unused MtuEntry, it needs to become Referenced with this MtuRef
                    // as the single reference.
                    
                    // Remove entry from free list.
                    cache->m_mtu_free_list.remove(mtu_ref, *cache);
                    
                    // Change the entry state from Unused to Referenced.
                    mtu_entry.state = EntryState::Referenced;
                    
                    // Set entry to Referenced state and setup our next link.
                    NextLink::link = nullptr;
                    
                    // Links to/from the head are setup below.
                } else {
                    // Referenced mtu_entry, we need to insert this MtuRef.
                    AMBRO_ASSERT(mtu_entry.state == EntryState::Referenced)
                    AMBRO_ASSERT(mtu_entry.first_ref.link != nullptr)
                    
                    // Get the current first MtuRef which will become the second.
                    MtuRef &next_ref = get_ref_from_next_link(mtu_entry.first_ref.link);
                    AMBRO_ASSERT(next_ref.PrevLink::link == mtu_entry.first_ref.self())
                    
                    // Setup links between this and the former first MtuRef.
                    NextLink::link = next_ref.PrevLink::self();
                    next_ref.PrevLink::link = NextLink::self();
                    
                    // Links to/from the head are setup below.
                }
            } else {
                // There is no MtuEntry for this address, we will try to add one.
                
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
                
                // An MtuEntry on the free list can be in Invalid or Unused state.
                MtuEntry &mtu_entry = *mtu_ref;
                AMBRO_ASSERT(mtu_entry.state ==
                    OneOf(EntryState::Invalid, EntryState::Unused))
                
                // Remove the entry from the free list.
                cache->m_mtu_free_list.removeFirst(*cache);
                
                // If the entry is in Unused state, it is inserted into the index
                // and needs to be removed before being re-inserted with a different
                // address.
                if (mtu_entry.state == EntryState::Unused) {
                    AMBRO_ASSERT(mtu_entry.remote_addr != remote_addr)
                    cache->m_mtu_index.removeEntry(mtu_ref, *cache);
                }
                
                // Setup some fields of the entry.
                mtu_entry.state = EntryState::Referenced;
                mtu_entry.remote_addr = remote_addr;
                mtu_entry.mtu = iface->getMtu();
                mtu_entry.minutes_old = 0;
                
                // Add the entry to the index with the new address.
                cache->m_mtu_index.addEntry(mtu_ref, *cache);
                
                // Make sure the MtuTimer is running, since it would not have been
                // running if we didn't have any non-Invalid timers before.
                if (!cache->tim(MtuTimer()).isSet()) {
                    mtu_entry.minutes_old = 1; // don't waste a minute
                    cache->tim(MtuTimer()).setAfter(MtuTimerTicks);
                }
                
                // Clear the next link since we are the first node.
                NextLink::link = nullptr;
                
                // Links to/from the head are setup below.
            }
            
            MtuEntry &mtu_entry = *mtu_ref;
            
            // In all cases we need to link ourselves with the head of the list.
            mtu_entry.first_ref.link = NextLink::self();
            PrevLink::link = mtu_entry.first_ref.self();
            
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
        return *(MtuEntry *)((char *)link - offsetof(MtuEntry, first_ref));
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
        AMBRO_ASSERT(mtu_entry.first_ref.link != nullptr)
    }
    
    void timerExpired (MtuTimer)
    {
        // Update non-Invalid MTU entries.
        MtuLinkModelRef mtu_ref = m_mtu_index.first(*this);
        while (!mtu_ref.isNull()) {
            // Get next MtuRef first since this one can be removed from the index below.
            MtuEntry &mtu_entry = *mtu_ref;
            mtu_ref = m_mtu_index.next(mtu_ref, *this);
            update_mtu_entry_expiry(mtu_entry);
        }
        
        // Restart the timer if there is any non-Invalid entry left.
        if (!m_mtu_index.first(*this).isNull()) {
            tim(MtuTimer()).setAfter(MtuTimerTicks);
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
        
        // If the entry is unused, just invalidate it.
        if (mtu_entry.state == EntryState::Unused) {
            invalidate_unused_entry(mtu_entry);
            return;
        }
        
        // Reset the timeout. Setting minutes_old to 1 rather than 0 allows the
        // next timeout to occur after exactly MtuTimeoutMinutes.
        mtu_entry.minutes_old = 1;
        
        // Find the route to the destination.
        Ip4RouteInfo route_info;
        if (!m_ip_stack->routeIp4(mtu_entry.remote_addr, route_info)) {
            // Couldn't find an interface, will try again next timeout.
        } else {
            // Reset the PMTU to that of the interface. If we changed the PMTU
            // then notify all MtuRef referencing this entry.
            uint16_t iface_mtu = route_info.iface->getMtu();
            if (mtu_entry.mtu != iface_mtu) {
                mtu_entry.mtu = iface_mtu;
                notify_pmtu_changed(mtu_entry);
            }
        }
        
        assert_entry_referenced(mtu_entry);
    }
    
    void invalidate_unused_entry (MtuEntry &mtu_entry)
    {
        AMBRO_ASSERT(mtu_entry.state == EntryState::Unused)
        
        // Remove the entry from the index.
        m_mtu_index.removeEntry({mtu_entry, *this}, *this);
        
        // Set entry to Invalid state.
        mtu_entry.state = EntryState::Invalid;
        
        // Move the entry to the front of the free list. We maintain all
        // Invalid entries at the front so that taking an Invalid entry is
        // preferred to reusing an Unused entry.
        m_mtu_free_list.remove({mtu_entry, *this}, *this);
        m_mtu_free_list.prepend({mtu_entry, *this}, *this);
    }
    
    void notify_pmtu_changed (MtuEntry &mtu_entry)
    {
        AMBRO_ASSERT(mtu_entry.state == EntryState::Referenced)
        
        // Iterate over all the MtuRef referencing this entry and call
        // their pmtuChanged callbacks. The callbacks must not change
        // any MtuRef's so we do not implement any safety but we do have
        // some asserts to detect violators.
        
        uint16_t pmtu = mtu_entry.mtu;
        
        MtuRef *ref = &get_ref_from_next_link(mtu_entry.first_ref.link);
        
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

#endif

/**
 * Options for @ref IpPathMtuCacheService.
 */
struct IpPathMtuCacheOptions {
    /**
     * Number of PMTU cache entries (must be \>0).
     */
    AIPSTACK_OPTION_DECL_VALUE(NumMtuEntries, size_t, 0)
    
    /**
     * PMTU cache entry timeout in minutes (must be \>0).
     */
    AIPSTACK_OPTION_DECL_VALUE(MtuTimeoutMinutes, uint8_t, 10)
    
    /**
     * Data structure service for indexing PMTU cache entries by IP address.
     * 
     * This should be one of the implementations in aprinter/structure/index.
     * Specifically supported are AvlTreeIndexService and MruListIndexService.
     */
    AIPSTACK_OPTION_DECL_TYPE(MtuIndexService, void)
};

/**
 * Service definition for the IP Path MTU cache implementation.
 * 
 * An instantiation of this template must be passed to @ref IpStackService.
 * 
 * The template parameters are assignments of options defined in
 * @ref IpPathMtuCacheOptions, for example:
 * AIpStack::IpPathMtuCacheOptions::NumMtuEntries::Is\<100\>.
 * 
 * @tparam Options Assignments of options defined in @ref IpPathMtuCacheOptions.
 */
template <typename... Options>
class IpPathMtuCacheService {
    template <typename>
    friend class IpPathMtuCache;
    
    AIPSTACK_OPTION_CONFIG_VALUE(IpPathMtuCacheOptions, NumMtuEntries)
    AIPSTACK_OPTION_CONFIG_VALUE(IpPathMtuCacheOptions, MtuTimeoutMinutes)
    AIPSTACK_OPTION_CONFIG_TYPE(IpPathMtuCacheOptions, MtuIndexService)
    
public:
#ifndef DOXYGEN_SHOULD_SKIP_THIS
    template <typename PlatformImpl_, typename IpStack_>
    struct Compose {
        using PlatformImpl = PlatformImpl_;
        using IpStack = IpStack_;
        using Params = IpPathMtuCacheService;
        APRINTER_DEF_INSTANCE(Compose, IpPathMtuCache)        
    };
#endif
};

}

#endif
