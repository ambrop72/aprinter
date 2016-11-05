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

#ifndef APRINTER_IPSTACK_IP4_PROTO_H
#define APRINTER_IPSTACK_IP4_PROTO_H

#include <stdint.h>

#include <aprinter/ipstack/misc/Struct.h>
#include <aprinter/ipstack/proto/IpAddr.h>

#include <aprinter/BeginNamespace.h>

APRINTER_TSTRUCT(Ip4Header,
    (VersionIhl,   uint8_t)
    (DscpEcn,      uint8_t)
    (TotalLen,     uint16_t)
    (Ident,        uint16_t)
    (FlagsOffset,  uint16_t)
    (TimeToLive,   uint8_t)
    (Protocol,     uint8_t)
    (HeaderChksum, uint16_t)
    (SrcAddr,      Ip4Addr)
    (DstAddr,      Ip4Addr)
)

static int const Ip4VersionShift = 4;
static uint8_t const Ip4IhlMask = 0xF;

static uint16_t const Ip4FlagDF = (uint16_t)1 << 14;
static uint16_t const Ip4FlagMF = (uint16_t)1 << 13;

static uint16_t const Ip4OffsetMask = UINT16_C(0x1fff);

static uint8_t const Ip4ProtocolIcmp = 1;
static uint8_t const Ip4ProtocolTcp  = 6;
static uint8_t const Ip4ProtocolUdp  = 17;

#include <aprinter/EndNamespace.h>

#endif
