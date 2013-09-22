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

#include <sys/types.h>

#include "at91sam7s_support.h"

static void at91sam7s_default_irq (void)
{
}

static void at91sam7s_spurious_irq (void)
{
}

extern "C" {

__attribute__((used))
void at91sam7s_lowlevel_init (void)
{
    // Configure flash.
#ifdef AT91SAM7S512_H
    AT91C_BASE_EFC0->EFC_FMR = ((AT91C_MC_FMCN) & ((uint32_t)((F_MCK / 1000000.0) + 0.51) << 16)) | AT91C_MC_FWS_1FWS;
    AT91C_BASE_EFC0->EFC_FMR = ((AT91C_MC_FMCN) & ((uint32_t)((F_MCK / 1000000.0) + 0.51) << 16)) | AT91C_MC_FWS_1FWS;
#else
    AT91C_BASE_MC->MC0_FMR = ((AT91C_MC_FMCN) & ((uint32_t)((F_MCK / 1000000.0) + 0.51) << 16)) | AT91C_MC_FWS_1FWS;
#endif

    // Disable watchdog.
    AT91C_BASE_WDTC->WDTC_WDMR = AT91C_WDTC_WDDIS;
    
    // Initialize crystal.
    AT91C_BASE_PMC->PMC_MOR = ((AT91C_CKGR_OSCOUNT & (UINT32_C(8) << 8)) | AT91C_CKGR_MOSCEN);
    while (!(AT91C_BASE_PMC->PMC_SR & AT91C_PMC_MOSCS));
    
    // Initialize PLL.
    AT91C_BASE_PMC->PMC_PLLR = AT91C_CKGR_OUT_0 |
                     (AT91C_CKGR_PLLCOUNT & (UINT32_C(40) << 8)) |
                     (AT91C_CKGR_MUL & (UINT32_C(F_MUL - 1) << 16)) |
                     (AT91C_CKGR_DIV & F_DIV);
    while (!(AT91C_BASE_PMC->PMC_SR & AT91C_PMC_LOCK));
    
    // Setup clock prescaler and switch to PLL clock.
    AT91C_BASE_PMC->PMC_MCKR = AT91C_PMC_PRES_CLK_2;
    while (!(AT91C_BASE_PMC->PMC_SR & AT91C_PMC_MCKRDY));
    AT91C_BASE_PMC->PMC_MCKR |= AT91C_PMC_CSS_PLL_CLK;
    while (!(AT91C_BASE_PMC->PMC_SR & AT91C_PMC_MCKRDY));
    
    // Enable user reset.
    AT91C_BASE_RSTC->RSTC_RMR = 0xa5000400U | AT91C_RSTC_URSTEN;

    // Set default interrupt handlers.
    AT91C_BASE_AIC->AIC_IDCR = 0xFFFFFFFF;
    for (int i = 0; i < 31; i++) {
        AT91C_BASE_AIC->AIC_SVR[i] = (uint32_t)&at91sam7s_default_irq;
    }
    AT91C_BASE_AIC->AIC_SPU = (uint32_t)&at91sam7s_spurious_irq;
    
    // Unstack nested interrupts
    for (int i = 0; i < 8 ; i++) {
        AT91C_BASE_AIC->AIC_EOICR = 0;
    }
    
    // Make sure SRAM is mapped to Internal Memory Area 0, so that the vectors
    // table, which we have copied into SRAM as part of the .data section,
    // is actually in use.
    uint32_t *remap = (uint32_t *)0;
    uint32_t *ram = (uint32_t *)AT91C_ISRAM;
    uint32_t temp = *ram;
    *ram = temp + 1;
    if (*remap != *ram) {
        *ram = temp;
        AT91C_BASE_MC->MC_RCR = AT91C_MC_RCB;
    }
    
    // WTF
    AT91C_BASE_RTTC->RTTC_RTMR &= ~(AT91C_RTTC_ALMIEN | AT91C_RTTC_RTTINCIEN);
    AT91C_BASE_PITC->PITC_PIMR &= ~AT91C_PITC_PITIEN;
}

__attribute__((used))
void _init (void)
{
}

__attribute__((used))
void _fini (void)
{
}

__attribute__((used))
caddr_t _sbrk (int incr)
{
    extern char _data_end;
    static char *heap_end = 0;
    char *prev_heap_end;

    if (heap_end == 0) {
        heap_end = &_data_end;
    }
    prev_heap_end = heap_end;

    if ((heap_end + incr) > &_data_end + HEAP_SIZE) {
        while (1);
    }
    
    heap_end += incr;
    return (caddr_t)prev_heap_end;
}

__attribute__((used))
void *__dso_handle = 0;

}
