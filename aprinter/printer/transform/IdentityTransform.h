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

#ifndef AMBROLIB_IDENTITY_TRANSFORM_H
#define AMBROLIB_IDENTITY_TRANSFORM_H

#include <aprinter/meta/TypeListUtils.h>
#include <aprinter/meta/ListForEach.h>
#include <aprinter/meta/ServiceUtils.h>

#include <aprinter/BeginNamespace.h>

template <typename Arg>
class IdentityTransform {
    using Context = typename Arg::Context;
    using Params  = typename Arg::Params;
    
public:
    static int const NumAxes = Params::NumAxes;
    
    template <typename Src, typename Dst>
    static bool virtToPhys (Context c, Src virt, Dst out_phys)
    {
        ListForEachForward<HelperList>([&] APRINTER_TL(helper, helper::copy_coords(virt, out_phys)));
        return true;
    }
    
    template <typename Src, typename Dst>
    static void physToVirt (Context c, Src phys, Dst out_virt)
    {
        ListForEachForward<HelperList>([&] APRINTER_TL(helper, helper::copy_coords(phys, out_virt)));
    }
    
private:
    template <int AxisIndex>
    struct Helper {
        template <typename Src, typename Dst>
        static void copy_coords (Src src, Dst dst)
        {
            dst.template set<AxisIndex>(src.template get<AxisIndex>());
        }
    };
    using HelperList = IndexElemListCount<NumAxes, Helper>;
    
public:
    struct Object {};
};

APRINTER_ALIAS_STRUCT_EXT(IdentityTransformService, (
    APRINTER_AS_VALUE(int, NumAxes)
), (
    APRINTER_ALIAS_STRUCT_EXT(Transform, (
        APRINTER_AS_TYPE(Context),
        APRINTER_AS_TYPE(ParentObject),
        APRINTER_AS_TYPE(Config),
        APRINTER_AS_TYPE(FpType)
    ), (
        using Params = IdentityTransformService;
        APRINTER_DEF_INSTANCE(Transform, IdentityTransform)
    ))
))

#include <aprinter/EndNamespace.h>

#endif
