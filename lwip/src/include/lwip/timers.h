/**
 * @file
 * Timer implementations
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
 *         Simon Goldschmidt
 *
 */
#ifndef LWIP_HDR_TIMERS_H
#define LWIP_HDR_TIMERS_H

#include "lwip/opt.h"
#include "lwip/def.h"
#include "lwip/err.h"

LWIP_EXTERN_C_BEGIN

#ifndef LWIP_DEBUG_TIMERNAMES
#ifdef LWIP_DEBUG
#define LWIP_DEBUG_TIMERNAMES TIMERS_DEBUG
#else
#define LWIP_DEBUG_TIMERNAMES 0
#endif
#endif

/** Function prototype for a timeout callback function. Register such a function
 * using sys_timeout().
 *
 * @param arg Additional argument to pass to the function - set up by sys_timeout()
 */
typedef void (* sys_timeout_handler)(void *arg);

struct sys_timeo {
  struct sys_timeo *next;
  u32_t time;
  sys_timeout_handler handler;
  void *arg;
#if LWIP_DEBUG_TIMERNAMES
  const char* handler_name;
#endif
};

void sys_timeouts_init(void);

#if LWIP_DEBUG_TIMERNAMES
void sys_timeout_debug(struct sys_timeo *timeout, u32_t msecs, sys_timeout_handler handler, void *arg, const char* handler_name);
#define sys_timeout(timeout, msecs, handler, arg) sys_timeout_debug(timeout, msecs, handler, arg, #handler)
#else
void sys_timeout      (struct sys_timeo *timeout, u32_t msecs, sys_timeout_handler handler, void *arg);
#endif

void sys_check_timeouts(u8_t max_timeouts_to_handle);
u8_t sys_timeouts_nextime(u32_t *out);

LWIP_EXTERN_C_END

#endif
