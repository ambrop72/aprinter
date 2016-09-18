/*
 * Copyright (c) 2016 Ambroz Bizjak
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

#ifndef APRINTER_STUB_PINS_H
#define APRINTER_STUB_PINS_H

#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/DebugObject.h>

#include <aprinter/BeginNamespace.h>

struct StubPin {};
struct StubPinInputMode {};
struct StubPinOutputMode {};

template <typename Arg>
class StubPins {
    using Context      = typename Arg::Context;
    using ParentObject = typename Arg::ParentObject;
    
public:
    struct Object;
    
private:
    using TheDebugObject = DebugObject<Context, Object>;
    
public:
    static void init (Context c)
    {
        TheDebugObject::init(c);
    }
    
    static void deinit (Context c)
    {
        TheDebugObject::deinit(c);
    }
    
    template <typename Pin, typename Mode=StubPinInputMode, typename ThisContext>
    static void setInput (ThisContext c)
    {
        TheDebugObject::access(c);
    }
    
    template <typename Pin, typename Mode=StubPinOutputMode, typename ThisContext>
    static void setOutput (ThisContext c)
    {
        TheDebugObject::access(c);
    }
    
    template <typename Pin, typename ThisContext>
    static bool get (ThisContext c)
    {
        TheDebugObject::access(c);
        return false;
    }
    
    template <typename Pin, typename ThisContext>
    static void set (ThisContext c, bool x)
    {
        TheDebugObject::access(c);
    }
    
    template <typename Pin>
    static void emergencySet (bool x)
    {
    }
    
public:
    struct Object : public ObjBase<StubPins, ParentObject, MakeTypeList<TheDebugObject>> {};
};

struct StubPinsService {
    APRINTER_ALIAS_STRUCT_EXT(Pins, (
        APRINTER_AS_TYPE(Context),
        APRINTER_AS_TYPE(ParentObject)
    ), (
        APRINTER_DEF_INSTANCE(Pins, StubPins)
    ))
};

#include <aprinter/EndNamespace.h>

#endif
