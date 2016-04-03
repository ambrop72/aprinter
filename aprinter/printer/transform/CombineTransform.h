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

#ifndef APRINTER_COMBINE_TRANSFORM_H
#define APRINTER_COMBINE_TRANSFORM_H

#include <aprinter/meta/TypeListUtils.h>
#include <aprinter/meta/ListForEach.h>
#include <aprinter/meta/BasicMetaUtils.h>
#include <aprinter/meta/AliasStruct.h>
#include <aprinter/base/Object.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, typename Config, typename FpType, typename Params>
class CombineTransform {
public:
    struct Object;
    
private:
    using TransformServicesList = typename Params::TransformServicesList;
    
public:
    template <typename Src, typename Dst>
    static bool virtToPhys (Context c, Src virt, Dst out_phys)
    {
        return ListForEachForwardInterruptible<HelpersList>([&] APRINTER_TL(helper, return helper::virt_to_phys(c, virt, out_phys)));
    }
    
    template <typename Src, typename Dst>
    static void physToVirt (Context c, Src phys, Dst out_virt)
    {
        ListForEachForward<HelpersList>([&] APRINTER_TL(helper, helper::phys_to_virt(c, phys, out_virt)));
    }
    
private:
    template <int TransformIndex, typename Dummy=void>
    struct Helper;
    
    template <typename Dummy>
    struct Helper<-1, Dummy> {
        static int const AxisEndIndex = 0;
    };
    
    template <int TransformIndex, typename Dummy>
    struct Helper<TransformIndex, Dummy> {
        struct Object;
        using TheTransformService = TypeListGet<TransformServicesList, TransformIndex>;
        
        using TheTransform = typename TheTransformService::template Transform<Context, Object, Config, FpType>;
        
        static int const AxisStartIndex = Helper<(TransformIndex-1)>::AxisEndIndex;
        static int const AxisEndIndex = AxisStartIndex + TheTransform::NumAxes;
        
        template <typename Src, typename Dst>
        static bool virt_to_phys (Context c, Src virt, Dst out_phys)
        {
            return TheTransform::virtToPhys(c, OffsetSrc<Src>{virt}, OffsetDst<Dst>{out_phys});
        }
        
        template <typename Src, typename Dst>
        static void phys_to_virt (Context c, Src phys, Dst out_virt)
        {
            TheTransform::physToVirt(c, OffsetSrc<Src>{phys}, OffsetDst<Dst>{out_virt});
        }
        
        template <typename Src>
        struct OffsetSrc {
            Src &src;
            template <int Index> FpType get () { return src.template get<(AxisStartIndex+Index)>(); }
        };
        
        template <typename Dst>
        struct OffsetDst {
            Dst &dst;
            template <int Index> void set (FpType x) { dst.template set<(AxisStartIndex+Index)>(x); }
        };
        
        struct Object : public ObjBase<Helper, typename CombineTransform::Object, MakeTypeList<
            TheTransform
        >> {};
    };
    using HelpersList = IndexElemList<TransformServicesList, DedummyIndexTemplate<Helper>::template Result>;
    
public:
    static int const NumAxes = Helper<(TypeListLength<TransformServicesList>::Value-1)>::AxisEndIndex;
    
public:
    struct Object : public ObjBase<CombineTransform, ParentObject, HelpersList> {};
};

APRINTER_ALIAS_STRUCT_EXT(CombineTransformService, (
    APRINTER_AS_TYPE(TransformServicesList)
), (
    template <typename Context, typename ParentObject, typename Config, typename FpType>
    using Transform = CombineTransform<Context, ParentObject, Config, FpType, CombineTransformService>;
))

#include <aprinter/EndNamespace.h>

#endif
