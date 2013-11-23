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
#include <aprinter/meta/TupleGet.h>
#include <aprinter/meta/TupleForEach.h>
#include <aprinter/meta/TypeListGet.h>
#include <aprinter/meta/FilterTypeList.h>
#include <aprinter/meta/IsEqualFunc.h>
#include <aprinter/meta/ComposeFunctions.h>
#include <aprinter/meta/GetMemberTypeFunc.h>
#include <aprinter/meta/SequenceList.h>
#include <aprinter/meta/TypeListLength.h>
#include <aprinter/meta/IndexElemTuple.h>
#include <aprinter/meta/Position.h>
#include <aprinter/meta/TuplePosition.h>
#include <aprinter/meta/MapTypeList.h>
#include <aprinter/meta/ValueTemplateFunc.h>
#include <aprinter/base/DebugObject.h>

#include <aprinter/BeginNamespace.h>

template <typename TDirPin, typename TStepPin, typename TEnablePin, bool TInvertDir>
struct StepperDef {
    using DirPin = TDirPin;
    using StepPin = TStepPin;
    using EnablePin = TEnablePin;
    static const bool InvertDir = TInvertDir;
};

template <typename Position, typename Context, typename StepperDefsList>
class Steppers : private DebugObject<Context, void> {
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_init, init)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_deinit, deinit)
    AMBRO_DECLARE_TUPLE_FOREACH_HELPER(Foreach_or_enabled, or_enabled)
    
    template <int StepperIndex> struct StepperPosition;
    
    static Steppers * self (Context c)
    {
        return PositionTraverse<typename Context::TheRootPosition, Position>(c.root());
    }
    
public:
    template <int StepperIndex>
    class Stepper {
    public:
        template <typename ThisContext>
        static void enable (ThisContext c, bool e)
        {
            Stepper *o = self(c);
            Steppers *s = Steppers::self(c);
            s->debugAccess(c);
            o->m_enabled = e;
            SameEnabledTuple dummy;
            bool pin_enabled = TupleForEachForwardAccRes(&dummy, false, Foreach_or_enabled(), c);
            c.pins()->template set<typename ThisDef::EnablePin>(c, !pin_enabled);
        }
        
        template <typename ThisContext>
        static void setDir (ThisContext c, bool dir)
        {
            Steppers *s = Steppers::self(c);
            s->debugAccess(c);
            c.pins()->template set<typename ThisDef::DirPin>(c, maybe_invert_dir(dir));
        }
        
        template <typename ThisContext>
        static void stepOn (ThisContext c)
        {
            Steppers *s = Steppers::self(c);
            s->debugAccess(c);
            c.pins()->template set<typename ThisDef::StepPin>(c, true);
        }
        
        template <typename ThisContext>
        static void stepOff (ThisContext c)
        {
            Steppers *s = Steppers::self(c);
            s->debugAccess(c);
            c.pins()->template set<typename ThisDef::StepPin>(c, false);
        }
        
        static void emergency ()
        {
            Context::Pins::template emergencySet<typename ThisDef::EnablePin>(true);
        }
        
    public: // private, workaround gcc bug
        friend Steppers;
        
        AMBRO_DECLARE_GET_MEMBER_TYPE_FUNC(GetMemberType_EnablePin, EnablePin)
        
        using ThisDef = TypeListGet<StepperDefsList, StepperIndex>;
        using SameEnableIndices = FilterTypeList<
            SequenceList<
                TypeListLength<StepperDefsList>::value
            >,
            ComposeFunctions<
                IsEqualFunc<typename ThisDef::EnablePin>,
                ComposeFunctions<
                    GetMemberType_EnablePin,
                    TypeListGetFunc<StepperDefsList>
                >
            >
        >;
        using SameEnabledTuple = Tuple<MapTypeList<SameEnableIndices, ValueTemplateFunc<int, Stepper>>>;
        
        static Stepper * self (Context c)
        {
            return PositionTraverse<typename Context::TheRootPosition, StepperPosition<StepperIndex>>(c.root());
        }
        
        static bool maybe_invert_dir (bool dir)
        {
            return (ThisDef::InvertDir) ? !dir : dir;
        }
        
        static void init (Context c)
        {
            Stepper *o = self(c);
            o->m_enabled = false;
            c.pins()->template set<typename ThisDef::DirPin>(c, maybe_invert_dir(false));
            c.pins()->template set<typename ThisDef::StepPin>(c, false);
            c.pins()->template set<typename ThisDef::EnablePin>(c, true);
            c.pins()->template setOutput<typename ThisDef::DirPin>(c);
            c.pins()->template setOutput<typename ThisDef::StepPin>(c);
            c.pins()->template setOutput<typename ThisDef::EnablePin>(c);
        }
        
        static void deinit (Context c)
        {
            c.pins()->template set<ThisDef::EnablePin>(c, true);
        }
        
        static bool or_enabled (bool accum, Context c)
        {
            Stepper *o = self(c);
            return (accum || o->m_enabled);
        }
        
        bool m_enabled;
    };
    
    static void init (Context c)
    {
        Steppers *o = self(c);
        TupleForEachForward(&o->m_steppers, Foreach_init(), c);
        o->debugInit(c);
    }
    
    static void deinit (Context c)
    {
        Steppers *o = self(c);
        o->debugDeinit(c);
        TupleForEachForward(&o->m_steppers, Foreach_deinit(), c);
    }
    
    template <int StepperIndex>
    static Stepper<StepperIndex> * getStepper (Context c)
    {
        Steppers *o = self(c);
        return TupleGetElem<StepperIndex>(&o->m_steppers);
    }
    
private:
    using SteppersTuple = IndexElemTuple<StepperDefsList, Stepper>;
    
    SteppersTuple m_steppers;
    
    template <int StepperIndex> struct StepperPosition : public TuplePosition<Position, SteppersTuple, &Steppers::m_steppers, StepperIndex> {};
};

#include <aprinter/EndNamespace.h>

#endif
