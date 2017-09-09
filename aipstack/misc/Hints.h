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

#ifndef AIPSTACK_HINTS_H
#define AIPSTACK_HINTS_H

#ifdef __GNUC__

#define AIPSTACK_LIKELY(x) __builtin_expect((x), 1)
#define AIPSTACK_UNLIKELY(x) __builtin_expect((x), 0)
#define AIPSTACK_ALWAYS_INLINE __attribute__((always_inline)) inline
#define AIPSTACK_NO_INLINE __attribute__((noinline))
#define AIPSTACK_NO_RETURN __attribute__((noreturn))
#define AIPSTACK_RESTRICT __restrict__

#ifndef __clang__
#define AIPSTACK_UNROLL_LOOPS __attribute__((optimize("unroll-loops")))
#define AIPSTACK_OPTIMIZE_SIZE __attribute__((optimize("Os")))
#else
#define AIPSTACK_UNROLL_LOOPS
#define AIPSTACK_OPTIMIZE_SIZE
#endif

#else

#define AIPSTACK_LIKELY(x) (x)
#define AIPSTACK_UNLIKELY(x) (x)
#define AIPSTACK_ALWAYS_INLINE
#define AIPSTACK_NO_INLINE
#define AIPSTACK_NO_RETURN
#define AIPSTACK_RESTRICT
#define AIPSTACK_UNROLL_LOOPS
#define AIPSTACK_OPTIMIZE_SIZE

#endif

#endif
