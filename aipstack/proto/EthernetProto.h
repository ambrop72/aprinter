/*
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

#ifndef APRINTER_IPSTACK_ETHERNET_PROTO_H
#define APRINTER_IPSTACK_ETHERNET_PROTO_H

#include <stdint.h>

#include <aipstack/misc/Struct.h>

#include <aprinter/BeginNamespace.h>

class MacAddr : public StructByteArray<6>
{
public:
    static inline constexpr MacAddr ZeroAddr ()
    {
        return MacAddr{};
    }
    
    static inline constexpr MacAddr BroadcastAddr ()
    {
        MacAddr result = {};
        for (int i = 0; i < MacAddr::Size; i++) {
            result.data[i] = 0xFF;
        }
        return result;
    }
};

APRINTER_TSTRUCT(EthHeader,
    (DstMac,  MacAddr)
    (SrcMac,  MacAddr)
    (EthType, uint16_t)
)

static uint16_t const EthTypeIpv4 = UINT16_C(0x0800);
static uint16_t const EthTypeArp  = UINT16_C(0x0806);

#include <aprinter/EndNamespace.h>

#endif
