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

#ifndef AMBROLIB_OBJECT_H
#define AMBROLIB_OBJECT_H

#include <aprinter/meta/TypeList.h>
#include <aprinter/meta/MakeTypeList.h>
#include <aprinter/meta/Tuple.h>
#include <aprinter/meta/Union.h>
#include <aprinter/meta/TypeListIndex.h>
#include <aprinter/meta/IsEqualFunc.h>
#include <aprinter/meta/MapTypeList.h>
#include <aprinter/meta/TupleGet.h>
#include <aprinter/meta/UnionGet.h>
#include <aprinter/meta/GetMemberTypeFunc.h>
#include <aprinter/meta/HasMemberTypeFunc.h>
#include <aprinter/meta/TypeListGet.h>
#include <aprinter/meta/If.h>
#include <aprinter/meta/JoinTypeLists.h>
#include <aprinter/meta/JoinTypeListList.h>
#include <aprinter/meta/MapTypeList.h>
#include <aprinter/meta/TemplateFunc.h>
#include <aprinter/meta/IfFunc.h>
#include <aprinter/meta/ConstantFunc.h>

#include <aprinter/BeginNamespace.h>

namespace ObjectPrivate {
    AMBRO_DECLARE_GET_MEMBER_TYPE_FUNC(GetMemberType_Object, Object)
}

template <typename TClass, typename TParentObject, typename TNestedClassesList>
struct ObjBase : public Tuple<MapTypeList<TNestedClassesList, ObjectPrivate::GetMemberType_Object>> {
    using Class = TClass;
    using ParentObject = TParentObject;
    using NestedClassesList = TNestedClassesList;
    using NestedClassesTuple = Tuple<MapTypeList<TNestedClassesList, ObjectPrivate::GetMemberType_Object>>;
    
    template <typename Context, typename DelayClass = Class>
    static typename DelayClass::Object * self (Context c)
    {
        ParentObject *parent_object = ParentObject::self(c);
        return parent_object->template get_nested_object<Class>(c);
    }
    
    template <typename NestedClass, typename Context>
    typename NestedClass::Object * get_nested_object (Context c)
    {
        return TupleGetElem<TypeListIndex<NestedClassesList, IsEqualFunc<NestedClass>>::Value>(static_cast<NestedClassesTuple *>(this));
    }
};

template <typename TClass, typename TParentObject, typename TNestedClassesList>
struct ObjUnionBase : public Union<MapTypeList<TNestedClassesList, ObjectPrivate::GetMemberType_Object>> {
    using Class = TClass;
    using ParentObject = TParentObject;
    using NestedClassesList = TNestedClassesList;
    using NestedClassesUnion = Union<MapTypeList<TNestedClassesList, ObjectPrivate::GetMemberType_Object>>;
    
    template <typename Context, typename DelayClass = Class>
    static typename DelayClass::Object * self (Context c)
    {
        ParentObject *parent_object = ParentObject::self(c);
        return parent_object->template get_nested_object<Class>(c);
    }
    
    template <typename NestedClass, typename Context>
    typename NestedClass::Object * get_nested_object (Context c)
    {
        return UnionGetElem<TypeListIndex<NestedClassesList, IsEqualFunc<NestedClass>>::Value>(static_cast<NestedClassesUnion *>(this));
    }
};

#define APRINTER_DECLARE_COLLECTIBLE(ClassName, CollectibleName) \
struct ClassName { \
    AMBRO_DECLARE_HAS_MEMBER_TYPE_FUNC(Has, CollectibleName) \
    AMBRO_DECLARE_GET_MEMBER_TYPE_FUNC(Get, CollectibleName) \
};

AMBRO_DECLARE_HAS_MEMBER_TYPE_FUNC(Obj__HasMemberType_NestedClassesList, NestedClassesList)
AMBRO_DECLARE_GET_MEMBER_TYPE_FUNC(Obj__GetMemberType_NestedClassesList, NestedClassesList)

template <typename TheClass, typename Collectible, bool WithoutSelf>
struct Obj__CollectHelper {
    template <typename TheChildClass>
    using CollectChild = typename Obj__CollectHelper<TheChildClass, Collectible, false>::Result;
    
    using Result = JoinTypeLists<
        If<
            WithoutSelf,
            EmptyTypeList,
            typename IfFunc<
                typename Collectible::Has,
                typename Collectible::Get,
                ConstantFunc<EmptyTypeList>
            >::template Call<TheClass>::Type
        >,
        JoinTypeListList<
            MapTypeList<
                typename IfFunc<
                    Obj__HasMemberType_NestedClassesList,
                    Obj__GetMemberType_NestedClassesList,
                    ConstantFunc<EmptyTypeList>
                >::template Call<typename TheClass::Object>::Type,
                TemplateFunc<CollectChild>
            >
        >
    >;
};

template <typename TheClass, typename Collectible, bool WithoutSelf = false>
using ObjCollect = typename Obj__CollectHelper<TheClass, Collectible, WithoutSelf>::Result;

template <typename Collectible>
struct Obj__CollectListHelper {
    template <typename TheChildClass>
    using Collect = ObjCollect<TheChildClass, Collectible, false>;
};

template <typename ClassList, typename Collectible>
using ObjCollectList = JoinTypeListList<
    MapTypeList<
        ClassList,
        TemplateFunc<Obj__CollectListHelper<Collectible>::template Collect>
    >
>;

#include <aprinter/EndNamespace.h>

#endif
