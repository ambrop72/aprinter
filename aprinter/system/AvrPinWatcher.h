/*
 * Copyright (c) 2013 Ambroz Bizjak
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

#ifndef AMBROLIB_AVR_PIN_WATCHER_H
#define AMBROLIB_AVR_PIN_WATCHER_H

#include <avr/io.h>
#include <avr/interrupt.h>

#include <aprinter/meta/Tuple.h>
#include <aprinter/meta/TupleFetch.h>
#include <aprinter/meta/TupleForEach.h>
#include <aprinter/meta/MapTypeList.h>
#include <aprinter/meta/TemplateFunc.h>
#include <aprinter/meta/TypesAreEqual.h>
#include <aprinter/meta/FilterTypeList.h>
#include <aprinter/meta/HasMemberTypeFunc.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/GetContainer.h>
#include <aprinter/base/OffsetCallback.h>
#include <aprinter/structure/SingleEndedList.h>
#include <aprinter/system/AvrPins.h>
#include <aprinter/system/AvrIo.h>
#include <aprinter/system/AvrLock.h>

#include <aprinter/BeginNamespace.h>

template <typename, typename, typename>
class AvrPinWatcher;

template <typename Context>
class AvrPinWatcherService
: private DebugObject<Context, AvrPinWatcherService<Context>>
{
    typedef typename Context::EventLoop Loop;
    
public:
    void init (Context c)
    {
        PCICR = 0;
        TupleForEach<Tuple<PortStateTypes>>::call_forward(&m_port_states, InitPortHelper(), c, this);
        
        this->debugInit(c);
    }
    
    void deinit (Context c)
    {
        this->debugDeinit(c);
        
        PCICR = 0;
        TupleForEach<Tuple<PortStateTypes>>::call_reverse(&m_port_states, DeinitPortHelper(), c, this);
    }
    
    template <typename Port>
    void pcint_isr (AvrInterruptContext<Context> c)
    {
        PortState<Port> *ps = getPortState<Port>();
        ps->queued_event.appendNow(c);
    }
    
    template <typename Pin, typename Handler>
    using PinWatcher = AvrPinWatcher<Context, Pin, Handler>;
    
private:
    template <typename, typename, typename>
    friend class AvrPinWatcher;
    
    template <typename Port>
    struct WatcherBase {
        SingleEndedListNode<WatcherBase> watchers_list_node;
        typename Loop::QueuedEvent pending;
    };
    
    template <typename TPort>
    struct PortState {
        typedef TPort Port;
        
        void port_queued_event_handler (Context c)
        {
            AvrPinWatcherService *o = GetContainer(TupleFetch<Tuple<PortStateTypes>, PortState>::getFromElem(this), &AvrPinWatcherService::m_port_states);
            o->template queued_event_handler<Port>(c);
        }
        
        typename Loop::QueuedEvent queued_event;
        SingleEndedList<WatcherBase<Port>, &WatcherBase<Port>::watchers_list_node> watchers_list;
    };
    
    AMBRO_DECLARE_HAS_MEMBER_TYPE_FUNC(HasPinChangeTagFunc, PinChangeTag)
    
    using Ports = FilterTypeList<AvrPorts, HasPinChangeTagFunc>;
    
    typedef MapTypeList<Ports, TemplateFunc<PortState>> PortStateTypes;
    
    struct InitPortHelper {
        template <typename PortStateType>
        void operator() (PortStateType *ps, Context c, AvrPinWatcherService *o)
        {
            ps->queued_event.init(c, AMBRO_OFFSET_CALLBACK_T(&PortStateType::queued_event, &PortStateType::port_queued_event_handler));
            ps->watchers_list.init();
            avrSetReg<PortStateType::Port::pcmsk_io_addr>(0);
            PCICR |= (1 << PortStateType::Port::pcie_bit);
        }
    };
    
    struct DeinitPortHelper {
        template <typename PortStateType>
        void operator() (PortStateType *ps, Context c, AvrPinWatcherService *o)
        {
            AMBRO_ASSERT(ps->watchers_list.isEmpty())
            ps->queued_event.deinit(c);
        }
    };
    
    template <typename Port>
    PortState<Port> * getPortState ()
    {
        return TupleFetch<Tuple<PortStateTypes>, PortState<Port>>::getElem(&m_port_states);
    }
    
    template <typename Port>
    void queued_event_handler (Context c)
    {
        this->debugAccess(c);
        PortState<Port> *ps = getPortState<Port>();
        
        for (WatcherBase<Port> *base = ps->watchers_list.first(); base; base = ps->watchers_list.next(base)) {
            base->pending.prependNow(c);
        }
    }
    
    Tuple<PortStateTypes> m_port_states;
};

template <typename Context, typename Pin, typename Handler>
class AvrPinWatcher
: private DebugObject<Context, AvrPinWatcher<Context, Pin, Handler>>
{
private:
    typedef typename Context::EventLoop Loop;
    typedef AvrPinWatcherService<Context> Service;
    typedef typename Pin::Port Port;
    static_assert(Service::HasPinChangeTagFunc::template Call<Port>::Type::value, "Port of this pin does not support pin change interrupts.");
    typedef typename Service::template PortState<Port> PortState;
    typedef typename Service::template WatcherBase<Port> WatcherBase;

public:
    void init (Context c)
    {
        Service *srv = c.pinWatcherService();
        PortState *ps = srv->template getPortState<Port>();
        AMBRO_ASSERT(!(avrGetReg<Port::pcmsk_io_addr>() & (1 << Pin::port_pin)))
        
        ps->watchers_list.prepend(&m_base);
        m_base.pending.init(c, AMBRO_OFFSET_CALLBACK2_T(&AvrPinWatcher::m_base, &WatcherBase::pending, &AvrPinWatcher::pending_handler));
        m_base.pending.prependNow(c);
        avrSoftSetBitReg<Port::pcmsk_io_addr>(Pin::port_pin);
        
        this->debugInit(c);
    }
    
    void deinit (Context c)
    {
        this->debugDeinit(c);
        Service *srv = c.pinWatcherService();
        PortState *ps = srv->template getPortState<Port>();
        
        avrSoftClearBitReg<Port::pcmsk_io_addr>(Pin::port_pin);
        m_base.pending.deinit(c);
        ps->watchers_list.remove(&m_base);
    }
    
private:
    void pending_handler (Context c)
    {
        this->debugAccess(c);
        
        bool state = c.pins()->template get<Pin>(c);
        Handler::call(this, c, state);
    }
    
    WatcherBase m_base;
};

#ifdef PCMSK0
#define AMBRO_AVR_PCMSK0_ISRS(service, context) \
ISR(PCINT0_vect) \
{ \
    (service).pcint_isr<AvrPortA>(MakeAvrInterruptContext(context)); \
}
#else
#define AMBRO_AVR_PCMSK0_ISRS(service, context)
#endif

#ifdef PCMSK1
#define AMBRO_AVR_PCMSK1_ISRS(service, context) \
ISR(PCINT1_vect) \
{ \
    (service).pcint_isr<AvrPortB>(MakeAvrInterruptContext(context)); \
}
#else
#define AMBRO_AVR_PCMSK1_ISRS(service, context)
#endif

#ifdef PCMSK2
#define AMBRO_AVR_PCMSK2_ISRS(service, context) \
ISR(PCINT2_vect) \
{ \
    (service).pcint_isr<AvrPortC>(MakeAvrInterruptContext(context)); \
}
#else
#define AMBRO_AVR_PCMSK2_ISRS(service, context)
#endif

#ifdef PCMSK3
#define AMBRO_AVR_PCMSK3_ISRS(service, context) \
ISR(PCINT3_vect) \
{ \
    (service).pcint_isr<AvrPortD>(MakeAvrInterruptContext(context)); \
}
#else
#define AMBRO_AVR_PCMSK3_ISRS(service, context)
#endif

#ifdef PCMSK4
#define AMBRO_AVR_PCMSK4_ISRS(service, context) \
ISR(PCINT4_vect) \
{ \
    (service).pcint_isr<AvrPortE>(MakeAvrInterruptContext(context)); \
}
#else
#define AMBRO_AVR_PCMSK4_ISRS(service, context)
#endif

#define AMBRO_AVR_PIN_WATCHER_ISRS(service, context) \
AMBRO_AVR_PCMSK0_ISRS(service, context) \
AMBRO_AVR_PCMSK1_ISRS(service, context) \
AMBRO_AVR_PCMSK2_ISRS(service, context) \
AMBRO_AVR_PCMSK3_ISRS(service, context) \
AMBRO_AVR_PCMSK4_ISRS(service, context)

#include <aprinter/EndNamespace.h>

#endif
