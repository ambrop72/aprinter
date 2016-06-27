/**
 * @file
 * Transmission Control Protocol, outgoing traffic
 *
 * The output functions of TCP.
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

#if LWIP_TCP /* don't build if not configured for use in lwipopts.h */

#include "lwip/tcp_impl.h"
#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/memp.h"
#include "lwip/ip_addr.h"
#include "lwip/netif.h"
#include "lwip/inet_chksum.h"
#include "lwip/stats.h"
#include "lwip/ip6.h"
#include "lwip/ip6_addr.h"
#include "lwip/inet_chksum.h"
#if LWIP_TCP_TIMESTAMPS
#include "lwip/sys.h"
#endif

#include <string.h>

/* Forward declarations.*/
static err_t tcp_output_segment(struct tcp_seg *seg, struct tcp_pcb *pcb, struct netif *netif);

/** Allocate a pbuf and create a tcphdr at p->payload, used for output
 * functions other than the default tcp_output -> tcp_output_segment
 * (e.g. tcp_send_empty_ack, etc.)
 *
 * @param pcb tcp pcb for which to send a packet (used to initialize tcp_hdr)
 * @param optlen length of header-options
 * @param datalen length of tcp data to reserve in pbuf
 * @param seqno_be seqno in network byte order (big-endian)
 * @return pbuf with p->payload being the tcp_hdr
 */
static struct pbuf *
tcp_output_alloc_header(struct tcp_pcb *pcb, u16_t optlen, u16_t datalen,
                      u32_t seqno_be /* already in network byte order */)
{
  struct tcp_hdr *tcphdr;
  struct pbuf *p = pbuf_alloc_pool(PBUF_IP, TCP_HLEN + optlen + datalen, TCP_HLEN + optlen);
  if (p != NULL) {
    LWIP_ASSERT("check that first pbuf can hold struct tcp_hdr",
                 (p->len >= TCP_HLEN + optlen));
    tcphdr = (struct tcp_hdr *)p->payload;
    tcphdr->src = htons(pcb->local_port);
    tcphdr->dest = htons(pcb->remote_port);
    tcphdr->seqno = seqno_be;
    tcphdr->ackno = htonl(pcb->rcv_nxt);
    TCPH_HDRLEN_FLAGS_SET(tcphdr, (5 + optlen / 4), TCP_ACK);
    tcphdr->wnd = htons(TCPWND_MIN16(RCV_WND_SCALE(pcb, pcb->rcv_ann_wnd)));
    tcphdr->chksum = 0;
    tcphdr->urgp = 0;

    /* If we're sending a packet, update the announced right window edge */
    pcb->rcv_ann_right_edge = pcb->rcv_nxt + pcb->rcv_ann_wnd;
  }
  return p;
}

/**
 * Called by tcp_close() to send a segment including FIN flag but not data.
 *
 * @param pcb the tcp_pcb over which to send a segment
 * @return ERR_OK if sent, another err_t otherwise
 */
err_t
tcp_send_fin(struct tcp_pcb *pcb)
{
  /* first, try to add the fin to the last unsent segment */
  if (pcb->unsent != NULL) {
    struct tcp_seg *last_unsent;
    for (last_unsent = pcb->unsent; last_unsent->next != NULL;
         last_unsent = last_unsent->next);

    if ((TCPH_FLAGS(last_unsent->tcphdr) & (TCP_SYN | TCP_FIN | TCP_RST)) == 0) {
      /* no SYN/FIN/RST flag in the header, we can add the FIN flag */
      TCPH_SET_FLAG(last_unsent->tcphdr, TCP_FIN);
      pcb->flags |= TF_FIN;
      return ERR_OK;
    }
  }
  /* no data, no length, flags, copy=1, no optdata */
  return tcp_enqueue_flags(pcb, TCP_FIN);
}

/**
 * Create a TCP segment with prefilled header.
 *
 * Called by tcp_write and tcp_enqueue_flags.
 *
 * @param pcb Protocol control block for the TCP connection.
 * @param p pbuf that is used to hold the TCP header.
 * @param flags TCP flags for header.
 * @param seqno TCP sequence number of this packet
 * @param optflags options to include in TCP header
 * @return a new tcp_seg pointing to p, or NULL.
 * The TCP header is filled in except ackno and wnd.
 * p is freed on failure.
 */
static struct tcp_seg *
tcp_create_segment(struct tcp_pcb *pcb, struct pbuf *p, u8_t flags, u32_t seqno, u8_t optflags)
{
  struct tcp_seg *seg;
  u8_t optlen = LWIP_TCP_OPT_LENGTH(optflags);

  if ((seg = (struct tcp_seg *)memp_malloc(MEMP_TCP_SEG)) == NULL) {
    LWIP_DEBUGF(TCP_OUTPUT_DEBUG | LWIP_DBG_LEVEL_SERIOUS, ("tcp_create_segment: no memory.\n"));
    pbuf_free(p);
    return NULL;
  }
  seg->flags = optflags;
  seg->next = NULL;
  seg->p = p;
  LWIP_ASSERT("p->tot_len >= optlen", p->tot_len >= optlen);
  seg->len = p->tot_len - optlen;

  /* build TCP header */
  if (pbuf_header(p, TCP_HLEN)) {
    LWIP_DEBUGF(TCP_OUTPUT_DEBUG | LWIP_DBG_LEVEL_SERIOUS, ("tcp_create_segment: no room for TCP header in pbuf.\n"));
    TCP_STATS_INC(tcp.err);
    tcp_seg_free(seg);
    return NULL;
  }
  seg->tcphdr = (struct tcp_hdr *)seg->p->payload;
  seg->tcphdr->src = htons(pcb->local_port);
  seg->tcphdr->dest = htons(pcb->remote_port);
  seg->tcphdr->seqno = htonl(seqno);
  /* ackno is set in tcp_output */
  TCPH_HDRLEN_FLAGS_SET(seg->tcphdr, (5 + optlen / 4), flags);
  /* wnd and chksum are set in tcp_output */
  seg->tcphdr->urgp = 0;
  return seg;
}

/** Checks if tcp_write is allowed or not (checks state, snd_buf and snd_queuelen).
 *
 * @param pcb the tcp pcb to check for
 * @param len length of data to send (checked agains snd_buf)
 * @param apiflags flags passed to tcp_write (we consider TCP_WRITE_FLAG_PARTIAL)
 * @return ERR_OK if tcp_write is allowed to proceed, another err_t otherwise
 */
