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
#include <aprinter/meta/AliasStruct.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename FpType, typename Params>
class IdentityTransform {
    AMBRO_DECLARE_LIST_FOREACH_HELPER(Foreach_copy_coords, copy_coords)
    
public:
    static int const NumAxes = Params::NumAxes;
    
    template <typename Src, typename Dst>
    static bool virtToPhys (Context c, Src virt, Dst out_phys)
    {
        ListForEachForward<HelperList>(Foreach_copy_coords(), virt, out_phys);
        return true;
    }
    
    template <typename Src, typename Dst>
    static void physToVirt (Context c, Src phys, Dst out_virt)
    {
        ListForEachForward<HelperList>(Foreach_copy_coords(), phys, out_virt);
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
    template <typename Context, typename ParentObject, typename Config, typename FpType>
    using Transform = IdentityTransform<Context, FpType, IdentityTransformService>;
))

#include <aprinter/EndNamespace.h>

#endif
