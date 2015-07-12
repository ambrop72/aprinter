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

#ifndef AMBROLIB_TEENSY3_SUPPORT_H
#define AMBROLIB_TEENSY3_SUPPORT_H

#include <stdint.h>
#include <mk20dx128.h>

#include <aprinter/platform/arm_cortex_common.h>

#define INTERRUPT_PRIORITY 4

// in accordance to startup code mk20dx128.c
#if F_CPU == 96000000 || F_CPU == 48000000
#define F_BUS 48000000
#elif F_CPU == 24000000
#define F_BUS 24000000
#else
#error F_CPU not recognized
#endif

typedef struct {
    union {
        uint8_t volatile u8;
        uint16_t volatile u16;
        uint32_t volatile u32;
    } volatile PORT[32];
    uint32_t RESERVED0[864];
    uint32_t volatile TER;
    uint32_t RESERVED1[15];
    uint32_t volatile TPR;
    uint32_t RESERVED2[15];
    uint32_t volatile TCR;
    uint32_t RESERVED3[29];
    uint32_t volatile IWR;
    uint32_t volatile IRR;
    uint32_t volatile IMCR;
    uint32_t RESERVED4[43];
    uint32_t volatile LAR;
    uint32_t volatile LSR;
    uint32_t RESERVED5[6];
    uint32_t volatile PID4;
    uint32_t volatile PID5;
    uint32_t volatile PID6;
    uint32_t volatile PID7;
    uint32_t volatile PID0;
    uint32_t volatile PID1;
    uint32_t volatile PID2;
    uint32_t volatile PID3;
    uint32_t volatile CID0;
    uint32_t volatile CID1;
    uint32_t volatile CID2;
    uint32_t volatile CID3;
} ITM_Type;

#define ITM ((ITM_Type *)0xE0000000UL)

#define ITM_TCR_ITMENA_Pos 0
#define ITM_TCR_ITMENA_Msk ((uint32_t)1 << ITM_TCR_ITMENA_Pos)

#endif
