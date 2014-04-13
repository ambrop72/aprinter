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

#ifndef AMBROLIB_COREXY_TRANSFORM_H
#define AMBROLIB_COREXY_TRANSFORM_H

#include <aprinter/printer/NoSplitter.h>

#include <aprinter/BeginNamespace.h>

struct CoreXyTransformParams {};

template <typename Params, typename FpType>
class CoreXyTransform {
public:
    static int const NumAxes = 2;
    
    template <typename Src, typename Dst>
    static void virtToPhys (Src virt, Dst out_phys)
    {
        out_phys.template set<0>(virt.template get<0>() + virt.template get<1>());
        out_phys.template set<1>(virt.template get<0>() - virt.template get<1>());
    }
    
    template <typename Src, typename Dst>
    static void physToVirt (Src phys, Dst out_virt)
    {
        out_virt.template set<0>(0.5f * (phys.template get<0>() + phys.template get<1>()));
        out_virt.template set<1>(0.5f * (phys.template get<0>() - phys.template get<1>()));
    }
    
    using Splitter = NoSplitter<FpType>;
};

#include <aprinter/EndNamespace.h>

#endif
