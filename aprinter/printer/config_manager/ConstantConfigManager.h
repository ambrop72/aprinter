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

#ifndef APRINTER_CONSTANT_CONFIG_MANAGER_H
#define APRINTER_CONSTANT_CONFIG_MANAGER_H

#include <aprinter/meta/Expr.h>
#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/meta/BasicMetaUtils.h>
#include <aprinter/base/Object.h>

namespace APrinter {

template <typename Arg>
class ConstantConfigManager {
    using Context      = typename Arg::Context;
    using ParentObject = typename Arg::ParentObject;
    
public:
    struct Object;
    
private:
    template <typename Option>
    using OptionExpr = ConstantExpr<typename Option::Type, typename Option::DefaultValue>;
    
public:
    static bool const HasStore = false;
    
    static void init (Context c)
    {
    }
    
    static void deinit (Context c)
    {
    }
    
    template <typename TheCommand>
    static bool checkCommand (Context c, TheCommand *cmd)
    {
        return true;
    }

    static void clearApplyPending (Context c)
    {
    }
    
    template <typename Option>
    static OptionExpr<Option> e (Option);
    
public:
    struct Object : public ObjBase<ConstantConfigManager, ParentObject, EmptyTypeList> {};
};

struct ConstantConfigManagerService {
    APRINTER_ALIAS_STRUCT_EXT(ConfigManager, (
        APRINTER_AS_TYPE(Context),
        APRINTER_AS_TYPE(ParentObject),
        APRINTER_AS_TYPE(ConfigOptionsList),
        APRINTER_AS_TYPE(ThePrinterMain),
        APRINTER_AS_TYPE(Handler)
    ), (
        APRINTER_DEF_INSTANCE(ConfigManager, ConstantConfigManager)
    ))
};

}

#endif
