/**
 * @file
 * Modules initialization
 *
 */

/*
 * Copyright (c) 2001-2004 Swedish Institute of Computer Science.
 * All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED 
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT 
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING 
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 * 
 * Author: Adam Dunkels <adam@sics.se>
 *
 */

#include "lwip/opt.h"
#include "lwip/init.h"
#include "lwip/stats.h"
#include "lwip/sys.h"
#include "lwip/memp.h"
#include "lwip/pbuf.h"
#include "lwip/netif.h"
#include "lwip/ip.h"
#include "lwip/raw.h"
#include "lwip/udp.h"
#include "lwip/tcp_impl.h"
#include "lwip/igmp.h"
#include "lwip/dns.h"
#include "lwip/timers.h"
#include "lwip/etharp.h"
#include "lwip/ip6.h"
#include "lwip/nd6.h"
#include "lwip/mld6.h"

/* Compile-time sanity checks for configuration errors.
 * These can be done independently of LWIP_DEBUG, without penalty.
 */
#ifndef LWIP_BYTE_ORDER
  #error "LWIP_BYTE_ORDER is not defined, you have to define it in your cc.h"
#endif
#if (!IP_SOF_BROADCAST && IP_SOF_BROADCAST_RECV)
  #error "If you want to use broadcast filter per pcb on recv operations, you have to define IP_SOF_BROADCAST=1 in your lwipopts.h"
#endif
#if (!LWIP_UDP && LWIP_UDPLITE)
  #error "If you want to use UDP Lite, you have to define LWIP_UDP=1 in your lwipopts.h"
#endif
#if (!LWIP_UDP && LWIP_DHCP)
  #error "If you want to use DHCP, you have to define LWIP_UDP=1 in your lwipopts.h"
#endif
#if (!LWIP_UDP && LWIP_MULTICAST_TX_OPTIONS)
  #error "If you want to use IGMP/LWIP_MULTICAST_TX_OPTIONS, you have to define LWIP_UDP=1 in your lwipopts.h"
#endif
#if (!LWIP_UDP && LWIP_DNS)
  #error "If you want to use DNS, you have to define LWIP_UDP=1 in your lwipopts.h"
#endif
#if (LWIP_ARP && ARP_QUEUEING && (MEMP_NUM_ARP_QUEUE<=0))
  #error "If you want to use ARP Queueing, you have to define MEMP_NUM_ARP_QUEUE>=1 in your lwipopts.h"
#endif
#if (LWIP_RAW && (MEMP_NUM_RAW_PCB<=0))
  #error "If you want to use RAW, you have to define MEMP_NUM_RAW_PCB>=1 in your lwipopts.h"
#endif
#if (LWIP_UDP && (MEMP_NUM_UDP_PCB<=0))
  #error "If you want to use UDP, you have to define MEMP_NUM_UDP_PCB>=1 in your lwipopts.h"
#endif
#if (LWIP_TCP && (MEMP_NUM_TCP_PCB<=0))
  #error "If you want to use TCP, you have to define MEMP_NUM_TCP_PCB>=1 in your lwipopts.h"
#endif
#if (LWIP_IGMP && (MEMP_NUM_IGMP_GROUP<=1))
  #error "If you want to use IGMP, you have to define MEMP_NUM_IGMP_GROUP>1 in your lwipopts.h"
#endif
#if (LWIP_IGMP && !LWIP_MULTICAST_TX_OPTIONS)
  #error "If you want to use IGMP, you have to define LWIP_MULTICAST_TX_OPTIONS==1 in your lwipopts.h"
#endif
#if (LWIP_IGMP && !LWIP_IPV4)
  #error "IGMP needs LWIP_IPV4 enabled in your lwipopts.h"
#endif
#if (LWIP_MULTICAST_TX_OPTIONS && !LWIP_IPV4)
  #error "LWIP_MULTICAST_TX_OPTIONS needs LWIP_IPV4 enabled in your lwipopts.h"
#endif
#if (IP_REASSEMBLY && (MEMP_NUM_REASSDATA > IP_REASS_MAX_PBUFS))
  #error "MEMP_NUM_REASSDATA > IP_REASS_MAX_PBUFS doesn't make sense since each struct ip_reassdata must hold 2 pbufs at least!"
#endif
#if (LWIP_TCP && (TCP_WND > 0xffff))
  #error "If you want to use TCP, TCP_WND must fit in an u16_t, so, you have to reduce it in your lwipopts.h (or enable window scaling)"
#endif
#if (LWIP_TCP && (TCP_SND_QUEUELEN > 0xffff))
  #error "If you want to use TCP, TCP_SND_QUEUELEN must fit in an u16_t, so, you have to reduce it in your lwipopts.h"
#endif
#if (LWIP_TCP && (TCP_SND_QUEUELEN < 2))
  #error "TCP_SND_QUEUELEN must be at least 2 for no-copy TCP writes to work"
#endif
#if (LWIP_TCP && ((TCP_MAXRTX > 12) || (TCP_SYNMAXRTX > 12)))
  #error "If you want to use TCP, TCP_MAXRTX and TCP_SYNMAXRTX must less or equal to 12 (due to tcp_backoff table), so, you have to reduce them in your lwipopts.h"
