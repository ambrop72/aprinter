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

#ifndef AMBROLIB_STEPPERS_H
#define AMBROLIB_STEPPERS_H

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
#include <aprinter/meta/WrapMember.h>
#include <aprinter/base/DebugObject.h>

#include <aprinter/BeginNamespace.h>

template <typename TDirPin, typename TStepPin, typename TEnablePin, bool TInvertDir>
struct StepperDef {
    using DirPin = TDirPin;
    using StepPin = TStepPin;
    using EnablePin = TEnablePin;
    static const bool InvertDir = TInvertDir;
};

template <typename Context, typename StepperDefsList, int DefIndex>
class SteppersStepper;

template <typename Context, typename StepperDefsList>
class Steppers : private DebugObject<Context, Steppers<Context, StepperDefsList>> {
private:
    template <typename, typename, int>
    friend class SteppersStepper;
    
    template <typename Index>
    using StepperByIndex = SteppersStepper<Context, StepperDefsList, Index::value>;
    
    using SteppersTuple = Tuple<
        MapTypeList<
            SequenceList<
                TypeListLength<StepperDefsList>::value
            >,
            TemplateFunc<StepperByIndex>
        >
    >;
    
    struct SteppersInitHelper {
        template <typename ThisStepperEntry>
        void operator() (ThisStepperEntry *se, Context c)
        {
            se->init(c);
        }
    };
    
    struct SteppersDeinitHelper {
        template <typename ThisStepperEntry>
        void operator() (ThisStepperEntry *se, Context c)
        {
            se->deinit(c);
        }
    };
    
public:
    void init (Context c)
    {
        TupleForEach<SteppersTuple>::call_forward(&m_steppers, SteppersInitHelper(), c);
        this->debugInit(c);
    }
    
    void deinit (Context c)
    {
        this->debugDeinit(c);
        TupleForEach<SteppersTuple>::call_reverse(&m_steppers, SteppersDeinitHelper(), c);
    }
    
    template <int StepperIndex>
    SteppersStepper<Context, StepperDefsList, StepperIndex> * getStepper ()
    {
        return TupleGet<SteppersTuple, StepperIndex>::getElem(&m_steppers);
    }
    
private:
    SteppersTuple m_steppers;
};

template <typename Context, typename StepperDefsList, int StepperIndex>
class SteppersStepper {
public:
    template <typename ThisContext>
    void enable (ThisContext c, bool e)
    {
        parent()->debugAccess(c);
        m_enabled = e;
        bool pin_enabled = RuntimeListFold<OrEnableOper, SameEnableIndices>::call(&parent()->m_steppers);
        c.pins()->template set<EnablePin>(c, !pin_enabled);
    }
    
    template <typename ThisContext>
    void setDir (ThisContext c, bool dir)
    {
        parent()->debugAccess(c);
        c.pins()->template set<DirPin>(c, maybe_invert_dir(dir));
    }
    
    template <typename ThisContext>
    void step (ThisContext c)
    {
        parent()->debugAccess(c);
        c.pins()->template set<StepPin>(c, true);
        c.pins()->template set<StepPin>(c, false);
    }
    
    static void emergency ()
    {
        Context::Pins::template emergencySet<EnablePin>(true);
    }
    
private:
    template <typename, typename>
    friend class Steppers;
    template <typename, typename, int>
    friend class SteppersStepper;
    
    using OurSteppers = Steppers<Context, StepperDefsList>;
    using SteppersTuple = typename OurSteppers::SteppersTuple;
    
    static bool maybe_invert_dir (bool dir)
    {
        return (ThisDef::InvertDir) ? !dir : dir;
    }
    
    void init (Context c)
    {
        m_enabled = false;
        c.pins()->template set<DirPin>(c, maybe_invert_dir(false));
        c.pins()->template set<StepPin>(c, false);
        c.pins()->template set<EnablePin>(c, true);
        c.pins()->template setOutput<DirPin>(c);
        c.pins()->template setOutput<StepPin>(c);
        c.pins()->template setOutput<EnablePin>(c);
    }
    
    void deinit (Context c)
    {
        c.pins()->template set<EnablePin>(c, true);
    }
    
    OurSteppers * parent ()
    {
        SteppersTuple *steppers = TupleGet<SteppersTuple, StepperIndex>::getFromElem(this);
        return AMBRO_WMEMB_TD(&OurSteppers::m_steppers)::container(steppers);
    }
    
    using ThisDef = TypeListGet<StepperDefsList, StepperIndex>;
    using DirPin = typename ThisDef::DirPin;
    using StepPin = typename ThisDef::StepPin;
    using EnablePin = typename ThisDef::EnablePin;
    
    AMBRO_DECLARE_GET_MEMBER_TYPE_FUNC(GetEnablePinFunc, EnablePin)
    
    using SameEnableIndices = FilterTypeList<
        SequenceList<
            TypeListLength<StepperDefsList>::value
        >,
        ComposeFunctions<
            IsEqualFunc<EnablePin>,
            ComposeFunctions<
                GetEnablePinFunc,
                TypeListGetFunc<StepperDefsList>
            >
        >
    >;
    
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
    
    bool m_enabled;
};

#include <aprinter/EndNamespace.h>

#endif
