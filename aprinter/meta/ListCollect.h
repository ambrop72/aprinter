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
#include <aprinter/meta/IfFunc.h>
#include <aprinter/meta/ConstantFunc.h>
#include <aprinter/meta/TemplateFunc.h>
#include <aprinter/meta/TypeListUtils.h>
#include <aprinter/meta/ComposeFunctions.h>
#include <aprinter/meta/MemberType.h>
#include <aprinter/meta/FuncCall.h>
#include <aprinter/meta/IsEqualFunc.h>

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
    
    template <typename CurrentGroups, typename NumberedEntry>
    struct AddToNewGroupFunc {
        template <typename FindResult>
        struct Call {
            using Type = ConsTypeList<TypeDictEntry<typename NumberedEntry::Value, MakeTypeList<typename NumberedEntry::Key>>, CurrentGroups>;
        };
    };
    
    template <typename NumberedEntry>
    struct PrependToGroupFunc {
        template <typename OldGroup>
        struct Call {
            using Type = TypeDictEntry<typename OldGroup::Key, ConsTypeList<typename NumberedEntry::Key, typename OldGroup::Value>>;
        };
    };
    
    struct DontChangeGroupFunc {
        template <typename OldGroup>
        struct Call {
            using Type = OldGroup;
        };
    };
    
    template <typename CurrentGroups, typename NumberedEntry>
    struct AddToExistingGroupFunc {
        template <typename FindResult>
        struct Call {
            using Type = MapTypeList<
                CurrentGroups,
                IfFunc<
                    ComposeFunctions<IsEqualFunc<typename NumberedEntry::Value>, typename MemberType_Key::Get>,
                    PrependToGroupFunc<NumberedEntry>,
                    DontChangeGroupFunc
                >
            >;
        };
    };
    
    template <typename NumberedEntry, typename CurrentGroups>
    using GroupFoldFunc = FuncCall<
        IfFunc<
            IsEqualFunc<TypeDictNotFound>,
            AddToNewGroupFunc<CurrentGroups, NumberedEntry>,
            AddToExistingGroupFunc<CurrentGroups, NumberedEntry>
        >,
        TypeDictFind<CurrentGroups, typename NumberedEntry::Value>
    >;
}

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

template <typename List>
using ListGroup = TypeListFold<List, EmptyTypeList, ListCollectImpl::GroupFoldFunc>;

#include <aprinter/EndNamespace.h>

#endif
