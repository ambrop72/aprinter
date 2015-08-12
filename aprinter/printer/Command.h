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

#ifndef APRINTER_COMMAND_H
#define APRINTER_COMMAND_H

#include <stdint.h>
#include <string.h>

#include <aprinter/base/ProgramMemory.h>
#include <aprinter/base/Assert.h>
#include <aprinter/math/FloatTools.h>
#include <aprinter/math/PrintInt.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename FpType>
class Command {
public:
    using PartsSizeType = uint8_t;
    struct PartRef { void *ptr; };
    using SendBufEventHandler = void (*) (Context);
    
    virtual void finishCommand (Context c, bool no_ok = false) = 0;
    virtual bool tryLockedCommand (Context c) = 0;
    virtual bool tryUnplannedCommand (Context c) = 0;
    virtual bool tryPlannedCommand (Context c) = 0;
    
    virtual char getCmdCode (Context c) = 0;
    virtual uint16_t getCmdNumber (Context c) = 0;
    virtual PartsSizeType getNumParts (Context c) = 0;
    virtual PartRef getPart (Context c, PartsSizeType i) = 0;
    virtual char getPartCode (Context c, PartRef part) = 0;
    virtual FpType getPartFpValue (Context c, PartRef part) = 0;
    virtual uint32_t getPartUint32Value (Context c, PartRef part) = 0;
    virtual char const * getPartStringValue (Context c, PartRef part) = 0;
    
    virtual void reply_poke (Context c) = 0;
    virtual void reply_append_buffer (Context c, char const *str, size_t length) = 0;
    virtual void reply_append_ch (Context c, char ch) = 0;
    virtual void reply_append_pbuffer (Context c, AMBRO_PGM_P pstr, size_t length) = 0;
    
    virtual bool requestSendBufEvent (Context c, size_t length, SendBufEventHandler handler) = 0;
    virtual void cancelSendBufEvent (Context c) = 0;
    
    bool find_command_param (Context c, char code, PartRef *out_part)
    {
        PartsSizeType num_parts = getNumParts(c);
        for (PartsSizeType i = 0; i < num_parts; i++) {
            PartRef part = getPart(c, i);
            if (getPartCode(c, part) == code) {
                if (out_part) {
                    *out_part = part;
                }
                return true;
            }
        }
        return false;
    }
    
    uint32_t get_command_param_uint32 (Context c, char code, uint32_t default_value)
    {
        PartRef part;
        if (!find_command_param(c, code, &part)) {
            return default_value;
        }
        return getPartUint32Value(c, part);
    }
    
    FpType get_command_param_fp (Context c, char code, FpType default_value)
    {
        PartRef part;
        if (!find_command_param(c, code, &part)) {
            return default_value;
        }
        return getPartFpValue(c, part);
    }
    
    char const * get_command_param_str (Context c, char code, char const *default_value)
    {
        PartRef part;
        if (!find_command_param(c, code, &part)) {
            return default_value;
        }
        char const *str = getPartStringValue(c, part);
        if (!str) {
            return default_value;
        }
        return str;
    }
    
    bool find_command_param_fp (Context c, char code, FpType *out)
    {
        PartRef part;
        if (!find_command_param(c, code, &part)) {
            return false;
        }
        *out = getPartFpValue(c, part);
        return true;
    }
    
    void reply_append_str (Context c, char const *str)
    {
        reply_append_buffer(c, str, strlen(str));
    }
    
    void reply_append_pstr (Context c, AMBRO_PGM_P pstr)
    {
        reply_append_pbuffer(c, pstr, AMBRO_PGM_STRLEN(pstr));
    }
    
    void reply_append_fp (Context c, FpType x)
    {
        char buf[30];
#if defined(AMBROLIB_AVR)
        uint8_t len = AMBRO_PGM_SPRINTF(buf, AMBRO_PSTR("%g"), x);
        reply_append_buffer(c, buf, len);
#else        
        FloatToStrSoft(x, buf);
        reply_append_buffer(c, buf, strlen(buf));
#endif
    }
    
    void reply_append_uint32 (Context c, uint32_t x)
    {
        char buf[11];
#if defined(AMBROLIB_AVR)
        uint8_t len = AMBRO_PGM_SPRINTF(buf, AMBRO_PSTR("%" PRIu32), x);
#else
        uint8_t len = PrintNonnegativeIntDecimal<uint32_t>(x, buf);
#endif
        reply_append_buffer(c, buf, len);
    }
    
    void reply_append_uint16 (Context c, uint16_t x)
    {
        char buf[6];
#if defined(AMBROLIB_AVR)
        uint8_t len = AMBRO_PGM_SPRINTF(buf, AMBRO_PSTR("%" PRIu16), x);
#else
        uint8_t len = PrintNonnegativeIntDecimal<uint16_t>(x, buf);
#endif
        reply_append_buffer(c, buf, len);
    }
    
    void reply_append_uint8 (Context c, uint8_t x)
    {
        char buf[4];
#if defined(AMBROLIB_AVR)
        uint8_t len = AMBRO_PGM_SPRINTF(buf, AMBRO_PSTR("%" PRIu8), x);
#else
        uint8_t len = PrintNonnegativeIntDecimal<uint8_t>(x, buf);
#endif
        reply_append_buffer(c, buf, len);
    }
};

#include <aprinter/EndNamespace.h>

#endif
