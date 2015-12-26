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

#ifndef AMBROLIB_TYPE_LIST_UTILS_H
#define AMBROLIB_TYPE_LIST_UTILS_H

#include <aprinter/meta/TypeList.h>
#include <aprinter/meta/TypeDict.h>
#include <aprinter/meta/WrapValue.h>
#include <aprinter/meta/FuncUtils.h>
#include <aprinter/meta/If.h>

#include <aprinter/BeginNamespace.h>

template <typename... Ts>
struct MakeTypeListHelper;

template <>
struct MakeTypeListHelper<> {
    typedef EmptyTypeList Type;
};

template <typename T, typename... Ts>
struct MakeTypeListHelper<T, Ts...> {
    typedef ConsTypeList<T, typename MakeTypeListHelper<Ts...>::Type> Type;
};

template <typename... Ts>
using MakeTypeList = typename MakeTypeListHelper<Ts...>::Type;

template <typename List, typename Func>
struct MapTypeListHelper;

template <typename Func>
struct MapTypeListHelper<EmptyTypeList, Func> {
    typedef EmptyTypeList Type;
};

template <typename Head, typename Tail, typename Func>
struct MapTypeListHelper<ConsTypeList<Head, Tail>, Func> {
    typedef ConsTypeList<FuncCall<Func, Head>, typename MapTypeListHelper<Tail, Func>::Type> Type;
};

template <typename List, typename Func>
using MapTypeList = typename MapTypeListHelper<List, Func>::Type;

namespace Private {
    template <typename List>
    struct TypeListLengthHelper;

    template <>
    struct TypeListLengthHelper<EmptyTypeList> {
        static const int Value = 0;
    };

    template <typename Head, typename Tail>
    struct TypeListLengthHelper<ConsTypeList<Head, Tail>> {
        static const int Value = 1 + TypeListLengthHelper<Tail>::Value;
    };
}

template <typename List>
using TypeListLength = Private::TypeListLengthHelper<List>;

template <typename List, typename Reversed>
struct TypeListReverseHelper;

template <typename Reversed>
struct TypeListReverseHelper<EmptyTypeList, Reversed> {
    using Type = Reversed;
};

template <typename Head, typename Tail, typename Reversed>
struct TypeListReverseHelper<ConsTypeList<Head, Tail>, Reversed> {
    using Type = typename TypeListReverseHelper<Tail, ConsTypeList<Head, Reversed>>::Type;
};

template <typename List>
using TypeListReverse = typename TypeListReverseHelper<List, EmptyTypeList>::Type;

template <typename List, typename Value, template<typename, typename> class FoldFunc>
struct TypeListFoldHelper;

template <typename Value, template<typename, typename> class FoldFunc>
struct TypeListFoldHelper<EmptyTypeList, Value, FoldFunc> {
    using Type = Value;
};

template <typename Head, typename Tail, typename Value, template<typename, typename> class FoldFunc>
struct TypeListFoldHelper<ConsTypeList<Head, Tail>, Value, FoldFunc> {
    using Type = typename TypeListFoldHelper<Tail, FoldFunc<Head, Value>, FoldFunc>::Type;
};

template <typename List, typename InitialValue, template<typename ListElem, typename AccumValue> class FoldFunc>
using TypeListFold = typename TypeListFoldHelper<List, InitialValue, FoldFunc>::Type;

template <typename List, typename Value, template<typename, typename> class FoldFunc>
struct TypeListFoldRightHelper;

template <typename Value, template<typename, typename> class FoldFunc>
struct TypeListFoldRightHelper<EmptyTypeList, Value, FoldFunc> {
    using Type = Value;
};

template <typename Head, typename Tail, typename Value, template<typename, typename> class FoldFunc>
struct TypeListFoldRightHelper<ConsTypeList<Head, Tail>, Value, FoldFunc> {
    using Type = FoldFunc<Head, typename TypeListFoldRightHelper<Tail, Value, FoldFunc>::Type>;
};

template <typename List, typename InitialValue, template<typename ListElem, typename AccumValue> class FoldFunc>
using TypeListFoldRight = typename TypeListFoldRightHelper<List, InitialValue, FoldFunc>::Type;

