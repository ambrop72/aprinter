/*
 * Copyright (c) 2015 Ambroz Bizjak
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

#ifndef APRINTER_LIST_COLLECT_H
#define APRINTER_LIST_COLLECT_H

#include <aprinter/meta/TypeList.h>
#include <aprinter/meta/TypeDict.h>
#include <aprinter/meta/FuncUtils.h>
#include <aprinter/meta/TypeListUtils.h>
#include <aprinter/meta/MemberType.h>

#include <aprinter/BeginNamespace.h>

namespace ListCollectImpl {
    APRINTER_DEFINE_MEMBER_TYPE(MemberType_Key, Key)
    APRINTER_DEFINE_MEMBER_TYPE(MemberType_Value, Value)
    APRINTER_DEFINE_MEMBER_TYPE(MemberType_Found, Found)
    
    template <typename Key>
    struct AddKeyFunc {
        template <typename X>
        struct Call {
            using Type = TypeDictEntry<Key, X>;
        };
    };
    
    template <typename MemberType>
    struct CollectHelper {
        template <typename NumberedEntry>
        using EnumerateMemberList = MapTypeList<
            FuncCall<typename MemberType::Get, typename NumberedEntry::Value>,
            AddKeyFunc<typename NumberedEntry::Key>
        >;
    };
    
    template <typename GroupFunc>
    struct GroupHelper {
        template <typename CurrentGroups, typename Element>
        struct AddToNewGroupFunc {
            template <typename FindResult>
            struct Call {
                using Type = ConsTypeList<
                    TypeDictEntry<
                        FuncCall<GroupFunc, Element>,
                        MakeTypeList<
                            Element
                        >
                    >,
                    CurrentGroups
                >;
            };
        };
        
        template <typename Element>
        struct PrependToGroupFunc {
            template <typename OldGroup>
            struct Call {
                using Type = TypeDictEntry<
                    typename OldGroup::Key,
                    ConsTypeList<
                        Element,
                        typename OldGroup::Value
                    >
                >;
            };
        };
        
        struct DontChangeGroupFunc {
            template <typename OldGroup>
            struct Call {
                using Type = OldGroup;
            };
        };
        
        template <typename CurrentGroups, typename Element>
        struct AddToExistingGroupFunc {
            template <typename FindResult>
            struct Call {
                using Type = MapTypeList<
                    CurrentGroups,
                    IfFunc<
                        ComposeFunctions<IsEqualFunc<FuncCall<GroupFunc, Element>>, typename MemberType_Key::Get>,
                        PrependToGroupFunc<Element>,
                        DontChangeGroupFunc
                    >
                >;
            };
        };
        
        template <typename Element, typename CurrentGroups>
        using GroupFoldFunc = FuncCall<
            IfFunc<
                IsEqualFunc<TypeDictNotFound>,
                AddToNewGroupFunc<CurrentGroups, Element>,
                AddToExistingGroupFunc<CurrentGroups, Element>
            >,
            TypeDictFind<CurrentGroups, FuncCall<GroupFunc, Element>>
        >;
    };
}

/**
 * Collect all the MemberType lists in the given List and return a
 * list of (index, element) pairs, where element is an element of one
 * of the MemberType lists, and index indicates which element of List
 * it comes from. Order is preserved.
 * 
 * Example:
 * 
 * \code
 * struct A { using X = MakeTypeList<int>; };
 * struct B { using X = MakeTypeList<float, int>; };
 * 
 * APRINTER_DEFINE_MEMBER_TYPE(MemberType_X, X)
 * 
 * ListCollect<MakeTypeList<A, B>, MemberType_X>
 * // produces
 * MakeTypeList<
 *     TypeDictEntry<WrapInt<0>, int>,
 *     TypeDictEntry<WrapInt<1>, float>,
 *     TypeDictEntry<WrapInt<1>, int>
 * >
 * \endcode
 */
template <typename List, typename MemberType>
using ListCollect = JoinTypeListList<
    MapTypeList<
        TypeListEnumerate<List>,
        IfFunc<
            ComposeFunctions<typename MemberType::Has, ListCollectImpl::MemberType_Value::Get>,
            TemplateFunc<ListCollectImpl::CollectHelper<MemberType>::template EnumerateMemberList>,
            ConstantFunc<EmptyTypeList>
        >
    >
>;

/**
 * Group the elements of List based on the function GroupFunc,
 * which given an element of List should return the group ID.
 * The order of the groups and group elements is unspecified.
 * 
 * Example:
 * 
 * \code
 * struct A { using Group = int; };
 * struct B { using Group = float; };
 * struct C { using Group = int; };
 * 
 * template <typename X>
 * using GroupFunc = typename X::Group;
 * 
 * ListGroup<MakeTypeList<A, B, C>, TemplateFunc<GroupFunc>>
 * // produces
 * MakeTypeList<
 *     TypeDictEntry<float, MakeTypeList<B>>,
 *     TypeDictEntry<int, MakeTypeList<C, A>>
 * >
 * \endcode
 */
template <typename List, typename GroupFunc>
using ListGroup = TypeListFold<List, EmptyTypeList, ListCollectImpl::template GroupHelper<GroupFunc>::template GroupFoldFunc>;

#include <aprinter/EndNamespace.h>

#endif
