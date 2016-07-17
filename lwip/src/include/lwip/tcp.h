/**
 * @file
 * TCP API (to be used from TCPIP thread)
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
#ifndef LWIP_HDR_TCP_H
#define LWIP_HDR_TCP_H

#include "lwip/opt.h"

#if LWIP_TCP /* don't build if not configured for use in lwipopts.h */

#include "lwip/def.h"
#include "lwip/pbuf.h"
#include "lwip/ip.h"
#include "lwip/icmp.h"
#include "lwip/err.h"
#include "lwip/ip6.h"
#include "lwip/ip6_addr.h"

#if TCP_SND_QUEUELEN > 0xFFFCu
#error "TCP_SND_QUEUELEN is too large"
#endif

LWIP_EXTERN_C_BEGIN

struct tcp_pcb;

/** Function prototype for tcp accept callback functions. Called when a new
 * connection can be accepted on a listening pcb.
 * 
 * You are implicitly given a reference to the  new connection,
 * and are responsible to release it with tcp_close (or tcp_abort).
 * If you're not ready to accept the connection, close it from
 * within the callback.
 *
 * @param arg Additional argument to pass to the callback function (@see tcp_arg())
 * @param newpcb The new connection pcb
 */
typedef void (*tcp_accept_fn)(void *arg, struct tcp_pcb *newpcb, err_t err);

/** Function prototype for tcp receive callback functions. Called when data has
 * been received.
 *
 * @param arg Additional argument to pass to the callback function (@see tcp_arg())
 * @param tpcb The connection pcb which received data
 * @param p The received data (or NULL when the connection has been closed!)
 */
typedef void (*tcp_recv_fn)(void *arg, struct tcp_pcb *tpcb, struct pbuf *p);

/** Function prototype for tcp sent callback functions. Called when sent data has
 * been acknowledged by the remote side. Use it to free corresponding resources.
 * This also means that the pcb has now space available to send new data.
 *
 * @param arg Additional argument to pass to the callback function (@see tcp_arg())
 * @param tpcb The connection pcb for which data has been acknowledged
 * @param len The amount of bytes acknowledged
 */
typedef void (*tcp_sent_fn)(void *arg, struct tcp_pcb *tpcb, u16_t len);

/** Function prototype for tcp error callback functions. Called when the pcb
 * receives a RST or is unexpectedly closed for any other reason.
 *
 * @note The corresponding pcb is already freed when this callback is called!
 *
 * @param arg Additional argument to pass to the callback function (@see tcp_arg())
 * @param err Error code to indicate why the pcb has been closed
 *            ERR_ABRT: aborted, e.g. because the memory is being reused
 *                      for another connection
 *            ERR_RST: the connection was reset by the remote host
 */
typedef void  (*tcp_err_fn)(void *arg, err_t err);

/** Function prototype for tcp connected callback functions. Called when a pcb
 * is connected to the remote side after initiating a connection attempt by
 * calling tcp_connect().
 *
 * @param arg Additional argument to pass to the callback function (@see tcp_arg())
 * @param tpcb The connection pcb which is connected
 * @param err An unused error code, always ERR_OK currently ;-) @todo!
 *
 * @note When a connection attempt fails, the error callback is currently called!
 */
typedef void (*tcp_connected_fn)(void *arg, struct tcp_pcb *tpcb, err_t err);

#define RCV_WND_SCALE(pcb, wnd) (wnd)
#define SND_WND_SCALE(pcb, wnd) (wnd)
#define TCPWND16(x)             (x)
#define TCP_WND_MAX(pcb)        TCP_WND
typedef u16_t tcpwnd_size_t;
typedef u16_t tcpflags_t;

enum tcp_state {
  CLOSED      = 0,
  LISTEN      = 1,
  SYN_SENT    = 2,
  SYN_RCVD    = 3,
  ESTABLISHED = 4,
  FIN_WAIT_1  = 5,
  FIN_WAIT_2  = 6,
  CLOSE_WAIT  = 7,
  CLOSING     = 8,
  LAST_ACK    = 9,
  TIME_WAIT   = 10,
  LISTEN_CLOS = 11
};

/**
 * members common to struct tcp_pcb and struct tcp_listen_pcb
 */
#define TCP_PCB_COMMON(type) \
  IP_PCB; \
  type *next; /* for the linked list */ \
  void *callback_arg; \
  enum tcp_state state; /* TCP state */ \
  u8_t prio; \
  /* ports are in host byte order */ \
  u16_t local_port

