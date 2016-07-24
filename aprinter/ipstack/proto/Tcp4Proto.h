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

#ifndef APRINTER_IPSTACK_TCP4_PROTO_H
#define APRINTER_IPSTACK_TCP4_PROTO_H

#include <stdint.h>
#include <stddef.h>

#include <aprinter/ipstack/Struct.h>

#include <aprinter/BeginNamespace.h>

APRINTER_TSTRUCT(Tcp4Header,
    (SrcPort,     uint16_t)
    (DstPort,     uint16_t)
    (SeqNum,      uint32_t)
    (AckNum,      uint32_t)
    (OffsetFlags, uint16_t)
    (WindowSize,  uint16_t)
    (Checksum,    uint16_t)
    (UrgentPtr,   uint16_t)
)

static uint16_t const Tcp4FlagFin = (uint16_t)1 << 0;
static uint16_t const Tcp4FlagSyn = (uint16_t)1 << 1;
static uint16_t const Tcp4FlagRst = (uint16_t)1 << 2;
static uint16_t const Tcp4FlagPsh = (uint16_t)1 << 3;
static uint16_t const Tcp4FlagAck = (uint16_t)1 << 4;
static uint16_t const Tcp4FlagUrg = (uint16_t)1 << 5;
static uint16_t const Tcp4FlagEce = (uint16_t)1 << 6;
static uint16_t const Tcp4FlagCwr = (uint16_t)1 << 7;
static uint16_t const Tcp4FlagNs  = (uint16_t)1 << 8;

static uint16_t const Tcp4BasicFlags = Tcp4FlagFin|Tcp4FlagSyn|Tcp4FlagRst|Tcp4FlagAck;
static uint16_t const Tcp4SeqFlags = Tcp4FlagFin|Tcp4FlagSyn;

static int const TcpOffsetShift = 12;

static uint8_t const TcpOptionEnd = 0;
static uint8_t const TcpOptionNop = 1;
static uint8_t const TcpOptionMSS = 2;

static uint8_t const TcpOptionLenMSS = 4;

#include <aprinter/EndNamespace.h>

#endif
