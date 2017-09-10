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

#include <aipstack/misc/Preprocessor.h>
#include <aipstack/misc/Hints.h>

#ifdef AIPSTACK_CONFIG_ASSERT_INCLUDE
#include AIPSTACK_CONFIG_ASSERT_INCLUDE
#endif

#ifdef AIPSTACK_CONFIG_ASSERT_HANDLER
#define AIPSTACK_HAS_EXTERNAL_ASSERT_HANDLER 1
#define AIPSTACK_ASSERT_HANDLER AIPSTACK_CONFIG_ASSERT_HANDLER
#else
#define AIPSTACK_HAS_EXTERNAL_ASSERT_HANDLER 0
#define AIPSTACK_ASSERT_HANDLER(msg) \
    AIpStack_AssertAbort(__FILE__, __LINE__, msg)
#endif

#define AIPSTACK_ASSERT_ABORT(msg) \
    do { AIPSTACK_ASSERT_HANDLER(msg); } while (0)

#define AIPSTACK_ASSERT_FORCE(e) \
    { \
        if (e) {} else AIPSTACK_ASSERT_ABORT(#e); \
    }

#define AIPSTACK_ASSERT_FORCE_MSG(e, msg) \
    { \
        if (e) {} else AIPSTACK_ASSERT_ABORT(msg); \
    }

#ifdef AIPSTACK_CONFIG_ENABLE_ASSERTIONS
#define AIPSTACK_ASSERTIONS 1
#define AIPSTACK_ASSERT(e) AIPSTACK_ASSERT_FORCE(e)
#else
#define AIPSTACK_ASSERTIONS 0
#define AIPSTACK_ASSERT(e) {}
#endif

#if !AIPSTACK_HAS_EXTERNAL_ASSERT_HANDLER

#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C"
#endif
AIPSTACK_NO_INLINE AIPSTACK_NO_RETURN
inline void AIpStack_AssertAbort (char const *file, unsigned int line, char const *msg)
{
    fprintf(stderr, "AIpStack %s:%u: Assertion `%s' failed.\n", file, line, msg);
    abort();
}

#endif

#endif