/* Pointers to this structure are used to refer to an unknown type
 * of pcb (tcp_pcb or tcp_pcb_listen), there structures don't actually
 * exist themselves.
 */
struct tcp_pcb_base {
  TCP_PCB_COMMON(struct tcp_pcb_base);
};

#define to_tcp_pcb_base(pcb) ((struct tcp_pcb_base *)(pcb))

/** the TCP protocol control block for listening pcbs */
struct tcp_pcb_listen {
  TCP_PCB_COMMON(struct tcp_pcb_listen);

  /* Function to call when a listener has been connected. */
  tcp_accept_fn accept;
  
  u8_t backlog;
  u8_t accepts_pending;
  
#if LWIP_IPV4 && LWIP_IPV6
  u8_t accept_any_ip_version;
#endif /* LWIP_IPV4 && LWIP_IPV6 */
  tcpwnd_size_t initial_rcv_wnd;
};

/* the TCP protocol control block */
struct tcp_pcb {
  TCP_PCB_COMMON(struct tcp_pcb);

  /* ports are in host byte order */
  u16_t remote_port;

  tcpflags_t flags;
#define TF_ACK_DELAY   0x01U   /* Delayed ACK. */
#define TF_ACK_NOW     0x02U   /* Immediate ACK. */
#define TF_INFR        0x04U   /* In fast recovery. */
#define TF_TIMESTAMP   0x08U   /* Timestamp option enabled */
#define TF_NOUSER      0x10U   /* There is no user reference to this PCB */
#define TF_FIN         0x20U   /* Connection was closed locally (FIN segment enqueued). */
#define TF_NAGLEMEMERR 0x40U   /* memerr, try to output to prevent delayed ACK to happen */
#define TF_BACKLOGPEND 0x80U   /* this PCB has an accepts_pending reference in the listener */
#define TF_FASTREXMIT  0x100U  /* fast retransmission of the first segment is desired */

  /* the rest of the fields are in host byte order
     as we have to do some math with them */

  /* Timers */
  u8_t last_timer;
  u32_t tmr;

  /* receiver variables */
  u32_t rcv_nxt;   /* next seqno expected */
  tcpwnd_size_t rcv_wnd;   /* receiver window available */
  tcpwnd_size_t rcv_ann_wnd; /* receiver window to announce */
  u32_t rcv_ann_right_edge; /* announced right edge of window */

  /* Retransmission timer. */
  s16_t rtime;

  u16_t mss;   /* maximum segment size */

  /* RTT (round trip time) estimation variables */
  u32_t rttest; /* RTT estimate in 500ms ticks */
  u32_t rtseq;  /* sequence number being timed */
  s16_t sa, sv; /* @todo document this */

  s16_t rto;    /* retransmission time-out */
  u8_t nrtx;    /* number of retransmissions */

  /* fast retransmit/recovery */
  u8_t dupacks;
  u32_t lastack; /* Highest acknowledged seqno. */

  /* congestion avoidance/control variables */
  tcpwnd_size_t cwnd;
  tcpwnd_size_t ssthresh;

  /* sender variables */
  u32_t snd_nxt;   /* next new seqno to be sent */
  u32_t snd_wl1, snd_wl2; /* Sequence and acknowledgement numbers of last
                             window update. */
  u32_t snd_lbb;       /* Sequence number of next byte to be buffered. */
  tcpwnd_size_t snd_wnd;   /* sender window */
  tcpwnd_size_t snd_wnd_max; /* the maximum sender window announced by the remote host */

  tcpwnd_size_t snd_buf;   /* Available buffer space for sending (in bytes). */
  u16_t snd_queuelen; /* Available buffer space for sending (in pbufs). */

  /* These are ordered by sequence number: */
  struct tcp_seg *sndq;       /* first segment in the queue */
  struct tcp_seg *sndq_last;  /* last segment in the queue (NULL if sndq==NULL) */
  struct tcp_seg *sndq_next;  /* next segment to be sent by tcp_output() */

  /* The associated listen PCB, if any. */
  struct tcp_pcb_listen *listener;
  
  /* Function to be called when more send buffer space is available. */
  tcp_sent_fn sent;
  /* Function to be called when (in-sequence) data has arrived. */
  tcp_recv_fn recv;
  /* Function to be called when a connection has been set up. */
  tcp_connected_fn connected;
  /* Function to be called whenever a fatal error occurs. */
  tcp_err_fn errf;

#if LWIP_TCP_TIMESTAMPS
  u32_t ts_lastacksent;
  u32_t ts_recent;
#endif /* LWIP_TCP_TIMESTAMPS */

