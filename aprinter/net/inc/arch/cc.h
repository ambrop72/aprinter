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

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <aprinter/base/Stringify.h>

typedef uint8_t u8_t;
typedef int8_t s8_t;
typedef uint16_t u16_t;
typedef int16_t s16_t;
typedef uint32_t u32_t;
typedef int32_t s32_t;
typedef uintptr_t mem_ptr_t;

#define PACK_STRUCT_STRUCT __attribute__((packed))

// TBD: Improve this to use Assert.h.
#define LWIP_PLATFORM_DIAG(x) do { printf x; } while (0)
#define LWIP_PLATFORM_ASSERT(x) do { puts(__FILE__ ":" AMBRO_STRINGIFY(__LINE__) x); while (1); } while (0)

#define U16_F PRIu16
#define S16_F PRId16
#define X16_F PRIx16
#define U32_F PRIu32
#define S32_F PRId32
#define X32_F PRIx32
#define SZT_F "zu"
#define X8_F "02"PRIx8

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define BYTE_ORDER LITTLE_ENDIAN
#define LWIP_PLATFORM_BYTESWAP 1
#define LWIP_PLATFORM_HTONS(x) __builtin_bswap16(x)
#define LWIP_PLATFORM_HTONL(x) __builtin_bswap32(x)

#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define BYTE_ORDER BIG_ENDIAN
#define LWIP_PLATFORM_BYTESWAP 1
#define LWIP_PLATFORM_HTONS(x) (x)
#define LWIP_PLATFORM_HTONL(x) (x)

#else
#error Unknown byte order
#endif
