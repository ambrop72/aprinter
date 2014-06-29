/*
 * Copyright (c) 2014 Ambroz Bizjak
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

#ifndef APRINTER_CONSTEXPR_CONFIG_MANAGER_H
#define APRINTER_CONSTEXPR_CONFIG_MANAGER_H

#include <aprinter/meta/Expr.h>
#include <aprinter/meta/Object.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, typename ConfigList>
class ConstexprConfigManager {
public:
    struct Object;
    
private:
    template <typename Option>
    using OptionExpr = ConstantExpr<typename Option::Type, typename Option::DefaultValue>;
    
public:
    static void init (Context c)
    {
    }
    
    static void deinit (Context c)
    {
    }
    
    template <typename Option>
    static OptionExpr<Option> e (Option);
    
public:
    struct Object : public ObjBase<ConstexprConfigManager, ParentObject, EmptyTypeList> {};
};

struct ConstexprConfigManagerService {
    template <typename Context, typename ParentObject, typename ConfigList>
    using ConfigManager = ConstexprConfigManager<Context, ParentObject, ConfigList>;
};

#include <aprinter/EndNamespace.h>

#endif
