/*
 * Adaptation for lwIP/Aprinter.
 * Based on in_cksum_arm.h of RTEMS.
 * 
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

/*
 * This file has been compiled by hand into inet_chksum_arm.S, which is the one
 * actually used during the build. The compile only works with gcc. With clang
 * there are problems with the order of registers used with the "ldmia" instructions.
 * 
 * The following command can be used to recompile this:
 * $(nix-build nix/ -A gcc-arm-embedded --no-out-link)/bin/arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -O1 -std=c99 -c aprinter/net/inet_chksum_arm.c -S -o aprinter/net/inet_chksum_arm.S
 */

/*
 * ARM version:
 *
 * Copyright (c) 1997 Mark Brinicome
 * Copyright (c) 1997 Causality Limited
 *
 * Based on the sparc version.
 */

/*
 * Sparc version:
 *
 * Copyright (c) 1995 Zubin Dittia.
 * Copyright (c) 1995 Matthew R. Green.
 * Copyright (c) 1994 Charles M. Hannum.
 * Copyright (c) 1992, 1993
 *      The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *      @(#)in_cksum.c  8.1 (Berkeley) 6/11/93
 */

#include <stdint.h>
#include <stddef.h>

#define ADD64   __asm __volatile("      \n\
        ldmia   %0!, {%2, %3, %4, %5}   \n\
        adds    %1,%7,%2; adcs %1,%1,%3 \n\
        adcs    %1,%1,%4; adcs %1,%1,%5 \n\
        ldmia   %0!, {%2, %3, %4, %5}   \n\
        adcs    %1,%1,%2; adcs %1,%1,%3 \n\
        adcs    %1,%1,%4; adcs %1,%1,%5 \n\
        ldmia   %0!, {%2, %3, %4, %5}   \n\
        adcs    %1,%1,%2; adcs %1,%1,%3 \n\
        adcs    %1,%1,%4; adcs %1,%1,%5 \n\
        ldmia   %0!, {%2, %3, %4, %5}   \n\
        adcs    %1,%1,%2; adcs %1,%1,%3 \n\
        adcs    %1,%1,%4; adcs %1,%1,%5 \n\
        adcs    %1,%1,#0\n"             \
        : "=r" (w), "=r" (sum), "=&r" (tmp1), "=&r" (tmp2), "=&r" (tmp3), "=&r" (tmp4) \
        : "0" (w), "r" (sum)    \
        : "cc")
        
#define ADD32   __asm __volatile("      \n\
        ldmia   %0!, {%2, %3, %4, %5}   \n\
        adds    %1,%7,%2; adcs %1,%1,%3 \n\
        adcs    %1,%1,%4; adcs %1,%1,%5 \n\
        ldmia   %0!, {%2, %3, %4, %5}   \n\
        adcs    %1,%1,%2; adcs %1,%1,%3 \n\
        adcs    %1,%1,%4; adcs %1,%1,%5 \n\
        adcs    %1,%1,#0\n"             \
        : "=r" (w), "=r" (sum), "=&r" (tmp1), "=&r" (tmp2), "=&r" (tmp3), "=&r" (tmp4) \
        : "0" (w), "r" (sum)    \
        : "cc")

#define ADD16   __asm __volatile("      \n\
        ldmia   %0!, {%2, %3, %4, %5}   \n\
        adds    %1,%7,%2; adcs %1,%1,%3 \n\
        adcs    %1,%1,%4; adcs %1,%1,%5 \n\
        adcs    %1,%1,#0\n"             \
        : "=r" (w), "=r" (sum), "=&r" (tmp1), "=&r" (tmp2), "=&r" (tmp3), "=&r" (tmp4) \
        : "0" (w), "r" (sum)    \
        : "cc")

#define ADD8    __asm __volatile("      \n\
        ldmia   %0!, {%2, %3}           \n\
        adds    %1,%5,%2; adcs %1,%1,%3 \n\
        adcs    %1,%1,#0\n"             \
        : "=r" (w), "=r" (sum), "=&r" (tmp1), "=&r" (tmp2)      \
        : "0" (w), "r" (sum)            \
        : "cc" )

#define ADD4    __asm __volatile("      \n\
        ldr     %2,[%0],#4              \n\
        adds    %1,%4,%2                \n\
        adcs    %1,%1,#0\n"             \
        : "=r" (w), "=r" (sum), "=&r" (tmp1) \
        : "0" (w), "r" (sum)            \
        : "cc")

/*#define REDUCE        {sum = (sum & 0xffff) + (sum >> 16);}*/
#define REDUCE  __asm __volatile("      \n\
        mov     %2, #0x00ff             \n\
        orr     %2, %2, #0xff00         \n\
        and     %2, %0, %2              \n\
        add     %0, %2, %0, lsr #16\n"  \
        : "=r" (sum)                    \
        : "0" (sum), "r" (tmp1))

#define ADDCARRY        {if (sum > 0xffff) sum -= 0xffff;}
#define ROL             {sum = sum << 8;}       /* depends on recent REDUCE */
#define ADDBYTE         {ROL; sum += (*w << 8); byte_swapped ^= 1;}
#define ADDSHORT        {sum += *(uint16_t *)w;}
#define ADVANCE(n)      {w += n; mlen -= n;}
#define ADVANCEML(n)    {mlen -= n;}

uint16_t IpChksumInverted (char const *data, size_t mlen)
{
    uint8_t *w = (uint8_t *)data;
    int byte_swapped = 0;
    uint32_t sum = 0;
    
    register uint32_t tmp1=0, tmp2, tmp3, tmp4;
    
    if ((3 & (uintptr_t)w) != 0) {
        if ((1 & (uintptr_t)w) != 0 && mlen >= 1) {
            ADDBYTE;
            ADVANCE(1);
        }
        if ((2 & (uintptr_t)w) != 0 && mlen >= 2) {
            ADDSHORT;
            ADVANCE(2);
        }
    }
    
    while (mlen >= 64) {
        ADD64;
        ADVANCEML(64);
    }
    if (mlen >= 32) {
        ADD32;
        ADVANCEML(32);
    }
    if (mlen >= 16) {
        ADD16;
        ADVANCEML(16);
    }
    if (mlen >= 8) {
        ADD8;
        ADVANCEML(8);
    }
    if (mlen >= 4) {
        ADD4;
        ADVANCEML(4)
    }
    if (mlen != 0) {
        REDUCE;
        if (mlen >= 2) {
                ADDSHORT;
                ADVANCE(2);
        }
        if (mlen == 1) {
                ADDBYTE;
        }
    }
    
    if (!byte_swapped) {
        REDUCE;
        ROL;
    }
    REDUCE;
    ADDCARRY;

    return sum;
}