static err_t
tcp_write_checks(struct tcp_pcb *pcb, u16_t len, u8_t apiflags)
{
  /* connection is in invalid state for data transmission? */
  if ((pcb->state != ESTABLISHED) &&
      (pcb->state != CLOSE_WAIT) &&
      (pcb->state != SYN_SENT) &&
      (pcb->state != SYN_RCVD)) {
    LWIP_DEBUGF(TCP_OUTPUT_DEBUG | LWIP_DBG_STATE | LWIP_DBG_LEVEL_SEVERE, ("tcp_write() called in invalid state\n"));
    return ERR_CONN;
  } else if (len == 0) {
    return ERR_OK;
  }

  /* fail on too much data */
  if (len > pcb->snd_buf) {
    LWIP_DEBUGF(TCP_OUTPUT_DEBUG | LWIP_DBG_LEVEL_SEVERE, ("tcp_write: too much data (len=%"U16_F" > snd_buf=%"TCPWNDSIZE_F")\n",
      len, pcb->snd_buf));
    pcb->flags |= TF_NAGLEMEMERR;
    return ERR_MEM;
  }

  LWIP_DEBUGF(TCP_QLEN_DEBUG, ("tcp_write: queuelen: %"TCPWNDSIZE_F"\n", (tcpwnd_size_t)pcb->snd_queuelen));

  /* If total number of pbufs on the unsent/unacked queues exceeds the
   * configured maximum, return an error */
  /* check for configured max queuelen and possible overflow */
  if (!(apiflags & TCP_WRITE_FLAG_PARTIAL) &&
      ((pcb->snd_queuelen >= TCP_SND_QUEUELEN) || (pcb->snd_queuelen > TCP_SNDQUEUELEN_OVERFLOW))) {
    LWIP_DEBUGF(TCP_OUTPUT_DEBUG | LWIP_DBG_LEVEL_SEVERE, ("tcp_write: too long queue %"U16_F" (max %"U16_F")\n",
      pcb->snd_queuelen, (u16_t)TCP_SND_QUEUELEN));
    TCP_STATS_INC(tcp.memerr);
    pcb->flags |= TF_NAGLEMEMERR;
    return ERR_MEM;
  }
  if (pcb->snd_queuelen != 0) {
    LWIP_ASSERT("tcp_write: pbufs on queue => at least one queue non-empty",
      pcb->unacked != NULL || pcb->unsent != NULL);
  } else {
    LWIP_ASSERT("tcp_write: no pbufs on queue => both queues empty",
      pcb->unacked == NULL && pcb->unsent == NULL);
  }
  return ERR_OK;
}

/**
 * Same as tcp_write_ext with written_len==NULL.
 */
err_t
tcp_write(struct tcp_pcb *pcb, const void *arg, u16_t len, u8_t apiflags)
{
  return tcp_write_ext(pcb, arg, len, apiflags, NULL);
}

/**
 * Write data for sending (but does not send it immediately).
 *
 * It waits in the expectation of more data being sent soon (as
 * it can send them more efficiently by combining them together).
 * To prompt the system to send data now, call tcp_output() after
 * calling tcp_write().
 *
 * @param pcb Protocol control block for the TCP connection to enqueue data for.
 * @param arg Pointer to the data to be enqueued for sending.
 * @param len Data length in bytes
 * @param apiflags combination of following flags :
 * - TCP_WRITE_FLAG_MORE (0x02) for TCP connection, PSH flag will not be set on last segment sent,
 * - TCP_WRITE_FLAG_PARTIAL (0x04) allow writing less data than requested in case there are no
 *                                 segments available
 * @param written_len when not NULL and we return ERR_OK, this is set to the number
 *                    of bytes written. It is only relevant when TCP_WRITE_FLAG_PARTIAL
 *                    is used, in which case this must not be NULL.
 * @return ERR_OK if enqueued, another err_t on error
 */
