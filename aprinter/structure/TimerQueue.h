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

#ifndef APRINTER_TIMER_QUEUE_H
#define APRINTER_TIMER_QUEUE_H

#include <type_traits>
#include <limits>

#include <aprinter/base/Assert.h>
#include <aprinter/base/Accessor.h>
#include <aprinter/base/Preprocessor.h>
#include <aprinter/base/Hints.h>
#include <aprinter/structure/TreeCompare.h>

namespace APrinter {

template <typename, typename, typename, typename, typename>
class TimerQueue;

template <
    typename TimersStructureService,
    typename LinkModel,
    typename TimeType,
    typename NodeUserData
>
class TimerQueueNode : public NodeUserData
{
    template <typename, typename, typename, typename, typename>
    friend class TimerQueue;
    
    // Get the Node type for the data structure.
    using TimersStructureNode = typename TimersStructureService::template Node<LinkModel>;
    
private:
    // Node in the data structure.
    TimersStructureNode timers_structure_node;
    
    // Expiration time, relevant only if inserted to the data structure.
    TimeType time;
};

template <
    typename TimersStructureService,
    typename LinkModel,
    typename Accessor,
    typename TimeType,
    typename NodeUserData
>
class TimerQueue
{
    // TimeType must be an unsigned integer type.
    static_assert(std::is_arithmetic<TimeType>::value, "");
    static_assert(std::is_unsigned<TimeType>::value, "");
    
    // Get the State and Ref types from the link model.
    APRINTER_USE_TYPES1(LinkModel, (State, Ref))
    
    // Get the TimerQueueNode type.
    using Node = TimerQueueNode<TimersStructureService, LinkModel, TimeType, NodeUserData>;
    
    // Create the accessor type for TimerQueueNode::timers_structure_node.
    struct TimersStructureNodeAccessor : public APrinter::ComposedAccessor<
        Accessor, APRINTER_MEMBER_ACCESSOR_TN(&Node::timers_structure_node)> {};
    
    // Define the comparator for comparing timers, based on TreeCompare.
    class KeyFuncs;
    struct TimerCompare : public APrinter::TreeCompare<LinkModel, KeyFuncs> {};
    
    // Get the data structure type for finding the earliest timers (e.g. LinkedHeap).
    using TimersStructure = typename TimersStructureService::template Structure<
        TimersStructureNodeAccessor, TimerCompare, LinkModel>;
    
    // Get the TimeType value for the most singificant bit.
    // This is also exactly half of the TimeType type range.
    static TimeType const TimeMsb = (std::numeric_limits<TimeType>::max() / 2) + 1;
    
    // Maximum time in the future (relative to the reference time) that
    // getFirstTime returns. This limit avoids problems if we have timers
    // expiring near the end of the future span of the reference time.
    static TimeType const TimeMaxFutureInterval = TimeMsb / 2 + TimeMsb / 4;
    
private:
    // Data structure instance.
    TimersStructure m_timers_structure;
    
    // Time relative to which all timers in the data structure are in the future.
    // That is, for all timers !time_less(timer_time, reference_time) holds.
    // Generally undefined/irrelevant if the data structure is empty, except just
    // after updateReferenceTime or prepareForRemovingExpired which is required
    // before inserting timers.
    TimeType m_referenece_time;
    
public:
    void init ()
    {
        // Initialize the data structure.
        m_timers_structure.init();
        
        // The reference time is left uninitialized, we would not read it
        // uninitialized if the API is used correctly.
    }
    
    void updateReferenceTime (TimeType now, State st = State())
    {
        // If the timers structure is empty, set the reference time to now,
        // This is the optimal thing to in this case as we do not have any
        // active timers to worry about.
        if (m_timers_structure.isEmpty()) {
            m_referenece_time = now;
        }
        // Otherwise, if the current time is in the past range of the reference
        // time (due to a sudden clock jump), we cannot do anything.
        // The next prepareForRemovingExpired will resolve the situation.
        else if (time_less(now, m_referenece_time)) {
            // Nothing.
        }
        // Otherwise, set the reference time to the smaller of the current time
        // and the first timeout. Limiting the bump bump to the first timer prevents
        // any timer from falling behind the reference time.
        else {
            Ref first_timer = m_timers_structure.first(st);
            TimeType first_time = ac(first_timer).time;
            m_referenece_time = time_less(now, first_time) ? now : first_time;
        }
    }
    
    // NOTE: updateReferenceTime or prepareForRemovingExpiredmmust have been called
    // soon before insert before any significant time could have passed (it is okay
    // to call it once then do multiple insertions).
    void insert (Ref entry, TimeType time, State st = State())
    {
        // If the time time is in the past relative to the reference time, bump
        // it up to the reference time. This is needed because time comparisons
        // work correctly only when the active timers span less than half of the
        // TimeType range.
        if (time_less(time, m_referenece_time)) {
            time = m_referenece_time;
        }
        
        // Store the expiration time.
        ac(entry).time = time;
        
        // Insert the entry to the data structure.
        m_timers_structure.insert(entry, st);
    }
    
