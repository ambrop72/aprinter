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
#include <aprinter/meta/TypeListUtils.h>
#include <aprinter/meta/FuncUtils.h>
#include <aprinter/meta/MemberType.h>
#include <aprinter/meta/ChooseInt.h>
#include <aprinter/meta/BasicMetaUtils.h>
#include <aprinter/meta/ListForEach.h>
#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Hints.h>
#include <aprinter/printer/Configuration.h>

#include <aprinter/BeginNamespace.h>

APRINTER_ALIAS_STRUCT(StepperDef, (
    APRINTER_AS_TYPE(DirPin),
    APRINTER_AS_TYPE(StepPin),
    APRINTER_AS_TYPE(EnablePin),
    APRINTER_AS_VALUE(bool, StepLevel),
    APRINTER_AS_VALUE(bool, EnableLevel),
    APRINTER_AS_TYPE(InvertDir)
))

template <typename Arg>
class Steppers {
    using Context         = typename Arg::Context;
    using ParentObject    = typename Arg::ParentObject;
    using Config          = typename Arg::Config;
    using StepperDefsList = typename Arg::StepperDefsList;
    
public:
    struct Object;
    
private:
    static int const NumSteppers = TypeListLength<StepperDefsList>::Value;
    using MaskType = ChooseInt<NumSteppers, false>;
    using TheDebugObject = DebugObject<Context, Object>;
    
public:
    template <int StepperIndex>
    class Stepper {
        friend Steppers;
        
        AMBRO_DECLARE_GET_MEMBER_TYPE_FUNC(GetMemberType_EnablePin, EnablePin)
        AMBRO_DECLARE_GET_MEMBER_TYPE_FUNC(GetMemberType_TheWrappedMask, TheWrappedMask)
        
        using ThisDef = TypeListGet<StepperDefsList, StepperIndex>;
        using EnablePin = typename ThisDef::EnablePin;
        static bool const StepLevel = ThisDef::StepLevel;
        static bool const EnableLevel = ThisDef::EnableLevel;
        static MaskType const TheMask = (MaskType)1 << StepperIndex;
        using TheWrappedMask = WrapValue<MaskType, TheMask>;
        
        template <typename X, typename Y>
        using OrMaskFunc = WrapValue<MaskType, (X::Value | Y::Value)>;
        
        // workaround clang bug
        template <int X>
        using Stepper_ = Stepper<X>;
        
        static MaskType const SameEnableMask = TypeListFold<
            MapTypeList<
                FilterTypeList<
                    MapTypeList<
                        SequenceList<TypeListLength<StepperDefsList>::Value>,
                        ValueTemplateFunc<int, Stepper_>
                    >,
                    ComposeFunctions<
                        IsEqualFunc<EnablePin>,
                        GetMemberType_EnablePin
                    >
                >,
                GetMemberType_TheWrappedMask
            >,
            WrapValue<MaskType, 0>,
            OrMaskFunc
        >::Value;
        
        static bool const SharesEnable = (SameEnableMask != TheMask);
        
        using CInvertDir = decltype(ExprCast<bool>(Config::e(ThisDef::InvertDir::i())));
        
    public:
        static void enable (Context c)
        {
            auto *s = Steppers::Object::self(c);
            TheDebugObject::access(c);
            if (SharesEnable) {
                s->mask |= TheMask;
            }
            Context::Pins::template set<EnablePin>(c, EnableLevel);
        }
        
        static void disable (Context c)
        {
            auto *s = Steppers::Object::self(c);
            TheDebugObject::access(c);
            if (SharesEnable) {
                s->mask &= ~TheMask;
                if (!(s->mask & SameEnableMask)) {
                    Context::Pins::template set<EnablePin>(c, !EnableLevel);
                }
            } else {
                Context::Pins::template set<EnablePin>(c, !EnableLevel);
            }
        }
        
        template <typename ThisContext>
        AMBRO_ALWAYS_INLINE
        static void setDir (ThisContext c, bool dir)
        {
            TheDebugObject::access(c);
            Context::Pins::template set<typename ThisDef::DirPin>(c, maybe_invert_dir(c, dir));
        }
        
        template <typename ThisContext>
        AMBRO_ALWAYS_INLINE
        static void stepOn (ThisContext c)
        {
            TheDebugObject::access(c);
            Context::Pins::template set<typename ThisDef::StepPin>(c, StepLevel);
        }
        
        template <typename ThisContext>
        AMBRO_ALWAYS_INLINE
        static void stepOff (ThisContext c)
        {
            TheDebugObject::access(c);
            Context::Pins::template set<typename ThisDef::StepPin>(c, !StepLevel);
        }
        
        static void emergency ()
        {
            Context::Pins::template emergencySet<EnablePin>(!EnableLevel);
        }
        
        struct Object {};
        
        using ConfigExprs = MakeTypeList<CInvertDir>;
        
    public: // private, workaround gcc bug
        static bool maybe_invert_dir (Context c, bool dir)
        {
            return APRINTER_CFG(Config, CInvertDir, c) ? !dir : dir;
        }
        
        static void init (Context c)
        {
            Context::Pins::template set<typename ThisDef::DirPin>(c, maybe_invert_dir(c, false));
            Context::Pins::template set<typename ThisDef::StepPin>(c, !StepLevel);
            Context::Pins::template set<EnablePin>(c, !EnableLevel);
            Context::Pins::template setOutput<typename ThisDef::DirPin>(c);
            Context::Pins::template setOutput<typename ThisDef::StepPin>(c);
            Context::Pins::template setOutput<EnablePin>(c);
        }
        
        static void deinit (Context c)
        {
            Context::Pins::template set<EnablePin>(c, !EnableLevel);
        }
    };
    
    static void init (Context c)
    {
        auto *o = Object::self(c);
        o->mask = 0;
        ListForEachForward<SteppersList>([&] APRINTER_TL(stepper, stepper::init(c)));
        TheDebugObject::init(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::deinit(c);
        ListForEachForward<SteppersList>([&] APRINTER_TL(stepper, stepper::deinit(c)));
    }
    
public:
    using SteppersList = IndexElemList<StepperDefsList, Stepper>;
    
    struct Object : public ObjBase<Steppers, ParentObject, JoinTypeLists<
        SteppersList,
        MakeTypeList<TheDebugObject>
    >> {
        MaskType mask;
    };
};

APRINTER_ALIAS_STRUCT_EXT(SteppersArg, (
    APRINTER_AS_TYPE(Context),
    APRINTER_AS_TYPE(ParentObject),
    APRINTER_AS_TYPE(Config),
    APRINTER_AS_TYPE(StepperDefsList)
), (
    APRINTER_DEF_INSTANCE(SteppersArg, Steppers)
))

#include <aprinter/EndNamespace.h>

#endif