template <typename List, int IndexFrom>
struct TypeListRangeFromHelper;

template <>
struct TypeListRangeFromHelper<EmptyTypeList, 0> {
    using Result = EmptyTypeList;
};

template <typename Head, typename Tail>
struct TypeListRangeFromHelper<ConsTypeList<Head, Tail>, 0> {
    using Result = ConsTypeList<Head, Tail>;
};

template <typename Head, typename Tail, int IndexFrom>
struct TypeListRangeFromHelper<ConsTypeList<Head, Tail>, IndexFrom> {
    static_assert(IndexFrom > 0, "");
    using Result = typename TypeListRangeFromHelper<Tail, (IndexFrom - 1)>::Result;
};

template <typename List, int IndexTo>
struct TypeListRangeToHelper;

template <>
struct TypeListRangeToHelper<EmptyTypeList, 0> {
    using Result = EmptyTypeList;
};

template <typename Head, typename Tail>
struct TypeListRangeToHelper<ConsTypeList<Head, Tail>, 0> {
    using Result = EmptyTypeList;
};

template <typename Head, typename Tail, int IndexTo>
struct TypeListRangeToHelper<ConsTypeList<Head, Tail>, IndexTo> {
    static_assert(IndexTo > 0, "");
    using Result = ConsTypeList<Head, typename TypeListRangeToHelper<Tail, (IndexTo - 1)>::Result>;
};

template <typename List, int IndexFrom>
using TypeListRangeFrom = typename TypeListRangeFromHelper<List, IndexFrom>::Result;

template <typename List, int IndexTo>
using TypeListRangeTo = typename TypeListRangeToHelper<List, IndexTo>::Result;

template <typename List, int IndexFrom, int Count>
using TypeListRange = TypeListRangeTo<TypeListRangeFrom<List, IndexFrom>, Count>;

template <typename List1, typename List2>
struct JoinTypeListsHelper;

template <typename List2>
struct JoinTypeListsHelper<EmptyTypeList, List2> {
    typedef List2 Type;
};

template <typename Head, typename Tail, typename List2>
struct JoinTypeListsHelper<ConsTypeList<Head, Tail>, List2> {
    typedef ConsTypeList<Head, typename JoinTypeListsHelper<Tail, List2>::Type> Type;
};

template <typename List1, typename List2>
using JoinTwoTypeLists = typename JoinTypeListsHelper<List1, List2>::Type;

template <typename List1, typename List2>
using JoinTwoTypeListsSwapped = typename JoinTypeListsHelper<List2, List1>::Type;

template <typename... Lists>
using JoinTypeLists = TypeListFoldRight<MakeTypeList<Lists...>, EmptyTypeList, JoinTwoTypeLists>;

template <typename Lists>
using JoinTypeListList = TypeListFoldRight<Lists, EmptyTypeList, JoinTwoTypeLists>;

template <typename Lists, int ListIndex>
using GetJoinedListOffset = WrapInt<(
    TypeListLength<JoinTypeListList<Lists>>::Value -
    TypeListLength<JoinTypeListList<TypeListRangeFrom<Lists, ListIndex>>>::Value
)>;

template <typename List, typename Predicate>
struct FilterTypeListHelper;

namespace Private {
    template <typename Head, typename Tail, typename Predicate, bool IncludeHead>
    struct FilterTypeListHelperHelper;

    template <typename Head, typename Tail, typename Predicate>
    struct FilterTypeListHelperHelper<Head, Tail, Predicate, true> {
        typedef ConsTypeList<Head, typename FilterTypeListHelper<Tail, Predicate>::Type> Type;
    };

    template <typename Head, typename Tail, typename Predicate>
    struct FilterTypeListHelperHelper<Head, Tail, Predicate, false> {
        typedef typename FilterTypeListHelper<Tail, Predicate>::Type Type;
    };
}

template <typename Predicate>
struct FilterTypeListHelper<EmptyTypeList, Predicate> {
    typedef EmptyTypeList Type;
};

