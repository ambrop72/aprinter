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

#ifndef AMBROLIB_STM32F4_SUPPORT_H
#define AMBROLIB_STM32F4_SUPPORT_H

#include <stdint.h>

#include <stm32f4xx.h>

#ifdef APRINTER_ENABLE_USB
#include <usbd_def.h>
#endif

#include <aprinter/platform/arm_cortex_common.h>

#define F_CPU (((double)HSE_VALUE * PLL_N_VALUE) / ((double)PLL_P_DIV_VALUE * PLL_M_VALUE))
#define APB1_TIMERS_DIV (APB1_PRESC_DIV == 1 ? APB1_PRESC_DIV : (APB1_PRESC_DIV / 2))
#define APB2_TIMERS_DIV (APB2_PRESC_DIV == 1 ? APB2_PRESC_DIV : (APB2_PRESC_DIV / 2))

#define INTERRUPT_PRIORITY 4

void platform_init (void);
void platform_init_final (void);

#ifdef APRINTER_ENABLE_USB
extern USBD_HandleTypeDef USBD_Device;
#endif

#endif
