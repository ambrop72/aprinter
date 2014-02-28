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

#ifndef AMBROLIB_AT91SAM3U_SPI_H
#define AMBROLIB_AT91SAM3U_SPI_H

#include <aprinter/system/At91SamSpi.h>
#include <aprinter/system/At91Sam3uPins.h>

#include <aprinter/BeginNamespace.h>

using At91Sam3uSpiDevice = At91SamSpiDevice<
    GET_PERIPHERAL_ADDR(SPI),
    ID_SPI,
    SPI_IRQn, 
    At91Sam3uPin<At91Sam3uPioA, 15>,
    At91Sam3uPin<At91Sam3uPioA, 14>,
    At91Sam3uPin<At91Sam3uPioA, 13>
>;

template <typename Context, typename ParentObject, typename Handler, int CommandBufferBits>
using At91Sam3uSpi = At91SamSpi<Context, ParentObject, Handler, CommandBufferBits, At91Sam3uSpiDevice>;

#define AMBRO_AT91SAM3U_SPI_GLOBAL(thespi, context) \
extern "C" \
__attribute__((used)) \
void SPI_Handler (void) \
{ \
    thespi::spi_irq(MakeInterruptContext(context)); \
}

#include <aprinter/EndNamespace.h>

#endif
