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

#ifndef APRINTER_OUTPUT_STREAM_H
#define APRINTER_OUTPUT_STREAM_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <inttypes.h>
#include <stdio.h>

#include <aprinter/base/ProgramMemory.h>
#include <aprinter/base/Hints.h>
#include <aprinter/math/FloatTools.h>
#include <aprinter/math/PrintInt.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename FpType>
class OutputStream {
public:
    virtual void reply_poke (Context c) = 0;
    virtual void reply_append_buffer (Context c, char const *str, size_t length) = 0;
#if AMBRO_HAS_NONTRANSPARENT_PROGMEM
    virtual void reply_append_pbuffer (Context c, AMBRO_PGM_P pstr, size_t length) = 0;
#endif
    
public:
#if !AMBRO_HAS_NONTRANSPARENT_PROGMEM
    void reply_append_pbuffer (Context c, AMBRO_PGM_P pstr, size_t length)
    {
        reply_append_buffer(c, pstr, length);
    }
#endif
    
    APRINTER_NO_INLINE
    void reply_append_str (Context c, char const *str)
    {
        reply_append_buffer(c, str, strlen(str));
    }
    
    APRINTER_NO_INLINE
    void reply_append_ch (Context c, char ch)
    {
        reply_append_buffer(c, &ch, 1);
    }
    
#if AMBRO_HAS_NONTRANSPARENT_PROGMEM
    APRINTER_NO_INLINE
    void reply_append_pstr (Context c, AMBRO_PGM_P pstr)
    {
        reply_append_pbuffer(c, pstr, AMBRO_PGM_STRLEN(pstr));
    }
#else
    void reply_append_pstr (Context c, AMBRO_PGM_P pstr)
    {
        reply_append_str(c, pstr);
    }
#endif
    
    APRINTER_NO_INLINE
    void reply_append_error (Context c, AMBRO_PGM_P errstr)
    {
        reply_append_pstr(c, AMBRO_PSTR("Error:"));
        reply_append_pstr(c, errstr);
        reply_append_ch(c, '\n');
    }
    
    APRINTER_NO_INLINE
    void reply_append_fp (Context c, FpType x)
    {
        char buf[30];
#if defined(AMBROLIB_AVR)
        uint8_t len = AMBRO_PGM_SPRINTF(buf, AMBRO_PSTR("%g"), x);
        reply_append_buffer(c, buf, len);
#else
        snprintf(buf, sizeof(buf), "%g", (double)x);
        reply_append_buffer(c, buf, strlen(buf));
#endif
    }
    
    APRINTER_NO_INLINE
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
};

#include <aprinter/EndNamespace.h>

#endif
