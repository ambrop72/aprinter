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

#ifndef AIPSTACK_ASSERT_H
#define AIPSTACK_ASSERT_H

#include <stdio.h>
#include <stdlib.h>
#ifdef AMBROLIB_AVR
#include <avr/pgmspace.h>
#endif

#include <aipstack/misc/Preprocessor.h>
#include <aipstack/misc/Hints.h>
#include <aipstack/misc/ProgramMemory.h>

#define AIPSTACK_ASSERT_ABORT(msg) \
    do { \
        AIpStack_AssertAbort(AIPSTACK_PSTR(msg)); \
    } while (0)

#define AIPSTACK_ASSERT_FORCE(e) \
    { \
        if (!(e)) AIPSTACK_ASSERT_ABORT("BUG " __FILE__ ":" AIPSTACK_STRINGIFY(__LINE__)); \
    }

#define AIPSTACK_ASSERT_FORCE_MSG(e, msg) \
    { \
        if (!(e)) AIPSTACK_ASSERT_ABORT(msg " at " __FILE__ ":" AIPSTACK_STRINGIFY(__LINE__)); \
    }

#ifdef AMBROLIB_ASSERTIONS
#define AIPSTACK_ASSERT(e) AIPSTACK_ASSERT_FORCE(e)
#else
#define AIPSTACK_ASSERT(e) {}
#endif

#ifdef __cplusplus
extern "C"
#endif
AIPSTACK_NO_INLINE AIPSTACK_NO_RETURN
inline void AIpStack_AssertAbort (char const *msg)
{
#ifdef AMBROLIB_EMERGENCY_ACTION
    AMBROLIB_EMERGENCY_ACTION
#endif
    
#if !defined(AMBROLIB_NO_PRINT)
#ifdef AMBROLIB_AVR
    puts_P(msg);
#else
    puts(msg);
#endif
#endif
    
#ifdef AMBROLIB_ABORT_ACTION
    AMBROLIB_ABORT_ACTION
#else
    abort();
#endif
}

#endif