err_t
tcp_write_ext(struct tcp_pcb *pcb, const void *arg, u16_t len, u8_t apiflags, u16_t *written_len)
{
  struct pbuf *concat_p = NULL;
  u16_t extendlen = 0;
  struct tcp_seg *last_unsent = NULL, *seg = NULL, *prev_seg = NULL, *queue = NULL;
  u16_t pos = 0; /* position in 'arg' data */
  u16_t queuelen;
  u8_t optlen = 0;
  u8_t optflags = 0;
  err_t err;
  /* don't allocate segments bigger than half the maximum window we ever received */
  u16_t mss_local = LWIP_MIN(pcb->mss, TCPWND_MIN16(pcb->snd_wnd_max/2));
  mss_local = mss_local ? mss_local : pcb->mss;

  LWIP_DEBUGF(TCP_OUTPUT_DEBUG, ("tcp_write(pcb=%p, data=%p, len=%"U16_F", apiflags=%"U16_F")\n",
    (void *)pcb, arg, len, (u16_t)apiflags));
  LWIP_ERROR("tcp_write: arg == NULL (programmer violates API)",
             arg != NULL, return ERR_ARG;);
  LWIP_ERROR("tcp_write: written_len == NULL with TCP_WRITE_FLAG_PARTIAL",
             !(apiflags & TCP_WRITE_FLAG_PARTIAL) || written_len != NULL, return ERR_ARG;);

  err = tcp_write_checks(pcb, len, apiflags);
  if (err != ERR_OK) {
    return err;
  }
  queuelen = pcb->snd_queuelen;

#if LWIP_TCP_TIMESTAMPS
  if ((pcb->flags & TF_TIMESTAMP)) {
    /* Make sure the timestamp option is only included in data segments if we
       agreed about it with the remote host. */
    optflags = TF_SEG_OPTS_TS;
    optlen = LWIP_TCP_OPT_LENGTH(TF_SEG_OPTS_TS);
    /* ensure that segments can hold at least one data byte... */
    mss_local = LWIP_MAX(mss_local, LWIP_TCP_OPT_LEN_TS + 1);
  }
#endif /* LWIP_TCP_TIMESTAMPS */


  /*
   * TCP segmentation is done in three phases with increasing complexity:
   *
   * 2. Chain a new pbuf to the end of pcb->unsent.
   * 3. Create new segments.
   *
   * We may run out of memory at any point. In that case we must
   * return ERR_MEM and not change anything in pcb. Therefore, all
   * changes are recorded in local variables and committed at the end
   * of the function. Some pcb fields are maintained in local copies:
   *
   * queuelen = pcb->snd_queuelen
   *
   * These variables are set consistently by the phases:
   *
   * seg points to the last segment tampered with.
   *
   * pos records progress as data is segmented.
   */

  /* Find the tail of the unsent queue. */
  if (pcb->unsent != NULL) {
    u16_t space;
    u16_t unsent_optlen;

    /* @todo: this could be sped up by keeping last_unsent in the pcb */
    for (last_unsent = pcb->unsent; last_unsent->next != NULL;
         last_unsent = last_unsent->next);

    /* Usable space at the end of the last unsent segment */
    unsent_optlen = LWIP_TCP_OPT_LENGTH(last_unsent->flags);
    LWIP_ASSERT("mss_local is too small", mss_local >= last_unsent->len + unsent_optlen);
    space = mss_local - (last_unsent->len + unsent_optlen);

    /*
     * Phase 2: Chain a new pbuf to the end of pcb->unsent.
     *
     * We don't extend segments containing SYN/FIN flags or options
     * (len==0). The new pbuf is kept in concat_p and pbuf_cat'ed at
     * the end.
     */
    if ((pos < len) && (space > 0) && (last_unsent->len > 0)) {
      u16_t seglen = space < len - pos ? space : len - pos;
      seg = last_unsent;

      /* Create a pbuf with a copy or reference to seglen bytes. We
       * can use PBUF_RAW here since the data appears in the middle of
       * a segment. A header will never be prepended. */
      /* If possible extend an existing PBUF_ROM pbuf. */
      struct pbuf *last_p = pbuf_last(last_unsent->p);
      if (pos == 0 && last_p->type == PBUF_ROM && (const u8_t*)last_p->payload + last_p->len == (const u8_t*)arg) {
        extendlen = seglen;
      } else {
        if ((concat_p = pbuf_alloc(PBUF_RAW, seglen, PBUF_ROM)) == NULL) {
          LWIP_DEBUGF(TCP_OUTPUT_DEBUG | LWIP_DBG_LEVEL_SERIOUS,
                      ("tcp_write: could not allocate memory for zero-copy pbuf\n"));
          goto memerr;
        }
        /* reference the non-volatile payload data */
        concat_p->payload = (void *)((const u8_t*)arg + pos);
        queuelen += pbuf_clen(concat_p);
      }

      pos += seglen;
    }
  }

  /*
   * Phase 3: Create new segments.
   *
   * The new segments are chained together in the local 'queue'
   * variable, ready to be appended to pcb->unsent.
   */
  while (pos < len) {
    struct pbuf *p;
    u16_t left = len - pos;
    u16_t max_len = mss_local - optlen;
    u16_t seglen = left > max_len ? max_len : left;

    /* If partial writes are allowed and we won't be able to allocate another segment,
     * stop gracefully. */
    if (apiflags & TCP_WRITE_FLAG_PARTIAL) {
      u16_t new_queuelen = queuelen + 2;
      if ((new_queuelen > TCP_SND_QUEUELEN) || (new_queuelen > TCP_SNDQUEUELEN_OVERFLOW)) {
        break;
      }
    }
    
    /* Copy is not set: First allocate a pbuf for holding the data.
      * Since the referenced data is available at least until it is
      * sent out on the link (as it has to be ACKed by the remote
      * party) we can safely use PBUF_ROM instead of PBUF_REF here.
      */
    struct pbuf *p2;
    if ((p2 = pbuf_alloc(PBUF_TRANSPORT, seglen, PBUF_ROM)) == NULL) {
      LWIP_DEBUGF(TCP_OUTPUT_DEBUG | LWIP_DBG_LEVEL_SERIOUS, ("tcp_write: could not allocate memory for zero-copy pbuf\n"));
      goto memerr;
    }
    /* reference the non-volatile payload data */
    p2->payload = (void *)((const u8_t*)arg + pos);

    /* Second, allocate a pbuf for the headers. */
    if ((p = pbuf_alloc(PBUF_TRANSPORT, optlen, PBUF_TCP)) == NULL) {
      /* If allocation fails, we have to deallocate the data pbuf as
        * well. */
      pbuf_free(p2);
      LWIP_DEBUGF(TCP_OUTPUT_DEBUG | LWIP_DBG_LEVEL_SERIOUS, ("tcp_write: could not allocate memory for header pbuf\n"));
      goto memerr;
    }
    /* Concatenate the headers and data pbufs together. */
    pbuf_cat(p/*header*/, p2/*data*/);

    queuelen += pbuf_clen(p);

    /* Now that there are more segments queued, we check again if the
     * length of the queue exceeds the configured maximum or
     * overflows. */
    if ((queuelen > TCP_SND_QUEUELEN) || (queuelen > TCP_SNDQUEUELEN_OVERFLOW)) {
      LWIP_DEBUGF(TCP_OUTPUT_DEBUG | LWIP_DBG_LEVEL_SERIOUS, ("tcp_write: queue too long %"U16_F" (%d)\n",
        queuelen, (int)TCP_SND_QUEUELEN));
      pbuf_free(p);
      goto memerr;
    }

    if ((seg = tcp_create_segment(pcb, p, 0, pcb->snd_lbb + pos, optflags)) == NULL) {
      goto memerr;
    }

    /* first segment of to-be-queued data? */
    if (queue == NULL) {
      queue = seg;
    } else {
      /* Attach the segment to the end of the queued segments */
      LWIP_ASSERT("prev_seg != NULL", prev_seg != NULL);
      prev_seg->next = seg;
    }
    /* remember last segment of to-be-queued data for next iteration */
    prev_seg = seg;

    LWIP_DEBUGF(TCP_OUTPUT_DEBUG | LWIP_DBG_TRACE, ("tcp_write: queueing %"U32_F":%"U32_F"\n",
      ntohl(seg->tcphdr->seqno),
      ntohl(seg->tcphdr->seqno) + TCP_TCPLEN(seg)));

    pos += seglen;
  }

  /*
   * All three segmentation phases were successful. We can commit the
   * transaction.
   */

  /*
   * Phase 2: concat_p can be concatenated onto last_unsent->p
   */
  if (concat_p != NULL) {
    LWIP_ASSERT("tcp_write: cannot concatenate when pcb->unsent is empty",
      (last_unsent != NULL));
    pbuf_cat(last_unsent->p, concat_p);
    last_unsent->len += concat_p->tot_len;
  }
  else if (extendlen > 0) {
    struct pbuf *p;
    for (p = last_unsent->p; p->next != NULL; p = p->next) {
      p->tot_len += extendlen;
    }
    p->tot_len += extendlen;
    p->len += extendlen;
    last_unsent->len += extendlen;
  }

  /*
   * Phase 3: Append queue to pcb->unsent. Queue may be NULL, but that
   * is harmless
   */
  if (last_unsent == NULL) {
    pcb->unsent = queue;
  } else {
    last_unsent->next = queue;
  }

  /*
   * Finally update the pcb state.
   */
  pcb->snd_lbb += pos;
  pcb->snd_buf -= pos;
  pcb->snd_queuelen = queuelen;

  LWIP_DEBUGF(TCP_QLEN_DEBUG, ("tcp_write: %"S16_F" (after enqueued)\n",
    pcb->snd_queuelen));
  if (pcb->snd_queuelen != 0) {
    LWIP_ASSERT("tcp_write: valid queue length",
                pcb->unacked != NULL || pcb->unsent != NULL);
  }

  /* Set the PSH flag in the last segment that we enqueued. */
  if (seg != NULL && seg->tcphdr != NULL && ((apiflags & TCP_WRITE_FLAG_MORE)==0)) {
    TCPH_SET_FLAG(seg->tcphdr, TCP_PSH);
  }

  if (written_len != NULL) {
    *written_len = pos;
  }
  return ERR_OK;
memerr:
  pcb->flags |= TF_NAGLEMEMERR;
  TCP_STATS_INC(tcp.memerr);

  if (concat_p != NULL) {
    pbuf_free(concat_p);
  }
  if (queue != NULL) {
    tcp_segs_free(queue);
  }
  if (pcb->snd_queuelen != 0) {
    LWIP_ASSERT("tcp_write: valid queue length", pcb->unacked != NULL ||
      pcb->unsent != NULL);
  }
  LWIP_DEBUGF(TCP_QLEN_DEBUG | LWIP_DBG_STATE, ("tcp_write: %"S16_F" (with mem err)\n", pcb->snd_queuelen));
  return ERR_MEM;
}

