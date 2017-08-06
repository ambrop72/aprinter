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

#ifndef AMBROLIB_PROGRAM_MEMORY_H
#define AMBROLIB_PROGRAM_MEMORY_H

#include <string.h>
#include <stdio.h>
#ifdef AMBROLIB_AVR
#include <avr/pgmspace.h>
#endif

#ifdef AMBROLIB_AVR

#define AMBRO_HAS_NONTRANSPARENT_PROGMEM 1
#define AMBRO_PROGMEM PROGMEM
#define AMBRO_PSTR(x) PSTR(x)
#define AMBRO_PGM_P PGM_P
#define AMBRO_PGM_MEMCPY memcpy_P
#define AMBRO_PGM_STRLEN strlen_P
#define AMBRO_PGM_SPRINTF sprintf_P
#define AMBRO_PGM_STRCMP strcmp_P
#define AMBRO_PGM_READBYTE(x) (pgm_read_byte((x)))

#else

#define AMBRO_HAS_NONTRANSPARENT_PROGMEM 0
#define AMBRO_PROGMEM
#define AMBRO_PSTR(x) (x)
#define AMBRO_PGM_P char const *
#define AMBRO_PGM_MEMCPY memcpy
#define AMBRO_PGM_STRLEN strlen
#define AMBRO_PGM_SPRINTF sprintf
#define AMBRO_PGM_STRCMP strcmp
#define AMBRO_PGM_READBYTE(x) (*(unsigned char const *)(x))

#endif

#ifdef __cplusplus

namespace APrinter {

template <typename T>
class ProgPtr {
public:
    static constexpr ProgPtr Make (T const *ptr)
    {
        return ProgPtr{ptr};
    }
    
    T operator* () const
    {
#if AMBRO_HAS_NONTRANSPARENT_PROGMEM
        T val;
        if (sizeof(T) == 1) {
            *(unsigned char *)&val = AMBRO_PGM_READBYTE(m_ptr);
        } else {
            AMBRO_PGM_MEMCPY(&val, m_ptr, sizeof(T));
        }
        return val;
#else
        return *m_ptr;
#endif
    }
    
    ProgPtr operator+ (size_t i) const
    {
        return Make(m_ptr + i);
    }
    
    ProgPtr & operator++ ()
    {
        m_ptr++;
        return *this;
    }
    
    T operator[] (size_t i) const
    {
        return (operator+(i)).operator*();
    }
    
public:
    T const *m_ptr;
};

}

#endif

#endif
