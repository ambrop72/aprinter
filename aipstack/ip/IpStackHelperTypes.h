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

#ifndef APRINTER_IPSTACK_IPSTACK_HELPER_TYPES_H
#define APRINTER_IPSTACK_IPSTACK_HELPER_TYPES_H

#include <stdint.h>

#include <aipstack/proto/IpAddr.h>
#include <aipstack/proto/Ip4Proto.h>
#include <aipstack/proto/Icmp4Proto.h>

#include <aipstack/BeginNamespace.h>

struct IpIfaceIp4AddrSetting {
    bool present;
    uint8_t prefix;
    Ip4Addr addr;
};

struct IpIfaceIp4GatewaySetting {
    bool present;
    Ip4Addr addr;
};

struct IpIfaceIp4Addrs {
    Ip4Addr addr;
    Ip4Addr netmask;
    Ip4Addr netaddr;
    Ip4Addr bcastaddr;
    uint8_t prefix;
};

struct IpIfaceDriverState {
    bool link_up;
};

struct IpSendFlags {
    enum : uint16_t {
        // These are real IP flags, the bits are correct but
        // relative to the high byte of FlagsOffset.
        DontFragmentFlag = Ip4FlagDF,
        
        // Mask of all allowed flags passed to send functions.
        AllFlags = DontFragmentFlag,
    };
};

struct Ip4DestUnreachMeta {
    uint8_t icmp_code;
    Icmp4RestType icmp_rest;
};

// Constains TTL and protocol (directly maps to IPv4 header bits).
class Ip4TtlProto {
public:
    uint16_t value;
    
public:
    Ip4TtlProto () = default;
    
    constexpr inline Ip4TtlProto (uint16_t ttl_proto)
    : value(ttl_proto)
    {
    }
    
    constexpr inline Ip4TtlProto (uint8_t ttl, uint8_t proto)
    : value(((uint16_t)ttl << 8) | proto)
    {
    }
    
    constexpr inline uint8_t ttl () const
    {
        return uint8_t(value >> 8);
    }
    
    constexpr inline uint8_t proto () const
    {
        return uint8_t(value);
    }
};

#include <aipstack/EndNamespace.h>

#endif
