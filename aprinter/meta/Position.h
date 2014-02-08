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

#ifndef AMBROLIB_POSITION_H
#define AMBROLIB_POSITION_H

#include <stddef.h>

#include <aprinter/meta/TypeList.h>
#include <aprinter/meta/TypeListReverse.h>
#include <aprinter/meta/HasMemberTypeFunc.h>
#include <aprinter/base/GetContainer.h>

#include <aprinter/BeginNamespace.h>

namespace PositionPrivate {
    AMBRO_DECLARE_HAS_MEMBER_TYPE_FUNC(HasParent, Parent)

    template <typename Position, typename DownPath, bool IsRoot>
    struct BuildPathHelper;

    template <typename Position, typename DownPath>
    using InvokeBuildPathHelper = typename BuildPathHelper<Position, DownPath, !HasParent::Call<Position>::Type::value>::Type;

    template <typename Position, typename DownPath>
    struct BuildPathHelper<Position, DownPath, true> {
        using Type = ConsTypeList<Position, DownPath>;
    };

    template <typename Position, typename DownPath>
    struct BuildPathHelper<Position, DownPath, false> {
        using Type = InvokeBuildPathHelper<typename Position::Parent, ConsTypeList<Position, DownPath>>;
    };

    template <typename Position>
    using BuildPath = InvokeBuildPathHelper<Position, EmptyTypeList>;

    template <typename List>
    struct GoDownHelper;

    template <typename Position, typename Position2, typename Tail>
    struct GoDownHelper<ConsTypeList<Position, ConsTypeList<Position2, Tail>>> {
        using TheDownHelper = GoDownHelper<ConsTypeList<Position2, Tail>>;
        using ResultType = typename TheDownHelper::ResultType;
        
        static ResultType * call (typename Position::ObjectType *x)
        {
            return TheDownHelper::call(Position2::down(x));
        }
    };

    template <typename Position>
    struct GoDownHelper<ConsTypeList<Position, EmptyTypeList>> {
        using ResultType = typename Position::ObjectType;
        
        static ResultType * call (typename Position::ObjectType *x)
        {
            return x;
        }
    };

    template <typename List>
    struct GoUpHelper;

    template <typename Position, typename Position2, typename Tail>
    struct GoUpHelper<ConsTypeList<Position, ConsTypeList<Position2, Tail>>> {
        using TheDownHelper = GoUpHelper<ConsTypeList<Position2, Tail>>;
        using ResultType = typename TheDownHelper::ResultType;
        
        static ResultType * call (typename Position::ObjectType *x)
        {
            return TheDownHelper::call(Position::up(x));
        }
    };

    template <typename Position>
    struct GoUpHelper<ConsTypeList<Position, EmptyTypeList>> {
        using ResultType = typename Position::ObjectType;
        
        static ResultType * call (typename Position::ObjectType *x)
        {
            return x;
        }
    };

    template <typename List1, typename List2>
    struct GetCommonHead;

    template <typename Head, typename Tail1, typename Tail2>
    struct GetCommonHead<ConsTypeList<Head, Tail1>, ConsTypeList<Head, Tail2>> {
        using Type = Head;
    };
}

template <typename TObjectType>
struct RootPosition {
    using ObjectType = TObjectType;
};

template <typename TParent, typename TObjectType, TObjectType TParent::ObjectType::*TMemberPtr>
struct MemberPosition {
    using Parent = TParent;
    using ObjectType = TObjectType;
    using ParentType = typename Parent::ObjectType;
    
    static ObjectType * down (ParentType *x)
    {
        return &(x->*TMemberPtr);
    }
    
    static ParentType * up (ObjectType *x)
    {
        return GetContainer(x, TMemberPtr);
    }
};

template <typename Position1, typename Position2>
typename Position2::ObjectType * PositionTraverse (typename Position1::ObjectType *x)
{
    using Path1 = PositionPrivate::BuildPath<Position1>;
    using Path2 = PositionPrivate::BuildPath<Position2>;
    using RootPosition = typename PositionPrivate::GetCommonHead<Path1, Path2>::Type;
    
    return PositionPrivate::GoDownHelper<Path2>::call(PositionPrivate::GoUpHelper<TypeListReverse<Path1>>::call(x));
}

#define AMBRO_MAKE_SELF(Context, Type, Position) \
static Type * self (Context c) \
{ \
    return APrinter::PositionTraverse<typename Context::TheRootPosition, Position>(c.root()); \
}

#include <aprinter/EndNamespace.h>

#endif
