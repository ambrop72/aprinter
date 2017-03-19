/*
 * Copyright (c) 2017 Ambroz Bizjak
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

#ifndef APRINTER_IPSTACK_DHCP_PROTO_H
#define APRINTER_IPSTACK_DHCP_PROTO_H

#include <stdint.h>

#include <aipstack/misc/Struct.h>
#include <aipstack/proto/IpAddr.h>

#include <aipstack/BeginNamespace.h>

enum class DhcpOp : uint8_t {
    BootRequest = 1,
    BootReply = 2,
};

enum class DhcpHwAddrType : uint8_t {
    Ethernet = 1,
};

static uint32_t const DhcpMagicNumber = UINT32_C(0x63825363);

static uint16_t const DhcpServerPort = 67;
static uint16_t const DhcpClientPort = 68;

enum class DhcpOptionType : uint8_t {
    Pad = 0,
    End = 255,
    SubnetMask = 1,
    Router = 3,
    DomainNameServer = 6,
    HostName = 12,
    RequestedIpAddress = 50,
    IpAddressLeaseTime = 51,
    DhcpMessageType = 53,
    DhcpServerIdentifier = 54,
    ParameterRequestList = 55,
    MaximumMessageSize = 57,
    RenewalTimeValue = 58,
    RebindingTimeValue = 59,
    VendorClassIdentifier = 60,
    ClientIdentifier = 61,
};

enum class DhcpMessageType : uint8_t {
    Discover = 1,
    Offer = 2,
    Request = 3,
    Decline = 4,
    Ack = 5,
    Nak = 6,
    Release = 7,
};

APRINTER_TSTRUCT(DhcpHeader,
    (DhcpOp,      AIpStack::DhcpOp)
    (DhcpHtype,   DhcpHwAddrType)
    (DhcpHlen,    uint8_t)
    (DhcpHops,    uint8_t)
    (DhcpXid,     uint32_t)
    (DhcpSecs,    uint16_t)
    (DhcpFlags,   uint16_t)
    (DhcpCiaddr,  Ip4Addr)
    (DhcpYiaddr,  Ip4Addr)
    (DhcpSiaddr,  Ip4Addr)
    (DhcpGiaddr,  Ip4Addr)
    (DhcpChaddr,  StructByteArray<16>)
    (DhcpSname,   StructByteArray<64>)
    (DhcpFile,    StructByteArray<128>)
    (DhcpMagic,   uint32_t)
)

APRINTER_TSTRUCT(DhcpOptionHeader,
    (OptType,     DhcpOptionType)
    (OptLen,      uint8_t)
)

APRINTER_TSTRUCT(DhcpOptMsgType,
    (MsgType,     DhcpMessageType)
)

APRINTER_TSTRUCT(DhcpOptMaxMsgSize,
    (MaxMsgSize,  uint16_t)
)

APRINTER_TSTRUCT(DhcpOptServerId,
    (ServerId,    uint32_t)
)

APRINTER_TSTRUCT(DhcpOptTime,
    (Time,        uint32_t)
)

APRINTER_TSTRUCT(DhcpOptAddr,
    (Addr,        Ip4Addr)
)

#include <aipstack/EndNamespace.h>

#endif
