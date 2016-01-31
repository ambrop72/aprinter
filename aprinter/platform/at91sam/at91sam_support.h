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

#ifndef AMBROLIB_AT91SAM_SUPPORT_H
#define AMBROLIB_AT91SAM_SUPPORT_H

#if defined(__SAM3X8E__)
#include <sam3xa.h>
#elif defined(__SAM3U4E__)
#include <sam3u.h>
#elif defined(__SAM3S2A__)
#include <sam3s.h>
#else
#error Unknown ASF include file for this chip
#endif

#include <aprinter/platform/arm_cortex_common.h>

#define F_SCLK CHIP_FREQ_SLCK_RC
#define F_MCK CHIP_FREQ_CPU_MAX
#define F_CPU F_MCK

#define INTERRUPT_PRIORITY 4

#define GET_PERIPHERAL_ADDR(x) ((uint32_t) GET_PERIPHERAL_ADDR_ x)
#define GET_PERIPHERAL_ADDR_(x) GET_PERIPHERAL_ADDR__ x
#define GET_PERIPHERAL_ADDR__(x)

void platform_init (void);

#endif
