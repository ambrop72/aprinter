/**
 * @file
 * Support for different processor and compiler architectures
 */

/*
 * Copyright (c) 2001-2004 Swedish Institute of Computer Science.
 * All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED 
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT 
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING 
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 * 
 * Author: Adam Dunkels <adam@sics.se>
 *
 */
#ifndef LWIP_HDR_ARCH_H
#define LWIP_HDR_ARCH_H

#define LWIP_LITTLE_ENDIAN 1234
#define LWIP_BIG_ENDIAN 4321

// Have this here because this header doesn't include other lwip headers.
#ifdef __cplusplus
#define LWIP_EXTERN_C_BEGIN extern "C" {
#define LWIP_EXTERN_C_END }
#else
#define LWIP_EXTERN_C_BEGIN
#define LWIP_EXTERN_C_END
#endif

#include "arch/cc.h"

/** Define this to 1 in arch/cc.h of your port if your compiler does not provide
 * the stdint.h header. This cannot be #defined in lwipopts.h since 
 * this is not an option of lwIP itself, but an option of the lwIP port
 * to your system.
 * Additionally, this header is meant to be #included in lwipopts.h
 * (you may need to declare function prototypes in there).
 */
#ifndef LWIP_NO_STDINT_H
#define LWIP_NO_STDINT_H 0
#endif

/* Define generic types used in lwIP */
#if !LWIP_NO_STDINT_H
#include <stdint.h>
typedef uint8_t   u8_t;
typedef int8_t    s8_t;
typedef uint16_t  u16_t;
typedef int16_t   s16_t;
typedef uint32_t  u32_t;
typedef int32_t   s32_t;
typedef uintptr_t mem_ptr_t;
#endif

/** Define this to 1 in arch/cc.h of your port if your compiler does not provide
 * the inttypes.h header. This cannot be #defined in lwipopts.h since 
 * this is not an option of lwIP itself, but an option of the lwIP port
 * to your system.
 * Additionally, this header is meant to be #included in lwipopts.h
 * (you may need to declare function prototypes in there).
 */
#ifndef LWIP_NO_INTTYPES_H
#define LWIP_NO_INTTYPES_H 0
#endif

/* Define (sn)printf formatters for these lwIP types */
#if !LWIP_NO_INTTYPES_H
#include <inttypes.h>
#ifndef X8_F
#define X8_F  "02" PRIx8
#endif
#ifndef U16_F
#define U16_F PRIu16
#endif
#ifndef S16_F
#define S16_F PRId16
#endif
#ifndef X16_F
#define X16_F PRIx16
#endif
#ifndef U32_F
#define U32_F PRIu32
#endif
#ifndef S32_F
#define S32_F PRId32
#endif
#ifndef X32_F
#define X32_F PRIx32
#endif
#ifndef SZT_F
#define SZT_F PRIuPTR
#endif
#endif

#ifndef PACK_STRUCT_BEGIN
#define PACK_STRUCT_BEGIN
#endif /* PACK_STRUCT_BEGIN */

#ifndef PACK_STRUCT_END
#define PACK_STRUCT_END
#endif /* PACK_STRUCT_END */

#ifndef PACK_STRUCT_STRUCT
#define PACK_STRUCT_STRUCT
#endif /* PACK_STRUCT_STRUCT */

#ifndef PACK_STRUCT_FIELD
#define PACK_STRUCT_FIELD(x) x
#endif /* PACK_STRUCT_FIELD */

/* Used for struct fields of u8_t,
 * where some compilers warn that packing is not necessary */
#ifndef PACK_STRUCT_FLD_8
#define PACK_STRUCT_FLD_8(x) PACK_STRUCT_FIELD(x)
#endif /* PACK_STRUCT_FLD_8 */

/* Used for struct fields of that are packed structs themself,
 * where some compilers warn that packing is not necessary */
#ifndef PACK_STRUCT_FLD_S
#define PACK_STRUCT_FLD_S(x) PACK_STRUCT_FIELD(x)
#endif /* PACK_STRUCT_FLD_S */

#ifndef LWIP_UNUSED_ARG
#define LWIP_UNUSED_ARG(x) (void)x
#endif /* LWIP_UNUSED_ARG */

#endif /* LWIP_HDR_ARCH_H */