/**
 * Enqueue TCP options for transmission.
 *
 * Called by tcp_connect(), tcp_listen_input(), and tcp_send_ctrl().
 *
 * @param pcb Protocol control block for the TCP connection.
 * @param flags TCP header flags to set in the outgoing segment.
 */
err_t
tcp_enqueue_flags(struct tcp_pcb *pcb, u8_t flags)
{
  struct pbuf *p;
  struct tcp_seg *seg;
  u8_t optflags = 0;
  u8_t optlen = 0;

  LWIP_DEBUGF(TCP_QLEN_DEBUG, ("tcp_enqueue_flags: queuelen: %"U16_F"\n", (u16_t)pcb->snd_queuelen));

  LWIP_ASSERT("tcp_enqueue_flags: need either TCP_SYN or TCP_FIN in flags (programmer violates API)",
              (flags & (TCP_SYN | TCP_FIN)) != 0);

  /* check for configured max queuelen and possible overflow (FIN flag should always come through!) */
  if (((pcb->snd_queuelen >= TCP_SND_QUEUELEN) || (pcb->snd_queuelen > TCP_SNDQUEUELEN_OVERFLOW)) &&
      ((flags & TCP_FIN) == 0)) {
    LWIP_DEBUGF(TCP_OUTPUT_DEBUG | LWIP_DBG_LEVEL_SEVERE, ("tcp_enqueue_flags: too long queue %"U16_F" (max %"U16_F")\n",
                                       pcb->snd_queuelen, (u16_t)TCP_SND_QUEUELEN));
    TCP_STATS_INC(tcp.memerr);
    pcb->flags |= TF_NAGLEMEMERR;
    return ERR_MEM;
  }

  if (flags & TCP_SYN) {
    optflags = TF_SEG_OPTS_MSS;
  }
#if LWIP_TCP_TIMESTAMPS
  if ((pcb->flags & TF_TIMESTAMP)) {
    /* Make sure the timestamp option is only included in data segments if we
       agreed about it with the remote host. */
    optflags |= TF_SEG_OPTS_TS;
  }
#endif /* LWIP_TCP_TIMESTAMPS */
  optlen = LWIP_TCP_OPT_LENGTH(optflags);

  /* tcp_enqueue_flags is always called with either SYN or FIN in flags.
   * We need one available snd_buf byte to do that.
   * This means we can't send FIN while snd_buf==0. A better fix would be to
   * not include SYN and FIN sequence numbers in the snd_buf count. */
  if (pcb->snd_buf == 0) {
    LWIP_DEBUGF(TCP_OUTPUT_DEBUG | LWIP_DBG_LEVEL_SEVERE, ("tcp_enqueue_flags: no send buffer available\n"));
    TCP_STATS_INC(tcp.memerr);
    return ERR_MEM;
  }

  /* Allocate pbuf with room for TCP header + options */
  if ((p = pbuf_alloc(PBUF_TRANSPORT, optlen, PBUF_TCP)) == NULL) {
    pcb->flags |= TF_NAGLEMEMERR;
    TCP_STATS_INC(tcp.memerr);
    return ERR_MEM;
  }
  LWIP_ASSERT("tcp_enqueue_flags: check that first pbuf can hold optlen",
              (p->len >= optlen));

  /* Allocate memory for tcp_seg, and fill in fields. */
  if ((seg = tcp_create_segment(pcb, p, flags, pcb->snd_lbb, optflags)) == NULL) {
    pcb->flags |= TF_NAGLEMEMERR;
    TCP_STATS_INC(tcp.memerr);
    return ERR_MEM;
  }
  LWIP_ASSERT("seg->tcphdr not aligned", ((mem_ptr_t)seg->tcphdr % LWIP_MIN(MEM_ALIGNMENT, 4)) == 0);
  LWIP_ASSERT("tcp_enqueue_flags: invalid segment length", seg->len == 0);

  LWIP_DEBUGF(TCP_OUTPUT_DEBUG | LWIP_DBG_TRACE,
              ("tcp_enqueue_flags: queueing %"U32_F":%"U32_F" (0x%"X16_F")\n",
               ntohl(seg->tcphdr->seqno),
               ntohl(seg->tcphdr->seqno) + TCP_TCPLEN(seg),
               (u16_t)flags));

  /* Now append seg to pcb->unsent queue */
  if (pcb->unsent == NULL) {
    pcb->unsent = seg;
  } else {
    struct tcp_seg *useg;
    for (useg = pcb->unsent; useg->next != NULL; useg = useg->next);
    useg->next = seg;
  }

  /* SYN and FIN bump the sequence number */
  if ((flags & TCP_SYN) || (flags & TCP_FIN)) {
    pcb->snd_lbb++;
    /* optlen does not influence snd_buf */
    pcb->snd_buf--;
  }
  if (flags & TCP_FIN) {
    pcb->flags |= TF_FIN;
  }

  /* update number of segments on the queues */
  pcb->snd_queuelen += pbuf_clen(seg->p);
  LWIP_DEBUGF(TCP_QLEN_DEBUG, ("tcp_enqueue_flags: %"S16_F" (after enqueued)\n", pcb->snd_queuelen));
  if (pcb->snd_queuelen != 0) {
    LWIP_ASSERT("tcp_enqueue_flags: invalid queue length",
      pcb->unacked != NULL || pcb->unsent != NULL);
  }

  return ERR_OK;
}

#if LWIP_TCP_TIMESTAMPS
/* Build a timestamp option (12 bytes long) at the specified options pointer)
 *
 * @param pcb tcp_pcb
 * @param opts option pointer where to store the timestamp option
 */
static void
tcp_build_timestamp_option(struct tcp_pcb *pcb, u32_t *opts)
{
  /* Pad with two NOP options to make everything nicely aligned */
  opts[0] = PP_HTONL(0x0101080A);
  opts[1] = htonl(sys_now());
  opts[2] = htonl(pcb->ts_recent);
}
#endif

/** Send an ACK without data.
 *
 * @param pcb Protocol control block for the TCP connection to send the ACK
 */