#endif
#if (LWIP_TCP && ((TCP_DEFAULT_LISTEN_BACKLOG < 0) || (TCP_DEFAULT_LISTEN_BACKLOG > 0xff)))
  #error "If you want to use TCP backlog, TCP_DEFAULT_LISTEN_BACKLOG must fit into an u8_t"
#endif
#if (((!LWIP_DHCP) || (!LWIP_ARP)) && DHCP_DOES_ARP_CHECK)
  #error "If you want to use DHCP ARP checking, you have to define LWIP_DHCP=1 and LWIP_ARP=1 in your lwipopts.h"
#endif
#if (DNS_LOCAL_HOSTLIST && !DNS_LOCAL_HOSTLIST_IS_DYNAMIC && !(defined(DNS_LOCAL_HOSTLIST_INIT)))
  #error "you have to define define DNS_LOCAL_HOSTLIST_INIT {{'host1', 0x123}, {'host2', 0x234}} to initialize DNS_LOCAL_HOSTLIST"
#endif
#if !LWIP_ETHERNET && (LWIP_ARP)
  #error "LWIP_ETHERNET needs to be turned on for LWIP_ARP"
#endif
#if (LWIP_IGMP || LWIP_IPV6) && !defined(LWIP_RAND)
  #error "When using IGMP or IPv6, LWIP_RAND() needs to be defined to a random-function returning an u32_t random value"
#endif

#ifndef LWIP_DISABLE_TCP_SANITY_CHECKS
#define LWIP_DISABLE_TCP_SANITY_CHECKS  0
#endif

/* TCP sanity checks */
#if !LWIP_DISABLE_TCP_SANITY_CHECKS
#if LWIP_TCP
#if MEMP_NUM_TCP_SEG < TCP_SND_QUEUELEN
  #error "lwip_sanity_check: WARNING: MEMP_NUM_TCP_SEG should be at least as big as TCP_SND_QUEUELEN. If you know what you are doing, define LWIP_DISABLE_TCP_SANITY_CHECKS to 1 to disable this error."
#endif
#if TCP_SND_BUF < (2 * TCP_MSS)
  #error "lwip_sanity_check: WARNING: TCP_SND_BUF must be at least as much as (2 * TCP_MSS) for things to work smoothly. If you know what you are doing, define LWIP_DISABLE_TCP_SANITY_CHECKS to 1 to disable this error."
#endif
#if TCP_SND_QUEUELEN < (2 * (TCP_SND_BUF / TCP_MSS))
  #error "lwip_sanity_check: WARNING: TCP_SND_QUEUELEN must be at least as much as (2 * TCP_SND_BUF/TCP_MSS) for things to work. If you know what you are doing, define LWIP_DISABLE_TCP_SANITY_CHECKS to 1 to disable this error."
#endif
#if PBUF_POOL_BUFSIZE <= (PBUF_LINK_ENCAPSULATION_HLEN + PBUF_LINK_HLEN + PBUF_IP_HLEN + PBUF_TRANSPORT_HLEN)
  #error "lwip_sanity_check: WARNING: PBUF_POOL_BUFSIZE does not provide enough space for protocol headers. If you know what you are doing, define LWIP_DISABLE_TCP_SANITY_CHECKS to 1 to disable this error."
#endif
#if TCP_WND > (PBUF_POOL_SIZE * (PBUF_POOL_BUFSIZE - (PBUF_LINK_ENCAPSULATION_HLEN + PBUF_LINK_HLEN + PBUF_IP_HLEN + PBUF_TRANSPORT_HLEN)))
  #error "lwip_sanity_check: WARNING: TCP_WND is larger than space provided by PBUF_POOL_SIZE * (PBUF_POOL_BUFSIZE - protocol headers). If you know what you are doing, define LWIP_DISABLE_TCP_SANITY_CHECKS to 1 to disable this error."
#endif
#if TCP_WND < TCP_MSS
  #error "lwip_sanity_check: WARNING: TCP_WND is smaller than MSS. If you know what you are doing, define LWIP_DISABLE_TCP_SANITY_CHECKS to 1 to disable this error."
#endif
#endif /* LWIP_TCP */
#endif /* !LWIP_DISABLE_TCP_SANITY_CHECKS */

/**
 * Initialize all modules.
 */
void
lwip_init(void)
{
  /* Modules initialization */
  stats_init();
  memp_init();
  pbuf_init();
  netif_init();
#if LWIP_IPV4
  ip_init();
#if LWIP_ARP
  etharp_init();
#endif /* LWIP_ARP */
#endif /* LWIP_IPV4 */
#if LWIP_RAW
  raw_init();
#endif /* LWIP_RAW */
#if LWIP_UDP
  udp_init();
#endif /* LWIP_UDP */
#if LWIP_TCP
  tcp_init();
#endif /* LWIP_TCP */
#if LWIP_IGMP
  igmp_init();
#endif /* LWIP_IGMP */
#if LWIP_DNS
  dns_init();
#endif /* LWIP_DNS */

  sys_timeouts_init();
}