template <typename Head, typename Tail, typename Predicate>
struct FilterTypeListHelper<ConsTypeList<Head, Tail>, Predicate> {
    typedef typename Private::FilterTypeListHelperHelper<Head, Tail, Predicate, FuncCall<Predicate, Head>::Value>::Type Type;
};

template <typename List, typename Predicate>
using FilterTypeList = typename FilterTypeListHelper<List, Predicate>::Type;

namespace Private {
    template <int, typename>
    struct TypeDictElemToIndexMapHelper;
    
    template <int Offset, typename Head, typename Tail>
    struct TypeDictElemToIndexMapHelper<Offset, ConsTypeList<Head, Tail>> {
        using Result = ConsTypeList<
            TypeDictEntry<Head, WrapInt<Offset>>,
            typename TypeDictElemToIndexMapHelper<
                (Offset + 1),
                Tail
            >::Result
        >;
    };
    
    template <int Offset>
    struct TypeDictElemToIndexMapHelper<Offset, EmptyTypeList> {
        using Result = EmptyTypeList;
    };
    
    template <int, typename>
    struct TypeDictIndexToSublistMapHelper;
    
    template <int Offset, typename Head, typename Tail>
    struct TypeDictIndexToSublistMapHelper<Offset, ConsTypeList<Head, Tail>> {
        using Result = ConsTypeList<
            TypeDictEntry<WrapInt<Offset>, ConsTypeList<Head, Tail>>,
            typename TypeDictIndexToSublistMapHelper<
                (Offset + 1),
                Tail
            >::Result
        >;
    };
    
    template <int Offset>
    struct TypeDictIndexToSublistMapHelper<Offset, EmptyTypeList> {
        using Result = EmptyTypeList;
    };
}

template <typename List>
using TypeDictMakeElemToIndexMap = typename Private::TypeDictElemToIndexMapHelper<0, List>::Result;

template <typename List>
using TypeDictMakeIndexToSublistMap = typename Private::TypeDictIndexToSublistMapHelper<0, List>::Result;

namespace Private {
    template <typename List, int Index>
    struct ListIndexGetHelper {
        using FindRes = TypeDictFindNoDupl<TypeDictMakeIndexToSublistMap<List>, WrapInt<Index>>;
        static_assert(FindRes::Found, "Element index is outside the range of the list.");
        using Result = typename FindRes::Result::Head;
    };
}

template <typename List, int Index>
using TypeListGet = typename Private::ListIndexGetHelper<List, Index>::Result;

namespace Private {
    template <typename List>
    struct TypeListGetFuncHelper {
        template <typename Index>
        struct Call {
            using Type = TypeListGet<List, Index::Value>;
        };
    };
}

template <typename List>
using TypeListGetFunc = Private::TypeListGetFuncHelper<List>;

template <typename List, typename Value>
using TypeListFind = TypeDictFind<TypeDictMakeElemToIndexMap<List>, Value>;

template <typename List, typename Value>
using TypeListIndex = typename TypeListFind<List, Value>::Result;

template <typename List, typename Func, typename FuncValue>
using TypeListFindMapped = TypeListFind<MapTypeList<List, Func>, FuncValue>;

template <typename List, typename Func, typename FuncValue>
using TypeListIndexMapped = typename TypeListFindMapped<List, Func, FuncValue>::Result;

template <typename List, typename Func, typename FuncValue>
using TypeListGetMapped = TypeListGet<List, TypeListIndexMapped<List, Func, FuncValue>::Value>;

namespace Private {
    struct TypeListGetDictEntryKeyFunc {
        template <typename DictEntry>
        struct Call;
        
        template <typename Key, typename Value>
        struct Call<TypeDictEntry<Key, Value>> {
            using Type = Key;
        };
    };
}

template <typename List>
using TypeListRemoveDuplicates = TypeListReverse<
    MapTypeList<
        TypeDictRemoveDuplicatesAndReverse<
            TypeDictMakeElemToIndexMap<List>
        >,
        Private::TypeListGetDictEntryKeyFunc
    >
>;

namespace Private {
    template <int Offset, typename List>
    struct TypeListEnumerateHelper;
    
