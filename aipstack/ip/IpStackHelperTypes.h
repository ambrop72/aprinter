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

/**
 * Represents the IPv4 address configuration of a network interface.
 * 
 * Structures of this type are passed to @ref IpStack::Iface::setIp4Addr
 * and returned by @ref IpStack::Iface::getIp4Addr.
 */
struct IpIfaceIp4AddrSetting {
    /**
     * Whether an IP address is or should be assigned.
     * 
     * If this is false, then other members of this structure are meaningless.
     */
    bool present;
    
    /**
     * The subnet prefix length.
     */
    uint8_t prefix;
    
    /**
     * The IPv4 address.
     */
    Ip4Addr addr;
};

/**
 * Represents the IPv4 gateway configuration of a network interface.
 * 
 * Structures of this type are passed to @ref IpStack::Iface::setIp4Gateway
 * and returned by @ref IpStack::Iface::getIp4Gateway.
 */
struct IpIfaceIp4GatewaySetting {
    /**
     * Whether a gateway address is or should be assigned.
     * 
     * If this is false, then other members of this structure are meaningless.
     */
    bool present;
    
    /**
     * The gateway address.
     */
    Ip4Addr addr;
};

/**
 * Contains cached information about the IPv4 address configuration of a
 * network interface.
 * 
 * A pointer to a structure of this type can be obtained using
 * @ref IpStack::Iface::getIp4AddrsFromDriver. In addition to the IP address
 * and subnet prefix length, this structure contains the network mask,
 * network address and local broadcast address.
 */
struct IpIfaceIp4Addrs {
    /**
     * The IPv4 address.
     */
    Ip4Addr addr;
    
    /**
     * The network mask.
     */
    Ip4Addr netmask;
    
    /**
     * The network address.
     */
    Ip4Addr netaddr;
    
    /**
     * The local broadcast address.
     */
    Ip4Addr bcastaddr;
    
    /**
     * The subnet prefix length.
     */
    uint8_t prefix;
};

/**
 * Contains state reported by IP interface drivers to the IP stack.
 * 
 * Structures of this type are returned by @ref IpStack::Iface::getDriverState,
 * as well as by @ref IpStack::Iface::driverGetState as part of the driver
 * interface.
 */
struct IpIfaceDriverState {
    /**
     * Whether the link is up.
     */
    bool link_up;
};

/**
 * Contains definitions of flags as accepted by @ref IpStack::sendIp4Dgram
 * and @ref IpStack::prepareSendIp4Dgram.
 * 
 * This is only an empty class with flag definitions, not a flags type.
 */
struct IpSendFlags {
    enum : uint16_t {
        /**
         * Do-not-fragment flag.
         * 
         * Using this flag will both prevent fragmentation of the outgoing
         * datagram as well as set the Dont-Fragment flag in the IP header.
         */
        DontFragmentFlag = Ip4FlagDF,
        
        /**
         * Mask of all flags which may be passed to send functions.
         */
        AllFlags = DontFragmentFlag,
    };
};

/**
 * Contains information about a received ICMP Destination Unreachable message.
 */
struct Ip4DestUnreachMeta {
    /**
     * The ICMP code.
     * 
     * For example, @ref Icmp4CodeDestUnreachFragNeeded may be of interest.
     */
    uint8_t icmp_code;
    
    /**
     * The "Rest of Header" part of the ICMP header (4 bytes).
     */
    Icmp4RestType icmp_rest;
};

/**
 * Encapsulates a pair of IPv4 TTL and protocol values.
 * 
 * These are encoded in a 16-bit unsigned integer in the same manner
 * as in the IPv4 header. That is, the TTL is stored in the higher
 * 8 bits and the protocol in the lower 8 bits.
 */
class Ip4TtlProto {
public:
    /**
     * The encoded TLL and protocol.
     */
    uint16_t value;
    
public:
    /**
     * Default constructor, leaves the \ref value uninitialized.
     */
    Ip4TtlProto () = default;
    
    /**
     * Constructor from an encoded TTL and protocol value.
     * 
     * @param ttl_proto Encoded TTL and protocol. The \ref value field
     *        will be initialized to this value.
     */
    constexpr inline Ip4TtlProto (uint16_t ttl_proto)
    : value(ttl_proto)
    {
    }
    
    /**
     * Constructor from separate TTL and protocol values.
     * 
     * @param ttl The TTL.
     * @param proto The protocol.
     */
    constexpr inline Ip4TtlProto (uint8_t ttl, uint8_t proto)
    : value(((uint16_t)ttl << 8) | proto)
    {
    }
    
    /**
     * Returns the TTL.
     * 
     * @return The TTL.
     */
    constexpr inline uint8_t ttl () const
    {
        return uint8_t(value >> 8);
    }
    
    /**
     * Returns the protocol.
     * 
     * @return The protocol.
     */
    constexpr inline uint8_t proto () const
    {
        return uint8_t(value);
    }
};

#include <aipstack/EndNamespace.h>

#endif
