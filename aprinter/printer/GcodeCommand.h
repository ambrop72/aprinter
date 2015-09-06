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

#ifndef APRINTER_GCODE_COMMAND_H
#define APRINTER_GCODE_COMMAND_H

#include <stdint.h>

#include <aprinter/meta/TypesAreEqual.h>

#include <aprinter/BeginNamespace.h>

enum GcodeError {
    GCODE_ERROR_NO_PARTS = -1,
    GCODE_ERROR_TOO_MANY_PARTS = -2,
    GCODE_ERROR_INVALID_PART = -3,
    GCODE_ERROR_CHECKSUM = -4,
    GCODE_ERROR_RECV_OVERRUN = -5,
    GCODE_ERROR_EOF = -6,
    GCODE_ERROR_BAD_ESCAPE = -7
};

template <typename Context, typename FpType>
class GcodeCommand {
public:
    using PartsSizeType = int8_t;
    
    struct PartRef {
        void *ptr;
    };
    
    virtual char getCmdCode (Context c) = 0;
    virtual uint16_t getCmdNumber (Context c) = 0;
    virtual PartsSizeType getNumParts (Context c) = 0;
    virtual PartRef getPart (Context c, PartsSizeType i) = 0;
    virtual char getPartCode (Context c, PartRef part) = 0;
    virtual FpType getPartFpValue (Context c, PartRef part) = 0;
    virtual uint32_t getPartUint32Value (Context c, PartRef part) = 0;
    virtual char const * getPartStringValue (Context c, PartRef part) = 0;
};

template <typename Context, typename FpType, typename TheImpl>
class GcodeCommandWrapper : public GcodeCommand<Context, FpType> {
private:
    using TheGcodeCommand = GcodeCommand<Context, FpType>;
    using PartsSizeType = typename TheGcodeCommand::PartsSizeType;
    using PartRef = typename TheGcodeCommand::PartRef;
    static_assert(TypesAreEqual<typename TheImpl::PartsSizeType, PartsSizeType>::Value, "");
    using ImplPartRef = typename TheImpl::PartRef;
    
    char getCmdCode (Context c)
    {
        return TheImpl::getCmdCode(c);
    }
    
    uint16_t getCmdNumber (Context c)
    {
        return TheImpl::getCmdNumber(c);
    }
    
    PartsSizeType getNumParts (Context c)
    {
        return TheImpl::getNumParts(c);
    }
    
    PartRef getPart (Context c, PartsSizeType i)
    {
        return PartRef{TheImpl::getPart(c, i)};
    }
    
    char getPartCode (Context c, PartRef part)
    {
        return TheImpl::getPartCode(c, (ImplPartRef)part.ptr);
    }
    
    FpType getPartFpValue (Context c, PartRef part)
    {
        return TheImpl::template getPartFpValue<FpType>(c, (ImplPartRef)part.ptr);
    }
    
    uint32_t getPartUint32Value (Context c, PartRef part)
    {
        return TheImpl::getPartUint32Value(c, (ImplPartRef)part.ptr);
    }
    
    char const * getPartStringValue (Context c, PartRef part)
    {
        return TheImpl::getPartStringValue(c, (ImplPartRef)part.ptr);
    }
};

#include <aprinter/EndNamespace.h>

#endif
