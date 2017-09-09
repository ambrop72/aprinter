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

#ifndef APRINTER_IPSTACK_ERR_H
#define APRINTER_IPSTACK_ERR_H

#include <stdint.h>

namespace AIpStack {

/**
 * Error code enumeration used in various places, e.g.\ for sending packets.
 */
enum class IpErr : uint8_t {
    SUCCESS         = 0, /**< The operation was successful. */
    ARP_QUERY       = 1, /**< An ARP query is in progress and needs to complete. */
    NO_HEADER_SPACE = 2, /**< Insufficient header space is available in the buffer. */
    BUFFER_FULL     = 3, /**< The transmit buffer of the interface is full. */
    NO_HW_ROUTE     = 4, /**< Could not determine the hardware address to send to. */
    NO_IP_ROUTE     = 5, /**< Could not find an IP route for the packet. */
    PKT_TOO_LARGE   = 6, /**< The packet exceeds the MTU of the interface. */
    NO_PORT_AVAIL   = 7, /**< A local port could not be allocated. */
    NO_PCB_AVAIL    = 8, /**< A TCP PCB structure could be allocated. */
    NO_IPMTU_AVAIL  = 9, /**< An IP MTU reference could not be allocated. */
    FRAG_NEEDED     = 10, /**< IP fragmentation is needed but not permitted. */
    HW_ERROR        = 11, /**< An unexpected hardware problem has occured. */
    LINK_DOWN       = 12, /**< The link is down for the network interface. */
};

}

#endif