    template <int Offset>
    struct TypeListEnumerateHelper<Offset, EmptyTypeList> {
        using Result = EmptyTypeList;
    };
    
    template <int Offset, typename Head, typename Tail>
    struct TypeListEnumerateHelper<Offset, ConsTypeList<Head, Tail>> {
        using Result = ConsTypeList<
            TypeDictEntry<WrapInt<Offset>, Head>,
            typename TypeListEnumerateHelper<(Offset + 1), Tail>::Result
        >;
    };
};

template <typename List, int Offset=0>
using TypeListEnumerate = typename Private::TypeListEnumerateHelper<Offset, List>::Result;

namespace Private {
    struct TypeDictGetKeyFunc {
        template <typename Entry>
        struct Call {
            using Type = typename Entry::Key;
        };
    };
    
    struct TypeDictGetValueFunc {
        template <typename Entry>
        struct Call {
            using Type = typename Entry::Value;
        };
    };
}

template <typename List>
using TypeDictKeys = MapTypeList<List, Private::TypeDictGetKeyFunc>;

template <typename List>
using TypeDictValues = MapTypeList<List, Private::TypeDictGetValueFunc>;

namespace ListSortPrivate {
    template <template<typename, typename> class LessFunc>
    struct SortHelper {
        template <typename Dummy, typename List1, typename List2>
        struct Merge;
        
        template <typename Dummy>
        struct Merge<Dummy, EmptyTypeList, EmptyTypeList> {
            using Result = EmptyTypeList;
        };
        
        template <typename Dummy, typename Head1, typename Tail1>
        struct Merge<Dummy, ConsTypeList<Head1, Tail1>, EmptyTypeList> {
            using Result = ConsTypeList<Head1, Tail1>;
        };
        
        template <typename Dummy, typename Head2, typename Tail2>
        struct Merge<Dummy, EmptyTypeList, ConsTypeList<Head2, Tail2>> {
            using Result = ConsTypeList<Head2, Tail2>;
        };
        
        template <typename Dummy, typename Head1, typename Tail1, typename Head2, typename Tail2>
        struct Merge<Dummy, ConsTypeList<Head1, Tail1>, ConsTypeList<Head2, Tail2>> {
            using Result = If<
                !(LessFunc<Head2, Head1>::Value),
                ConsTypeList<Head1, typename Merge<Dummy, Tail1, ConsTypeList<Head2, Tail2>>::Result>,
                ConsTypeList<Head2, typename Merge<Dummy, ConsTypeList<Head1, Tail1>, Tail2>::Result>
            >;
        };
        
        template <typename Dummy, typename List>
        struct Sort {
            static int const SplitIndex = TypeListLength<List>::Value / 2;
            using Result = typename Merge<
                void,
                typename Sort<Dummy, TypeListRangeTo<List, SplitIndex>>::Result,
                typename Sort<Dummy, TypeListRangeFrom<List, SplitIndex>>::Result
            >::Result;
        };
        
        template <typename Dummy, typename Head>
        struct Sort<Dummy, ConsTypeList<Head, EmptyTypeList>> {
            using Result = ConsTypeList<Head, EmptyTypeList>;
        };
        
        template <typename Dummy>
        struct Sort<Dummy, EmptyTypeList> {
            using Result = EmptyTypeList;
        };
    };
}

/**
 * Sort the elements of List based on the comparision function.
 * The LessFunc is given two elements of List and should
 * expand to a WrapBool determining if the first element is lesser
 * than the second. The order of elements which compare equal is
 * preserved.
 * 
 * Example:
 * 
 * \code
 * template <typename X, typename Y>
 * using LessFunc = WrapBool<(X::Value < Y::Value)>;
 * 
 * ListSort<MakeTypeList<WrapInt<6>, WrapInt<3>, WrapInt<4>>, LessFunc>
 * // produces
 * MakeTypeList<WrapInt<3>, WrapInt<4>, WrapInt<6>>
 * \endcode
 */
template <typename List, template<typename, typename> class LessFunc>
using ListSort = typename ListSortPrivate::template SortHelper<LessFunc>::template Sort<void, List>::Result;

#include <aprinter/EndNamespace.h>

#endif
