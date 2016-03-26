/*
 * Copyright (c) 2015 Ambroz Bizjak
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

// We expect these to be defined externally:
//#define APRINTER_NUM_TCP_CONN <count>
//#define APRINTER_NUM_TCP_CONN_QUEUED <count>
//#define APRINTER_NUM_TCP_LISTEN <count>
//#define APRINTER_TCP_RX_BUF <bytes>
//#define APRINTER_TCP_TX_BUF <bytes>
//#define APRINTER_MEM_ALIGNMENT <bytes>

// Simple options, mostly enable/disable.
#define NO_SYS 1
#define MEM_ALIGNMENT APRINTER_MEM_ALIGNMENT
#define ETH_PAD_SIZE 0
#define ARP_QUEUEING 0
#define IP_FRAG 0
#define IP_REASSEMBLY 0
#define LWIP_NETIF_HOSTNAME 1
#define LWIP_RAW 0
#define LWIP_DHCP 1
#define LWIP_NETCONN 0
#define LWIP_SOCKET 0
#define LWIP_STATS 0
#define LWIP_DHCP_CHECK_LINK_UP 1
#define LWIP_NETIF_STATUS_CALLBACK 1
#define LWIP_DISABLE_TCP_SANITY_CHECKS 1
#define LWIP_CHKSUM_ALGORITHM APRINTER_LWIP_CHKSUM_ALGORITHM

// Here you can enable debug messages from lwIP.
// They will be output via PrinterMain to the consoles.
/*
#define LWIP_DEBUG 1
#define TCP_DEBUG LWIP_DBG_ON
#define TCP_INPUT_DEBUG LWIP_DBG_ON
*/

// Size of ARP table. Add one extra entry for every TCP connection.
#define ARP_TABLE_SIZE (8 + APRINTER_NUM_TCP_CONN + APRINTER_NUM_TCP_CONN_QUEUED)

// Disable ARP queuing entirely.
// This is a custom feature implemented in lwIP.
// ARP queuing can exhaust the general allocator, since it may
// try to allocate up to ARP_TABLE_SIZE full-sized PBUF_RAM pbufs.
#define ARP_NO_QUEUING 1

// Do update the ARP table based on incoming packets.
// This means we can immediately respond to requests even
// though we have ARP queuing disabled.
#define ETHARP_TRUST_IP_MAC 1

// Number of UDP PCBs. Need just one for DHCP.
#define MEMP_NUM_UDP_PCB 1

// Number of TCP PCBs.
#define MEMP_NUM_TCP_PCB (APRINTER_NUM_TCP_CONN + APRINTER_NUM_TCP_CONN_QUEUED)
#define MEMP_NUM_TCP_PCB_LISTEN APRINTER_NUM_TCP_LISTEN

// Enable TCP listen backlog.
// Note that in LwipNetwork we decrement/increment pcb->backlog
// as connections are accepted/closed, in order to limit the combined
// number of pending and established connections.
#define TCP_LISTEN_BACKLOG 1

// Oversize can be disabled since it does nothing when tcp_write()
// is called without TCP_WRITE_FLAG_COPY.
#define TCP_OVERSIZE 0

// TCP Maximum Segment Size, use the Ethernet value.
#define TCP_MSS 1460

// TCP receive and send buffer sizes.
// Note that we actually have ring buffers of these sizes in
// LwipNetwork::TcpConnection, lwIP just needs to be aware of
// how large they are.
#define TCP_WND APRINTER_TCP_RX_BUF
#define TCP_SND_BUF APRINTER_TCP_TX_BUF

// Disable queuing of received out-of-sequence segments.
// This must be disabled because our RX code expects pbufs fed into the stack
// to be immediately freed, never queued.
#define TCP_QUEUE_OOSEQ 0

// Estimate how many TCP segments are needed to fully utilize the TX buffer space.
#define APRINTER_NUM_TCP_DATA_SEG ((TCP_SND_BUF + (TCP_MSS - 1)) / TCP_MSS)

// Maximum number of pbufs in the TCP send queue for a single connection.
// Note that currently lwIP only enforces this limit when adding new
// segments and not when adding a pbuf to an existing segment.
// Nevertheless we should not run out of pbufs due to a custom feature
// in tcp_out.c which resizes PBUF_ROM pbufs in place.
// We compute this based on our estimation of how many segments are needed,
// counting, each segment twice, since segments will typically have a header
// pbuf and a data pbuf. Allow one more to accomodate segments with one
// additional pbuf at ring buffer wrap-around.
#define TCP_SND_QUEUELEN (2 * APRINTER_NUM_TCP_DATA_SEG + 1)

// Number of TCP segments in the pool.
// For each connection we reserve:
// - APRINTER_NUM_TCP_DATA_SEG segments for data segments.
//   Note that TCP_SND_QUEUELEN should ensure that indeed no more than
//   this many data segments are queued.
// - 2 segments for segments created by tcp_enqueue_flags() (SYN/ACK, FIN).
#define MEMP_NUM_TCP_SEG (APRINTER_NUM_TCP_CONN * (APRINTER_NUM_TCP_DATA_SEG + 2) + APRINTER_NUM_TCP_CONN_QUEUED * 2)

// Number of pbufs in PBUF pool.
// These are allocated via pbuf_alloc(..., PBUF_ROM or PBUF_REF) and
// reference external data. They are used for:
// - In the TCP TX path (tcp_write), they reference application data
//   that is passed to tcp_write() without TCP_WRITE_FLAG_COPY.
//   Note that we patched lwIP so that it internally detects when
//   the buffer is a continuation of the previous buffer and extends
//   an existing pbuf instead of allocating a new one, when possible.
//   The magic number in this define is based on the fact these REF
//   pbufs always appear as part of TCP segments together with a header
//   pbuf; usually we have a single REF pbuf following a header, except
//   at buffer wrap-around we may have one extra REF pbuf.
// - In the fragmentation of IP packets, they reference parts of the
//   original full packet. Since we don't need and disable fragmentation,
//   we don't reserve anything for this.
#define MEMP_NUM_PBUF (APRINTER_NUM_TCP_CONN * (APRINTER_NUM_TCP_DATA_SEG + 1))

// Number of pbufs in PBUF_POOL pool.
// In a typical lwIP application these would be allocated by the drive for RX.
// However, we use them for the various *outgoing* packets; the lwIP code has
// been changed to allocate PBUF_POOL instead of PBUF_RAM pbufs in many places.
#define PBUF_POOL_SIZE 1
#define PBUF_POOL_BUFSIZE 384

// No need to specifically limit the TCP MSS against interface MTU,
// because the MTU is standard Ethernet and consistent with our TCP_MSS.
#define TCP_CALCULATE_EFF_SEND_MSS 0
