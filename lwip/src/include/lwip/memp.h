/**
 * @file
 * Memory pool API
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

#ifndef LWIP_HDR_MEMP_H
#define LWIP_HDR_MEMP_H

#include "lwip/opt.h"
#include "lwip/def.h"

LWIP_EXTERN_C_BEGIN

/* do we have the memp_desc array of pool names */
#define LWIP_MEMP_HAVE_DESC (defined(LWIP_DEBUG) || LWIP_STATS_DISPLAY)

/* Create the list of all memory pools managed by memp. MEMP_MAX represents a NULL pool at the end */
typedef enum {
#define LWIP_MEMPOOL(name,num,elem_type,desc)  MEMP_##name,
#include "lwip/memp_std.h"
  MEMP_MAX
} memp_t;

void  memp_init(void);

#if MEMP_OVERFLOW_CHECK
void *memp_malloc_fn(memp_t type, const char* file, int line);
#define memp_malloc(t) memp_malloc_fn((t), __FILE__, __LINE__)
#else
void *memp_malloc(memp_t type);
#endif
void  memp_free(memp_t type, void *mem);

#if LWIP_MEMP_HAVE_DESC
extern const char *const memp_desc[MEMP_MAX];
#endif

LWIP_EXTERN_C_END

#endif
