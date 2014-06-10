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

#ifndef AMBROLIB_STEPPER_GROUPS_H
#define AMBROLIB_STEPPER_GROUPS_H

#include <aprinter/meta/Object.h>
#include <aprinter/meta/ListForEach.h>
#include <aprinter/meta/TypeList.h>
#include <aprinter/meta/TypeListLength.h>
#include <aprinter/meta/TypeListGet.h>
#include <aprinter/meta/JoinTypeLists.h>
#include <aprinter/meta/TypeListFold.h>
#include <aprinter/meta/MapTypeList.h>
#include <aprinter/meta/TypeListRange.h>
#include <aprinter/meta/MakeTypeList.h>
#include <aprinter/meta/GetMemberTypeFunc.h>
#include <aprinter/driver/Steppers.h>

#include <aprinter/BeginNamespace.h>

template <typename TStepperDefList>
struct StepperGroupParams {
    using StepperDefList = TStepperDefList;
};

template <typename Context, typename ParentObject, typename GroupParamsList>
class StepperGroups {
public:
    struct Object;
    
    template <int GroupIndex>
    class Group;
    
    template <int GroupIndex>
    using Stepper = Group<GroupIndex>;
    
private:
    AMBRO_DECLARE_LIST_FOREACH_HELPER(Foreach_enable, enable)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(Foreach_disable, disable)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(Foreach_setDir, setDir)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(Foreach_stepOn, stepOn)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(Foreach_stepOff, stepOff)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(Foreach_emergency, emergency)
    AMBRO_DECLARE_GET_MEMBER_TYPE_FUNC(GetMemberType_StepperDefList, StepperDefList)
    
    template <int GroupIndex, typename Dummy = void>
    struct FirstStepperInGroup {
        using PreviousGroup = Group<(GroupIndex - 1)>;
        static int const Value = PreviousGroup::FirstStepperIndex + PreviousGroup::NumSteppers;
    };
    
    template <typename Dummy>
    struct FirstStepperInGroup<0, Dummy> {
        static int const Value = 0;
    };
    
    using StepperDefList = TypeListFold<MapTypeList<GroupParamsList, GetMemberType_StepperDefList>, EmptyTypeList, JoinTwoTypeListsSwapped>;
    using TheSteppers = Steppers<Context, Object, StepperDefList>;
    
public:
    template <int GroupIndex>
    class Group {
        friend StepperGroups;
        using GroupParams = TypeListGet<GroupParamsList, GroupIndex>;
        using GroupStepperDefList = typename GroupParams::StepperDefList;
        static int const FirstStepperIndex = FirstStepperInGroup<GroupIndex>::Value;
        static int const NumSteppers = TypeListLength<GroupStepperDefList>::Value;
        using GroupSteppersList = TypeListRange<typename TheSteppers::SteppersList, FirstStepperIndex, NumSteppers>;
        
    public:
        static void enable (Context c)
        {
            ListForEachForward<GroupSteppersList>(Foreach_enable(), c);
        }
        
        static void disable (Context c)
        {
            ListForEachForward<GroupSteppersList>(Foreach_disable(), c);
        }
        
        template <typename ThisContext>
        static void setDir (ThisContext c, bool dir)
        {
            ListForEachForward<GroupSteppersList>(Foreach_setDir(), c, dir);
        }
        
        template <typename ThisContext>
        static void stepOn (ThisContext c)
        {
            ListForEachForward<GroupSteppersList>(Foreach_stepOn(), c);
        }
        
        template <typename ThisContext>
        static void stepOff (ThisContext c)
        {
            ListForEachForward<GroupSteppersList>(Foreach_stepOff(), c);
        }
        
        static void emergency ()
        {
            ListForEachForward<GroupSteppersList>(Foreach_emergency());
        }
    };
    
    static void init (Context c)
    {
        TheSteppers::init(c);
    }
    
    static void deinit (Context c)
    {
        TheSteppers::deinit(c);
    }
    
public:
    struct Object : public ObjBase<StepperGroups, ParentObject, MakeTypeList<
        TheSteppers
    >> {};
};

#include <aprinter/EndNamespace.h>

#endif