err_t
tcp_send_empty_ack(struct tcp_pcb *pcb)
{
  err_t err;
  struct pbuf *p;
  u8_t optlen = 0;
  struct netif *netif;
#if LWIP_TCP_TIMESTAMPS || CHECKSUM_GEN_TCP
  struct tcp_hdr *tcphdr;
#endif /* LWIP_TCP_TIMESTAMPS || CHECKSUM_GEN_TCP */

#if LWIP_TCP_TIMESTAMPS
  if (pcb->flags & TF_TIMESTAMP) {
    optlen = LWIP_TCP_OPT_LENGTH(TF_SEG_OPTS_TS);
  }
#endif

  p = tcp_output_alloc_header(pcb, optlen, 0, htonl(pcb->snd_nxt));
  if (p == NULL) {
    /* let tcp_fasttmr retry sending this ACK */
    pcb->flags |= (TF_ACK_DELAY | TF_ACK_NOW);
    LWIP_DEBUGF(TCP_OUTPUT_DEBUG, ("tcp_output: (ACK) could not allocate pbuf\n"));
    return ERR_BUF;
  }
#if LWIP_TCP_TIMESTAMPS || CHECKSUM_GEN_TCP
  tcphdr = (struct tcp_hdr *)p->payload;
#endif /* LWIP_TCP_TIMESTAMPS || CHECKSUM_GEN_TCP */
  LWIP_DEBUGF(TCP_OUTPUT_DEBUG,
              ("tcp_output: sending ACK for %"U32_F"\n", pcb->rcv_nxt));

  /* NB. MSS and window scale options are only sent on SYNs, so ignore them here */
#if LWIP_TCP_TIMESTAMPS
  pcb->ts_lastacksent = pcb->rcv_nxt;

  if (pcb->flags & TF_TIMESTAMP) {
    tcp_build_timestamp_option(pcb, (u32_t *)(tcphdr + 1));
  }
#endif

  netif = ip_route(PCB_ISIPV6(pcb), &pcb->local_ip, &pcb->remote_ip);
  if (netif == NULL) {
    err = ERR_RTE;
  } else {
#if CHECKSUM_GEN_TCP
    IF__NETIF_CHECKSUM_ENABLED(netif, NETIF_CHECKSUM_GEN_TCP) {
      tcphdr->chksum = ip_chksum_pseudo(p, IP_PROTO_TCP, p->tot_len,
        &pcb->local_ip, &pcb->remote_ip);
    }
#endif
    NETIF_SET_HWADDRHINT(netif, &(pcb->addr_hint));
    err = ip_output_if(PCB_ISIPV6(pcb), p, &pcb->local_ip, &pcb->remote_ip,
      pcb->ttl, pcb->tos, IP_PROTO_TCP, netif);
    NETIF_SET_HWADDRHINT(netif, NULL);
  }
  pbuf_free(p);

  if (err != ERR_OK) {
    /* let tcp_fasttmr retry sending this ACK */
    pcb->flags |= (TF_ACK_DELAY | TF_ACK_NOW);
  } else {
    /* remove ACK flags from the PCB, as we sent an empty ACK now */
    pcb->flags &= ~(TF_ACK_DELAY | TF_ACK_NOW);
  }

  return err;
}

/**
 * Find out what we can send and send it
 *
 * @param pcb Protocol control block for the TCP connection to send data
 * @return ERR_OK if data has been sent or nothing to send
 *         another err_t on error
 */
err_t
tcp_output(struct tcp_pcb *pcb)
{
  struct tcp_seg *seg, *useg;
  u32_t wnd, snd_nxt;
  err_t err;
  struct netif *netif;
#if TCP_CWND_DEBUG
  s16_t i = 0;
#endif /* TCP_CWND_DEBUG */

  /* pcb->state LISTEN not allowed here */
  LWIP_ASSERT("don't call tcp_output for listen-pcbs",
    pcb->state != LISTEN);

  /* First, check if we are invoked by the TCP input processing
     code. If so, we do not output anything. Instead, we rely on the
     input processing code to call us when input processing is done
     with. */
  if (tcp_input_pcb == pcb) {
    return ERR_OK;
  }

  wnd = LWIP_MIN(pcb->snd_wnd, pcb->cwnd);

  seg = pcb->unsent;

  /* If the TF_ACK_NOW flag is set and no data will be sent (either
   * because the ->unsent queue is empty or because the window does
   * not allow it), construct an empty ACK segment and send it.
   *
   * If data is to be sent, we will just piggyback the ACK (see below).
   */
  if (pcb->flags & TF_ACK_NOW &&
     (seg == NULL ||
      ntohl(seg->tcphdr->seqno) - pcb->lastack + seg->len > wnd)) {
     return tcp_send_empty_ack(pcb);
  }

  /* useg should point to last segment on unacked queue */
  useg = pcb->unacked;
  if (useg != NULL) {
    for (; useg->next != NULL; useg = useg->next);
  }

  netif = ip_route(PCB_ISIPV6(pcb), &pcb->local_ip, &pcb->remote_ip);
  if (netif == NULL) {
    return ERR_RTE;
  }

  /* If we don't have a local IP address, we get one from netif */
  if (ip_addr_isany(&pcb->local_ip)) {
    const ip_addr_t *local_ip = ip_netif_get_local_ip(PCB_ISIPV6(pcb), netif, &pcb->remote_ip);
    if (local_ip == NULL) {
      return ERR_RTE;
    }
    ip_addr_copy(pcb->local_ip, *local_ip);
  }

#if TCP_OUTPUT_DEBUG
  if (seg == NULL) {
    LWIP_DEBUGF(TCP_OUTPUT_DEBUG, ("tcp_output: nothing to send (%p)\n",
                                   (void*)pcb->unsent));
  }
#endif /* TCP_OUTPUT_DEBUG */
#if TCP_CWND_DEBUG
  if (seg == NULL) {
    LWIP_DEBUGF(TCP_CWND_DEBUG, ("tcp_output: snd_wnd %"TCPWNDSIZE_F
                                 ", cwnd %"TCPWNDSIZE_F", wnd %"U32_F
                                 ", seg == NULL, ack %"U32_F"\n",
                                 pcb->snd_wnd, pcb->cwnd, wnd, pcb->lastack));
  } else {
    LWIP_DEBUGF(TCP_CWND_DEBUG,
                ("tcp_output: snd_wnd %"TCPWNDSIZE_F", cwnd %"TCPWNDSIZE_F", wnd %"U32_F
                 ", effwnd %"U32_F", seq %"U32_F", ack %"U32_F"\n",
                 pcb->snd_wnd, pcb->cwnd, wnd,
                 ntohl(seg->tcphdr->seqno) - pcb->lastack + seg->len,
                 ntohl(seg->tcphdr->seqno), pcb->lastack));
  }
#endif /* TCP_CWND_DEBUG */
  /* data available and window allows it to be sent? */
  while (seg != NULL &&
         ntohl(seg->tcphdr->seqno) - pcb->lastack + seg->len <= wnd) {
    LWIP_ASSERT("RST not expected here!",
                (TCPH_FLAGS(seg->tcphdr) & TCP_RST) == 0);
    /* Stop sending if the nagle algorithm would prevent it
     * Don't stop:
     * - if tcp_write had a memory error before (prevent delayed ACK timeout) or
     * - if FIN was already enqueued for this PCB (SYN is always alone in a segment -
     *   either seg->next != NULL or pcb->unacked == NULL;
     *   RST is no sent using tcp_write/tcp_output.
     */
    if((tcp_do_output_nagle(pcb) == 0) &&
      ((pcb->flags & (TF_NAGLEMEMERR | TF_FIN)) == 0)) {
      break;
    }
#if TCP_CWND_DEBUG
    LWIP_DEBUGF(TCP_CWND_DEBUG, ("tcp_output: snd_wnd %"TCPWNDSIZE_F", cwnd %"TCPWNDSIZE_F", wnd %"U32_F", effwnd %"U32_F", seq %"U32_F", ack %"U32_F", i %"S16_F"\n",
                            pcb->snd_wnd, pcb->cwnd, wnd,
                            ntohl(seg->tcphdr->seqno) + seg->len -
                            pcb->lastack,
                            ntohl(seg->tcphdr->seqno), pcb->lastack, i));
    ++i;
#endif /* TCP_CWND_DEBUG */

    if (pcb->state != SYN_SENT) {
      TCPH_SET_FLAG(seg->tcphdr, TCP_ACK);
    }

    err = tcp_output_segment(seg, pcb, netif);
    if (err != ERR_OK) {
      /* segment could not be sent, for whatever reason */
      pcb->flags |= TF_NAGLEMEMERR;
      return err;
    }
    pcb->unsent = seg->next;
    if (pcb->state != SYN_SENT) {
      pcb->flags &= ~(TF_ACK_DELAY | TF_ACK_NOW);
    }
    snd_nxt = ntohl(seg->tcphdr->seqno) + TCP_TCPLEN(seg);
    if (TCP_SEQ_LT(pcb->snd_nxt, snd_nxt)) {
      pcb->snd_nxt = snd_nxt;
    }
    /* put segment on unacknowledged list if length > 0 */
    if (TCP_TCPLEN(seg) > 0) {
      seg->next = NULL;
      /* unacked list is empty? */
      if (pcb->unacked == NULL) {
        pcb->unacked = seg;
        useg = seg;
      /* unacked list is not empty? */
      } else {
        /* In the case of fast retransmit, the packet should not go to the tail
         * of the unacked queue, but rather somewhere before it. We need to check for
         * this case. -STJ Jul 27, 2004 */
        if (TCP_SEQ_LT(ntohl(seg->tcphdr->seqno), ntohl(useg->tcphdr->seqno))) {
          /* add segment to before tail of unacked list, keeping the list sorted */
          struct tcp_seg **cur_seg = &(pcb->unacked);
          while (*cur_seg &&
            TCP_SEQ_LT(ntohl((*cur_seg)->tcphdr->seqno), ntohl(seg->tcphdr->seqno))) {
              cur_seg = &((*cur_seg)->next );
          }
          seg->next = (*cur_seg);
          (*cur_seg) = seg;
        } else {
          /* add segment to tail of unacked list */
          useg->next = seg;
          useg = useg->next;
        }
      }
    /* do not queue empty segments on the unacked list */
    } else {
      tcp_seg_free(seg);
    }
    seg = pcb->unsent;
  }

  pcb->flags &= ~TF_NAGLEMEMERR;
  return ERR_OK;
}

