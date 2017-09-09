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

#ifndef AIPSTACK_ICMP4_PROTO_H
#define AIPSTACK_ICMP4_PROTO_H

#include <stdint.h>

#include <aipstack/common/Struct.h>
#include <aipstack/misc/BinaryTools.h>

namespace AIpStack {

using Icmp4RestType = StructByteArray<4>;

#ifndef DOXYGEN_SHOULD_SKIP_THIS

AIPSTACK_DEFINE_STRUCT(Icmp4Header,
    (Type,         uint8_t)
    (Code,         uint8_t)
    (Chksum,       uint16_t)
    (Rest,         Icmp4RestType)
)

#endif

static uint8_t const Icmp4TypeEchoReply   = 0;
static uint8_t const Icmp4TypeEchoRequest = 8;
static uint8_t const Icmp4TypeDestUnreach = 3;

/**
 * ICMP code "fragmentation needed" for "destination unreachable" type.
 */
static uint8_t const Icmp4CodeDestUnreachFragNeeded = 4;

inline static uint16_t const Icmp4GetMtuFromRest (Icmp4RestType rest)
{
    return ReadBinaryInt<uint16_t, BinaryBigEndian>((char const *)rest.data + 2);
}

}

#endif