  /* idle time before KEEPALIVE is sent */
  u32_t keep_idle;
#if LWIP_TCP_KEEPALIVE
  u32_t keep_intvl;
  u32_t keep_cnt;
#endif /* LWIP_TCP_KEEPALIVE */

  /* Persist timer counter */
  u8_t persist_cnt;
  /* Persist timer back-off */
  u8_t persist_backoff;

  /* KEEPALIVE counter */
  u8_t keep_cnt_sent;
};

/* Application program's interface: */

struct tcp_pcb * tcp_new     (void);
struct tcp_pcb_listen * tcp_new_listen (void);

void             tcp_arg     (struct tcp_pcb_base *pcb, void *arg);
void             tcp_accept  (struct tcp_pcb_listen *pcb, tcp_accept_fn accept);
void             tcp_recv    (struct tcp_pcb *pcb, tcp_recv_fn recv);
void             tcp_sent    (struct tcp_pcb *pcb, tcp_sent_fn sent);
void             tcp_err     (struct tcp_pcb *pcb, tcp_err_fn err);

#define          tcp_mss(pcb)             (((pcb)->flags & TF_TIMESTAMP) ? ((pcb)->mss - 12)  : (pcb)->mss)
#define          tcp_sndbuf(pcb)          (TCPWND16((pcb)->snd_buf))
#define          tcp_sndqueuelen(pcb)     ((pcb)->snd_queuelen)

void             tcp_recved  (struct tcp_pcb *pcb, u16_t len);
err_t            tcp_bind    (struct tcp_pcb_base *pcb, const ip_addr_t *ipaddr, u16_t port);
err_t            tcp_connect (struct tcp_pcb *pcb, const ip_addr_t *ipaddr,
                              u16_t port, tcp_connected_fn connected);

err_t            tcp_listen_with_backlog(struct tcp_pcb_listen *lpcb, u8_t backlog);
#define          tcp_listen(pcb) tcp_listen_with_backlog(pcb, TCP_DEFAULT_LISTEN_BACKLOG)

void             tcp_backlog_set(struct tcp_pcb_listen *lpcb, u8_t new_backlog);
void             tcp_backlog_delayed(struct tcp_pcb* pcb);
void             tcp_backlog_accepted(struct tcp_pcb* pcb);

void             tcp_abort (struct tcp_pcb *pcb);
void             tcp_close (struct tcp_pcb *pcb);
err_t            tcp_shut_tx (struct tcp_pcb *pcb);
void             tcp_close_listen(struct tcp_pcb_listen *lpcb);

/* Flags for "apiflags" parameter in tcp_write */
#define TCP_WRITE_FLAG_MORE 0x02
#define TCP_WRITE_FLAG_PARTIAL 0x04

err_t            tcp_write(struct tcp_pcb *pcb, const void *dataptr, u16_t len,
                           u8_t apiflags, u16_t *written_len);

void             tcp_setprio (struct tcp_pcb_base *pcb, u8_t prio);

#define TCP_PRIO_MIN    1
#define TCP_PRIO_NORMAL 64
#define TCP_PRIO_MAX    127

err_t            tcp_output  (struct tcp_pcb *pcb);


const char* tcp_debug_state_str(enum tcp_state s);

#if LWIP_IPV6
struct tcp_pcb * tcp_new_ip6 (void);
struct tcp_pcb_listen * tcp_new_listen_ip6 (void);
#endif /* LWIP_IPV6 */

#if LWIP_IPV4 && LWIP_IPV6
err_t            tcp_listen_dual_with_backlog(struct tcp_pcb_listen *lpcb, u8_t backlog);
#define          tcp_listen_dual(lpcb) tcp_listen_dual_with_backlog(lpcb, TCP_DEFAULT_LISTEN_BACKLOG)
#else /* LWIP_IPV4 && LWIP_IPV6 */
#define          tcp_listen_dual_with_backlog(lpcb, backlog) tcp_listen_with_backlog(lpcb, backlog)
#define          tcp_listen_dual(lpcb) tcp_listen(lpcb)
#endif /* LWIP_IPV4 && LWIP_IPV6 */

LWIP_EXTERN_C_END

#endif /* LWIP_TCP */

#endif /* LWIP_HDR_TCP_H */