/**
 * Called by tcp_output() to actually send a TCP segment over IP.
 *
 * @param seg the tcp_seg to send
 * @param pcb the tcp_pcb for the TCP connection used to send the segment
 * @param netif the netif used to send the segment
 */
static err_t
tcp_output_segment(struct tcp_seg *seg, struct tcp_pcb *pcb, struct netif *netif)
{
  err_t err;
  u16_t len;
  u32_t *opts;

  if (seg->p->ref != 1) {
    /* This can happen if the pbuf of this segment is still referenced by the
       netif driver due to deferred transmission. Since this function modifies
       p->len, we must not continue in this case. */
    return ERR_OK;
  }

  /* The TCP header has already been constructed, but the ackno and
   wnd fields remain. */
  seg->tcphdr->ackno = htonl(pcb->rcv_nxt);

  /* advertise our receive window size in this TCP segment */
  seg->tcphdr->wnd = htons(TCPWND_MIN16(RCV_WND_SCALE(pcb, pcb->rcv_ann_wnd)));

  pcb->rcv_ann_right_edge = pcb->rcv_nxt + pcb->rcv_ann_wnd;

  /* Add any requested options.  NB MSS option is only set on SYN
     packets, so ignore it here */
  opts = (u32_t *)(void *)(seg->tcphdr + 1);
  if (seg->flags & TF_SEG_OPTS_MSS) {
    u16_t mss;
#if TCP_CALCULATE_EFF_SEND_MSS
    mss = tcp_eff_send_mss(TCP_MSS, &pcb->local_ip, &pcb->remote_ip, PCB_ISIPV6(pcb));
#else /* TCP_CALCULATE_EFF_SEND_MSS */
    mss = TCP_MSS;
#endif /* TCP_CALCULATE_EFF_SEND_MSS */
    *opts = TCP_BUILD_MSS_OPTION(mss);
    opts += 1;
  }
#if LWIP_TCP_TIMESTAMPS
  pcb->ts_lastacksent = pcb->rcv_nxt;

  if (seg->flags & TF_SEG_OPTS_TS) {
    tcp_build_timestamp_option(pcb, opts);
    opts += 3;
  }
#endif

  /* Set retransmission timer running if it is not currently enabled
     This must be set before checking the route. */
  if (pcb->rtime < 0) {
    pcb->rtime = 0;
  }

  if (pcb->rttest == 0) {
    pcb->rttest = tcp_ticks;
    pcb->rtseq = ntohl(seg->tcphdr->seqno);

    LWIP_DEBUGF(TCP_RTO_DEBUG, ("tcp_output_segment: rtseq %"U32_F"\n", pcb->rtseq));
  }
  LWIP_DEBUGF(TCP_OUTPUT_DEBUG, ("tcp_output_segment: %"U32_F":%"U32_F"\n",
          htonl(seg->tcphdr->seqno), htonl(seg->tcphdr->seqno) +
          seg->len));

  len = (u16_t)((u8_t *)seg->tcphdr - (u8_t *)seg->p->payload);
  if (len == 0) {
    /** Exclude retransmitted segments from this count. */
    MIB2_STATS_INC(mib2.tcpoutsegs);
  }

  seg->p->len -= len;
  seg->p->tot_len -= len;

  seg->p->payload = seg->tcphdr;

  seg->tcphdr->chksum = 0;
#if CHECKSUM_GEN_TCP
  IF__NETIF_CHECKSUM_ENABLED(netif, NETIF_CHECKSUM_GEN_TCP) {
    seg->tcphdr->chksum = ip_chksum_pseudo(seg->p, IP_PROTO_TCP,
      seg->p->tot_len, &pcb->local_ip, &pcb->remote_ip);
  }
#endif /* CHECKSUM_GEN_TCP */
  TCP_STATS_INC(tcp.xmit);

  NETIF_SET_HWADDRHINT(netif, &(pcb->addr_hint));
  err = ip_output_if(PCB_ISIPV6(pcb), seg->p, &pcb->local_ip, &pcb->remote_ip, pcb->ttl,
    pcb->tos, IP_PROTO_TCP, netif);
  NETIF_SET_HWADDRHINT(netif, NULL);
  return err;
}

