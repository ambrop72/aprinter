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

#ifndef AMBROLIB_STEPPER_SYNCHRONIZER_H
#define AMBROLIB_STEPPER_SYNCHRONIZER_H

#include <aprinter/meta/TypeList.h>
#include <aprinter/meta/Tuple.h>
#include <aprinter/meta/TemplateFunc.h>
#include <aprinter/meta/MapTypeList.h>
#include <aprinter/meta/TupleGet.h>
#include <aprinter/meta/TupleForEach.h>
#include <aprinter/meta/TypeListGet.h>
#include <aprinter/meta/FilterTypeList.h>
#include <aprinter/meta/IsEqualFunc.h>
#include <aprinter/meta/ComposeFunctions.h>
#include <aprinter/meta/GetMemberTypeFunc.h>
#include <aprinter/meta/SequenceList.h>
#include <aprinter/meta/TypeListLength.h>
#include <aprinter/meta/RuntimeListFold.h>
#include <aprinter/devices/Stepper.h>

#include <aprinter/BeginNamespace.h>

template<class List >
class Tuple;
template <typename TDirPin, typename TStepPin, typename TEnablePin>
struct StepperDef {
    typedef TDirPin DirPin;
    typedef TStepPin StepPin;
    typedef TEnablePin EnablePin;
};

template <typename Context, typename StepperDefsList>
class StepperSynchronizer {
public:
    template <typename DefIndex>
    class StepperEntry;
    
private:
    AMBRO_DECLARE_GET_MEMBER_TYPE_FUNC(GetEnablePinFunc, EnablePin)
    
    template <typename EnablePin>
    using SameEnableIndices = typename FilterTypeList<
        typename SequenceList<
            TypeListLength<StepperDefsList>::value
        >::Type,
        ComposeFunctions<
            IsEqualFunc<EnablePin>,
            ComposeFunctions<
                GetEnablePinFunc,
                TypeListGetFunc<StepperDefsList>
            >
        >
    >::Type;
    
    typedef Tuple<
        typename MapTypeList<
            typename SequenceList<
                TypeListLength<StepperDefsList>::value
            >::Type,
            TemplateFunc<StepperEntry>
        >::Type
    > SteppersTuple;
    
    struct OrEnableOper {
        static bool zero (SteppersTuple *steppers)
        {
            return false;
        }
        
        template <typename Index>
        static bool combine (bool x, SteppersTuple *steppers)
        {
            return (x || TupleGet<SteppersTuple, Index::value>::getElem(steppers)->m_enabled);
        }
    };
    
    struct SteppersInitHelper {
        template <typename ThisStepperEntry>
        void operator() (ThisStepperEntry *se, Context c)
        {
            se->m_enabled = false;
            se->m_stepper.init(c);
            c.pins()->template setOutput<typename ThisStepperEntry::EnablePin>(c);
            c.pins()->template set<typename ThisStepperEntry::EnablePin>(c, true);
        }
    };
    
    struct SteppersDeinitHelper {
        template <typename ThisStepperEntry>
        void operator() (ThisStepperEntry *se, Context c)
        {
            c.pins()->template set<typename ThisStepperEntry::EnablePin>(c, true);
            se->m_stepper.deinit(c);
        }
    };
    
public:
    template <typename DefIndex>
    class StepperEntry {
    public:
        template <typename ThisContext>
        void enable (ThisContext c, bool e)
        {
            m_enabled = e;
            SteppersTuple *steppers = TupleGet<SteppersTuple, DefIndex::value>::getFromElem(this);
            bool pin_enabled = RuntimeListFold<MySameEnableIndices>::template call<OrEnableOper>(steppers);
            c.pins()->template set<EnablePin>(c, !pin_enabled);
        }
        
        template <typename ThisContext>
        void setDir (ThisContext c, bool dir)
        {
            m_stepper.setDir(c, dir);
        }
        
        template <typename ThisContext>
        void step (ThisContext c)
        {
            m_stepper.step(c);
        }
        
    private:
        friend class StepperSynchronizer;
        
        typedef typename TypeListGet<StepperDefsList, DefIndex::value>::Type ThisDef;
        typedef typename ThisDef::DirPin DirPin;
        typedef typename ThisDef::StepPin StepPin;
        typedef typename ThisDef::EnablePin EnablePin;
        typedef Stepper<Context, DirPin, StepPin> ThisStepper;
        typedef SameEnableIndices<EnablePin> MySameEnableIndices;
        
        bool m_enabled;
        ThisStepper m_stepper;
    };
    
    template <int StepperIndex>
    using StepperType = StepperEntry<WrapInt<StepperIndex>>;
    
    void init (Context c)
    {
        TupleForEach<SteppersTuple>::call_forward(&m_steppers, SteppersInitHelper(), c);
    }
    
    void deinit (Context c)
    {
        TupleForEach<SteppersTuple>::call_reverse(&m_steppers, SteppersDeinitHelper(), c);
    }
    
    template <int StepperIndex>
    StepperType<StepperIndex> * getStepper ()
    {
        return TupleGet<SteppersTuple, StepperIndex>::getElem(&m_steppers);
    }
    
private:
    SteppersTuple m_steppers;
};

#include <aprinter/EndNamespace.h>

#endif