    void remove (Ref entry, State st = State())
    {
        // Remove the entry from the data structure.
        m_timers_structure.remove(entry, st);
    }
    
    void prepareForRemovingExpired (TimeType now, State st = State())
    {
        // Timer updates are only needed if there are any timers, and we should
        // not read the reference time in this case as it may be uninitialized.
        if (!m_timers_structure.isEmpty()) {
            // Determine the time based on which we consider timers expired.
            // The first case will dispatches all timers, after the clock jumped into
            // past relative to the reference time, because in this case comparing
            // timers to "now" the usual way may yield incorrect results.
            TimeType dispatch_time;
            if (AMBRO_UNLIKELY(time_less(now, m_referenece_time))) {
                dispatch_time = m_referenece_time + (TimeMsb - 1);
            } else {
                dispatch_time = now;
            }
            
            // Set the time of expired timers to now. This allows us to safely update
            // the reference time before dispatching any timer, and effectively allows
            // new timers to be started in the full future relative to now.
            // NOTE: While we are doing this, comparisons of timers to each other may
            // return wrong results (in case we decided to dispatch all timers above).
            // But the data structure must be designed so that it does not compare
            // timers between each other in this process.
            m_timers_structure.findAllLesserOrEqual(dispatch_time, [&](Ref entry) {
                ac(entry).time = now;
            }, st);
            
            // Call assertValidHeap so the data structure can now verify
            // self-consistency if configured to do so.
            m_timers_structure.assertValidHeap(st);
        }
        
        // Update the reference time to 'now'. This is safe because the above
        // updates ensured that all active timers (including ones about to be
        // dispatched) belong to the future range relative to 'now'.
        m_referenece_time = now;
    }
    
    // NOTE: prepareForRemovingExpired must be called before calling this.
    // It is okay to call that once then call removeExpired multiple times
    // (typically until null is returned).
    Ref removeExpired (State st = State())
    {
        // Get the first timer if any.
        Ref entry = m_timers_structure.first(st);
        
        // If there are no timers or the first timer is not expired, return null.
        if (entry.isNull() || ac(entry).time != m_referenece_time) {
            return Ref::null();
        }
        
        // Remove the timer from the data structure.
        m_timers_structure.remove(entry, st);
        
        // Return the timer which was removed.
        return entry;
    }
    
    inline bool getFirstTime (TimeType &out_time, State st = State())
    {
        // If there are no active timers, return false.
        Ref entry = m_timers_structure.first(st);
        if (entry.isNull()) {
            return false;
        }
        
        // Get the expiration time of the earliest time.
        TimeType time = ac(entry).time;
        
        // If this is greater than TimeMaxFutureInterval after the reference time,
        // bump it down to avoid returning times too far in the future.
        TimeType max_time = m_referenece_time + TimeMaxFutureInterval;
        if (time_less(max_time, time)) {
            time = max_time;
        }
        
        // Return this time.
        out_time = time;
        return true;
    }
    
private:
    // Get the Node of an entry given by a Ref.
    inline static Node & ac (Ref ref)
    {
        return Accessor::access(*ref);
    }
    
    // Compare to timers to determine if one is less than the other.
    // Note that in a set of times, this only gives consistent results
    // if the times span less than half of the TimeType range.
    inline static bool time_less (TimeType time1, TimeType time2)
    {
        return (TimeType)(time1 - time2) >= TimeMsb;
        //return ((TimeType)(time1 - time2) & TimeMsb) != 0;
    }
    
    class KeyFuncs
    {
    public:
        // Get the key (timer time) of an entry.
        template <typename EntryType>
        static TimeType GetKeyOfEntry (EntryType &entry)
        {
            return Accessor::access(entry).time;
        }
        
        // Compare two keys (times).
        static int CompareKeys (TimeType time1, TimeType time2)
        {
            return time_less(time1, time2) ? -1 : (time1 == time2) ? 0 : 1;
        }
    };
};

template <typename TimersStructureService>
struct TimerQueueService {
    template <typename LinkModel, typename TimeType, typename NodeUserData>
    using Node = TimerQueueNode<TimersStructureService, LinkModel, TimeType, NodeUserData>;
    
    template <typename LinkModel, typename Accessor,
              typename TimeType, typename NodeUserData>
    using Queue = TimerQueue<TimersStructureService, LinkModel, Accessor,
                             TimeType, NodeUserData>;
};

}

#endif