/**
 * Send a TCP RESET packet (empty segment with RST flag set) either to
 * abort a connection or to show that there is no matching local connection
 * for a received segment.
 *
 * Called by tcp_abort() (to abort a local connection), tcp_input() (if no
 * matching local pcb was found), tcp_listen_input() (if incoming segment
 * has ACK flag set) and tcp_process() (received segment in the wrong state)
 *
 * Since a RST segment is in most cases not sent for an active connection,
 * tcp_rst() has a number of arguments that are taken from a tcp_pcb for
 * most other segment output functions.
 *
 * @param seqno the sequence number to use for the outgoing segment
 * @param ackno the acknowledge number to use for the outgoing segment
 * @param local_ip the local IP address to send the segment from
 * @param remote_ip the remote IP address to send the segment to
 * @param local_port the local TCP port to send the segment from
 * @param remote_port the remote TCP port to send the segment to
 */
void
tcp_rst(u32_t seqno, u32_t ackno,
  const ip_addr_t *local_ip, const ip_addr_t *remote_ip,
  u16_t local_port, u16_t remote_port)
{
  struct pbuf *p;
  struct tcp_hdr *tcphdr;
  struct netif *netif;
  p = pbuf_alloc_pool(PBUF_IP, TCP_HLEN, TCP_HLEN);
  if (p == NULL) {
    LWIP_DEBUGF(TCP_DEBUG, ("tcp_rst: could not allocate memory for pbuf\n"));
    return;
  }
  LWIP_ASSERT("check that first pbuf can hold struct tcp_hdr",
              (p->len >= sizeof(struct tcp_hdr)));

  tcphdr = (struct tcp_hdr *)p->payload;
  tcphdr->src = htons(local_port);
  tcphdr->dest = htons(remote_port);
  tcphdr->seqno = htonl(seqno);
  tcphdr->ackno = htonl(ackno);
  TCPH_HDRLEN_FLAGS_SET(tcphdr, TCP_HLEN/4, TCP_RST | TCP_ACK);
  tcphdr->wnd = PP_HTONS(TCP_WND);
  tcphdr->chksum = 0;
  tcphdr->urgp = 0;

  TCP_STATS_INC(tcp.xmit);
  MIB2_STATS_INC(mib2.tcpoutrsts);

  netif = ip_route(IP_IS_V6(remote_ip), local_ip, remote_ip);
  if (netif != NULL) {
#if CHECKSUM_GEN_TCP
    IF__NETIF_CHECKSUM_ENABLED(netif, NETIF_CHECKSUM_GEN_TCP) {
      tcphdr->chksum = ip_chksum_pseudo(p, IP_PROTO_TCP, p->tot_len,
                                        local_ip, remote_ip);
    }
#endif
    /* Send output with hardcoded TTL/HL since we have no access to the pcb */
    ip_output_if(IP_IS_V6(remote_ip), p, local_ip, remote_ip, TCP_TTL, 0, IP_PROTO_TCP, netif);
  }
  pbuf_free(p);
  LWIP_DEBUGF(TCP_RST_DEBUG, ("tcp_rst: seqno %"U32_F" ackno %"U32_F".\n", seqno, ackno));
}

/**
 * Requeue all unacked segments for retransmission
 *
 * Called by tcp_slowtmr() for slow retransmission.
 *
 * @param pcb the tcp_pcb for which to re-enqueue all unacked segments
 */
void
tcp_rexmit_rto(struct tcp_pcb *pcb)
{
  struct tcp_seg *seg;

  if (pcb->unacked == NULL) {
    return;
  }

  /* Move all unacked segments to the head of the unsent queue */
  for (seg = pcb->unacked; seg->next != NULL; seg = seg->next);
  /* concatenate unsent queue after unacked queue */
  seg->next = pcb->unsent;
  /* unsent queue is the concatenated queue (of unacked, unsent) */
  pcb->unsent = pcb->unacked;
  /* unacked queue is now empty */
  pcb->unacked = NULL;

  /* increment number of retransmissions */
  ++pcb->nrtx;

  /* Don't take any RTT measurements after retransmitting. */
  pcb->rttest = 0;

  /* Do the actual retransmission */
  tcp_output(pcb);
}

/**
 * Requeue the first unacked segment for retransmission
 *
 * Called by tcp_receive() for fast retramsmit.
 *
 * @param pcb the tcp_pcb for which to retransmit the first unacked segment
 */
void
tcp_rexmit(struct tcp_pcb *pcb)
{
  struct tcp_seg *seg;
  struct tcp_seg **cur_seg;

  if (pcb->unacked == NULL) {
    return;
  }

  /* Move the first unacked segment to the unsent queue */
  /* Keep the unsent queue sorted. */
  seg = pcb->unacked;
  pcb->unacked = seg->next;

  cur_seg = &(pcb->unsent);
  while (*cur_seg &&
    TCP_SEQ_LT(ntohl((*cur_seg)->tcphdr->seqno), ntohl(seg->tcphdr->seqno))) {
      cur_seg = &((*cur_seg)->next );
  }
  seg->next = *cur_seg;
  *cur_seg = seg;

  ++pcb->nrtx;

  /* Don't take any rtt measurements after retransmitting. */
  pcb->rttest = 0;

  /* Do the actual retransmission. */
  MIB2_STATS_INC(mib2.tcpretranssegs);
  /* No need to call tcp_output: we are always called from tcp_input()
     and thus tcp_output directly returns. */
}


/**
 * Handle retransmission after three dupacks received
 *
 * @param pcb the tcp_pcb for which to retransmit the first unacked segment
 */
void
tcp_rexmit_fast(struct tcp_pcb *pcb)
{
  if (pcb->unacked != NULL && !(pcb->flags & TF_INFR)) {
    /* This is fast retransmit. Retransmit the first unacked segment. */
    LWIP_DEBUGF(TCP_FR_DEBUG,
                ("tcp_receive: dupacks %"U16_F" (%"U32_F
                 "), fast retransmit %"U32_F"\n",
                 (u16_t)pcb->dupacks, pcb->lastack,
                 ntohl(pcb->unacked->tcphdr->seqno)));
    tcp_rexmit(pcb);

    /* Set ssthresh to half of the minimum of the current
     * cwnd and the advertised window */
    if (pcb->cwnd > pcb->snd_wnd) {
      pcb->ssthresh = pcb->snd_wnd / 2;
    } else {
      pcb->ssthresh = pcb->cwnd / 2;
    }
    
    /* The minimum value for ssthresh should be 2 MSS */
    if (pcb->ssthresh < (2U * pcb->mss)) {
      LWIP_DEBUGF(TCP_FR_DEBUG, 
                  ("tcp_receive: The minimum value for ssthresh %"TCPWNDSIZE_F
                   " should be min 2 mss %"U16_F"...\n",
                   pcb->ssthresh, (u16_t)(2*pcb->mss)));
      pcb->ssthresh = 2*pcb->mss;
    }
    
    pcb->cwnd = pcb->ssthresh + 3 * pcb->mss;
    pcb->flags |= TF_INFR;
    
    /* Reset the retransmission timer to prevent immediate rto retransmissions */
    pcb->rtime = 0;
  } 
}


/**
 * Send keepalive packets to keep a connection active although
 * no data is sent over it.
 *
 * Called by tcp_slowtmr()
 *
 * @param pcb the tcp_pcb for which to send a keepalive packet
 */
err_t
tcp_keepalive(struct tcp_pcb *pcb)
{
  err_t err;
  struct pbuf *p;
  struct netif *netif;

  LWIP_DEBUGF(TCP_DEBUG, ("tcp_keepalive: sending KEEPALIVE probe to "));
  ip_addr_debug_print(TCP_DEBUG, &pcb->remote_ip);
  LWIP_DEBUGF(TCP_DEBUG, ("\n"));

  LWIP_DEBUGF(TCP_DEBUG, ("tcp_keepalive: tcp_ticks %"U32_F"   pcb->tmr %"U32_F" pcb->keep_cnt_sent %"U16_F"\n",
                          tcp_ticks, pcb->tmr, (u16_t)pcb->keep_cnt_sent));

  p = tcp_output_alloc_header(pcb, 0, 0, htonl(pcb->snd_nxt - 1));
  if (p == NULL) {
    LWIP_DEBUGF(TCP_DEBUG,
                ("tcp_keepalive: could not allocate memory for pbuf\n"));
    return ERR_MEM;
  }
  netif = ip_route(PCB_ISIPV6(pcb), &pcb->local_ip, &pcb->remote_ip);
  if (netif == NULL) {
    err = ERR_RTE;
  } else {
#if CHECKSUM_GEN_TCP
    IF__NETIF_CHECKSUM_ENABLED(netif, NETIF_CHECKSUM_GEN_TCP) {
      struct tcp_hdr *tcphdr = (struct tcp_hdr *)p->payload;
      tcphdr->chksum = ip_chksum_pseudo(p, IP_PROTO_TCP, p->tot_len,
        &pcb->local_ip, &pcb->remote_ip);
    }
#endif /* CHECKSUM_GEN_TCP */
    TCP_STATS_INC(tcp.xmit);

    /* Send output to IP */
    NETIF_SET_HWADDRHINT(netif, &(pcb->addr_hint));
    err = ip_output_if(PCB_ISIPV6(pcb), p, &pcb->local_ip, &pcb->remote_ip, pcb->ttl,
      0, IP_PROTO_TCP, netif);
    NETIF_SET_HWADDRHINT(netif, NULL);
  }
  pbuf_free(p);

  LWIP_DEBUGF(TCP_DEBUG, ("tcp_keepalive: seqno %"U32_F" ackno %"U32_F" err %d.\n",
                          pcb->snd_nxt - 1, pcb->rcv_nxt, (int)err));
  return err;
}


/**
 * Send persist timer zero-window probes to keep a connection active
 * when a window update is lost.
 *
 * Called by tcp_slowtmr()
 *
 * @param pcb the tcp_pcb for which to send a zero-window probe packet
 */
err_t
tcp_zero_window_probe(struct tcp_pcb *pcb)
{
  err_t err;
  struct pbuf *p;
  struct tcp_hdr *tcphdr;
  struct tcp_seg *seg;
  u16_t len;
  u8_t is_fin;
  struct netif *netif;

  LWIP_DEBUGF(TCP_DEBUG, ("tcp_zero_window_probe: sending ZERO WINDOW probe to "));
  ip_addr_debug_print(TCP_DEBUG, &pcb->remote_ip);
  LWIP_DEBUGF(TCP_DEBUG, ("\n"));

  LWIP_DEBUGF(TCP_DEBUG,
              ("tcp_zero_window_probe: tcp_ticks %"U32_F
               "   pcb->tmr %"U32_F" pcb->keep_cnt_sent %"U16_F"\n",
               tcp_ticks, pcb->tmr, (u16_t)pcb->keep_cnt_sent));

  seg = pcb->unacked;

  if (seg == NULL) {
    seg = pcb->unsent;
  }
  if (seg == NULL) {
    /* nothing to send, zero window probe not needed */
    return ERR_OK;
  }

  is_fin = ((TCPH_FLAGS(seg->tcphdr) & TCP_FIN) != 0) && (seg->len == 0);
  /* we want to send one seqno: either FIN or data (no options) */
  len = is_fin ? 0 : 1;

  p = tcp_output_alloc_header(pcb, 0, len, seg->tcphdr->seqno);
  if (p == NULL) {
    LWIP_DEBUGF(TCP_DEBUG, ("tcp_zero_window_probe: no memory for pbuf\n"));
    return ERR_MEM;
  }
  tcphdr = (struct tcp_hdr *)p->payload;

  if (is_fin) {
    /* FIN segment, no data */
    TCPH_FLAGS_SET(tcphdr, TCP_ACK | TCP_FIN);
  } else {
    /* Data segment, copy in one byte from the head of the unacked queue */
    char *d = ((char *)p->payload + TCP_HLEN);
    /* Depending on whether the segment has already been sent (unacked) or not
       (unsent), seg->p->payload points to the IP header or TCP header.
       Ensure we copy the first TCP data byte: */
    pbuf_copy_partial(seg->p, d, 1, seg->p->tot_len - seg->len);
  }

  netif = ip_route(PCB_ISIPV6(pcb), &pcb->local_ip, &pcb->remote_ip);
  if (netif == NULL) {
    err = ERR_RTE;
  } else {
#if CHECKSUM_GEN_TCP
    IF__NETIF_CHECKSUM_ENABLED(netif, NETIF_CHECKSUM_GEN_TCP) {
      tcphdr->chksum = ip_chksum_pseudo(p, IP_PROTO_TCP, p->tot_len,
        &pcb->local_ip, &pcb->remote_ip);
    }
#endif
    TCP_STATS_INC(tcp.xmit);

    /* Send output to IP */
    NETIF_SET_HWADDRHINT(netif, &(pcb->addr_hint));
    err = ip_output_if(PCB_ISIPV6(pcb), p, &pcb->local_ip, &pcb->remote_ip, pcb->ttl,
      0, IP_PROTO_TCP, netif);
    NETIF_SET_HWADDRHINT(netif, NULL);
  }

  pbuf_free(p);

  LWIP_DEBUGF(TCP_DEBUG, ("tcp_zero_window_probe: seqno %"U32_F
                          " ackno %"U32_F" err %d.\n",
                          pcb->snd_nxt - 1, pcb->rcv_nxt, (int)err));
  return err;
}
#endif /* LWIP_TCP */
