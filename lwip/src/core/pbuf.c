/**
 * @file
 * Packet buffer management
 *
 * Packets are built from the pbuf data structure. It supports dynamic
 * memory allocation for packet contents or can reference externally
 * managed packet contents both in RAM and ROM. Quick allocation for
 * incoming packets is provided through pools with fixed sized pbufs.
 *
 * A packet may span over multiple pbufs, chained as a singly linked
 * list. This is called a "pbuf chain". The last pbuf of the chain is
 * indicated by a NULL ->next field.
 * 
 * The ->tot_len field of a pbuf is the sum of the ->len fields of
 * that pbuf and all the subsequent pbufs in the chain. This invariant
 * must be maintained.
 * 
 * WARNING: When using pbuf_header, pbuf_header_force and pbuf_unheader
 * to hide or reveal headers, if the pbuf being changed is not the first
 * in a chain, you must then immediately manually correct the tot_len
 * of the preceding pbufs. This is because various functions in this
 * module may check (using assertions) the tot_len fields of pbufs they
 * encounter. 
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

#include "lwip/stats.h"
#include "lwip/def.h"
#include "lwip/memp.h"
#include "lwip/pbuf.h"
#include "lwip/sys.h"
#if LWIP_TCP
#include "lwip/tcp_impl.h"
#endif

#include <string.h>

/** Get the pointer to the start of available data for a pbuf that has data in the pool. */
#define pbuf_pool_payload(p) ((u8_t *)((struct pbuf_pool_elem_head *)(p))->payload)

#ifndef LWIP_NOASSERT

/**
 * Does basic sanity checks for a pbuf (valid type, len<=tot_len).
 * No null check is done.
 */
#define pbuf_basic_sanity(p) do { \
  LWIP_ASSERT("sane pbuf type", pbuf_type_sane((p)->type)); \
  LWIP_ASSERT("sane pbuf len", (p)->len <= (p)->tot_len); \
} while (0)

/**
 * Declares variables needed for sanity testing of pbuf chains.
 * 
 * Usage of this chain sanity testing is as follows:
 * 1) Declare needed local variables using pbuf_sanity_decl.
 * 2) Initialize using pbuf_sanity_start, pbuf_sanity_start_second
 *    or pbuf_sanity_start_visit.
 * 3) Visit pbufs as you encounter them using pbuf_sanity_visit
 *    or pbuf_sanity_next.
 * 4) Optionally, visit the remainder of the chain using pbuf_sanity_end.
 * 
 * @param rem_tot_len name of variable, possibly prefix in the future
 */
#define pbuf_sanity_decl(rem_tot_len) \
u16_t rem_tot_len;

/**
 * Initializes pbuf chain sanity testing.
 * A valid first pbuf must be passed but it is not visited.
 * 
 * This remembers the tot_len of the first pbuf as rem_tot_len.
 * As each pbuf is is visited, its tot_len will be asserted equal
 * to rem_tot_len, and rem_tot_len will be decremented by its len.
 * 
 * @param p the first pbuf in the chain (not null)
 */
#define pbuf_sanity_start(rem_tot_len, p) do { \
  rem_tot_len = (p)->tot_len; \
} while (0)

/**
 * Initializes pbuf iteration sanity testing while implicitly
 * visiting the firstpbuf in the chain. Use when pbuf_basic_sanity
 * has already been done on the first pbuf, otherwise use
 * pbuf_sanity_start_visit.
 * 
 * @param p the first pbuf in the chain (not null)
 */
#define pbuf_sanity_start_second(rem_tot_len, p) do { \
  rem_tot_len = (p)->tot_len - (p)->len; \
} while (0)

/**
 * Visits a pbuf as part of pbuf chain sanity testing.
 * This first does a pbuf_basic_sanity check, then checks its
 * tot_len and updates the rem_tot_len.
 * 
 * @param p the next (unvisited) pbuf (not null)
 */
#define pbuf_sanity_visit(rem_tot_len, p) do { \
  pbuf_basic_sanity(p); \
  LWIP_ASSERT("expected pbuf tot_len", (p)->tot_len == rem_tot_len); \
  rem_tot_len -= (p)->len; \
} while (0)

/**
 * Visits the remainder of the chain as part of pbuf chain sanity testing.
 * Note that the remainder be null (if the chain ends) or a pbuf,
 * because it is not needed to iterate the chain in its entirety.
 * 
 * This checks two things:
 * - If the rem_tot_len is nonzero (more data is expected in the chain),
 *   that the remainder p is not null.
 * - If the remainder p is not null, that p->tot_len is equal to rem_tot_len.
 * 
 * @param p the remainder of the chain, not yet visited (may be null)
 */
#define pbuf_sanity_end(rem_tot_len, p) do { \
  LWIP_ASSERT("pbuf chain shouldn't end", rem_tot_len == 0 || (p) != NULL); \
  LWIP_ASSERT("bad tot_len in rest of pbuf chain", (p) == NULL || (p)->tot_len == rem_tot_len); \
} while (0)

/**
 * Visits the next pbuf or the null end of the chain as part of pbuf chain
 * sanity testing. Typically used just after "p = p->next" where naturally
 * the resulting p may be null.
 * 
 * @param p the next (unvisited) pbuf, or null to visit the end
 */
#define pbuf_sanity_next(rem_tot_len, p) do { \
  if ((p) != NULL) { \
    pbuf_sanity_visit(rem_tot_len, p); \
  } else { \
    pbuf_sanity_end(rem_tot_len, p); \
  } \
} while (0)

/**
 * Initializes pbuf iteration sanity and visits the first pbuf.
 * 
 * @param p the first pbuf in the chain (not null)
 */
#define pbuf_sanity_start_visit(rem_tot_len, p) do { \
  pbuf_sanity_start(rem_tot_len, p); \
  pbuf_sanity_visit(rem_tot_len, p); \
} while (0)

#else
#define pbuf_basic_sanity(p)
#define pbuf_sanity_decl(rem_tot_len)
#define pbuf_sanity_start(rem_tot_len, p)
#define pbuf_sanity_start_second(rem_tot_len, p)
#define pbuf_sanity_visit(rem_tot_len, p)
#define pbuf_sanity_end(rem_tot_len, p)
#define pbuf_sanity_next(rem_tot_len, p)
#define pbuf_sanity_start_visit(rem_tot_len, p)
#endif

static u8_t pbuf_type_sane(pbuf_type type)
{
    return type == PBUF_ROM || type == PBUF_REF || type == PBUF_POOL || type == PBUF_TCP;
}

static u8_t pbuf_get_offset_for_layer(pbuf_layer layer, u16_t *offset)
{
  switch (layer) {
  case PBUF_TRANSPORT:
    /* add room for transport (often TCP) layer header */
    *offset = PBUF_LINK_ENCAPSULATION_HLEN + PBUF_LINK_HLEN + PBUF_IP_HLEN + PBUF_TRANSPORT_HLEN;
    break;
  case PBUF_IP:
    /* add room for IP layer header */
    *offset = PBUF_LINK_ENCAPSULATION_HLEN + PBUF_LINK_HLEN + PBUF_IP_HLEN;
    break;
  case PBUF_LINK:
    /* add room for link layer header */
    *offset = PBUF_LINK_ENCAPSULATION_HLEN + PBUF_LINK_HLEN;
    break;
  case PBUF_RAW_TX:
    /* add room for encapsulating link layer headers (e.g. 802.11) */
    *offset = PBUF_LINK_ENCAPSULATION_HLEN;
    break;
  case PBUF_RAW:
    /* no offset (e.g. RX buffers or chain successors) */
    *offset = 0;
    break;
  default:
    return 0;
  }
  return 1;
}

/**
 * Allocates a pbuf of the given type (possibly a chain for PBUF_POOL type).
 *
 * The actual memory allocated for the pbuf is determined by the
 * layer at which the pbuf is allocated and the requested size
 * (from the size parameter).
 *
 * @param layer flag to define header size
 * @param length size of the pbuf's payload
 * @param type this parameter decides how and where the pbuf
 * should be allocated as follows:
 *
 * - PBUF_ROM: no buffer memory is allocated for the pbuf, even for
 *             protocol headers. Additional headers must be prepended
 *             by allocating another pbuf and chain in to the front of
 *             the ROM pbuf. It is assumed that the memory used is really
 *             similar to ROM in that it is immutable and will not be
 *             changed. Memory which is dynamic should generally not
 *             be attached to PBUF_ROM pbufs. Use PBUF_REF instead.
 * - PBUF_REF: no buffer memory is allocated for the pbuf, even for
 *             protocol headers. It is assumed that the pbuf is only
 *             being used in a single thread. If the pbuf gets queued,
 *             then pbuf_take should be called to copy the buffer.
 * - PBUF_POOL: the pbuf is allocated as a pbuf chain, with pbufs from
 *              the pbuf pool that is allocated during pbuf_init().
 *
 * @return the allocated pbuf. If multiple pbufs where allocated, this
 * is the first pbuf of a pbuf chain.
 */
struct pbuf *
pbuf_alloc(pbuf_layer layer, u16_t length, pbuf_type type)
{
  struct pbuf *p, *q, *r;
  u16_t offset;
  u16_t rem_len; /* remaining length */
  LWIP_DEBUGF(PBUF_DEBUG | LWIP_DBG_TRACE, ("pbuf_alloc(length=%"U16_F")\n", length));

  if (!pbuf_get_offset_for_layer(layer, &offset)) {
    LWIP_ASSERT("pbuf_alloc: bad pbuf layer", 0);
    return NULL;
  }

  switch (type) {
  case PBUF_POOL:
    /* check that the offset fits into one pbuf element */
    if (offset > PBUF_POOL_BUFSIZE) {
      LWIP_DEBUGF(PBUF_DEBUG | LWIP_DBG_TRACE, ("pbuf_alloc: offset too large\n"));
      return NULL;
    }
    
    /* allocate head of pbuf chain into p */
    p = (struct pbuf *)memp_malloc(MEMP_PBUF_POOL);
    LWIP_DEBUGF(PBUF_DEBUG | LWIP_DBG_TRACE, ("pbuf_alloc: allocated pbuf %p\n", (void *)p));
    if (p == NULL) {
      return NULL;
    }
    p->type = type;
    p->next = NULL;

    /* make the payload pointer point 'offset' bytes into pbuf data memory */
    p->payload = pbuf_pool_payload(p) + offset;
    /* the total length of the pbuf chain is the requested size */
    p->tot_len = length;
    /* set the length of the first pbuf in the chain */
    p->len = LWIP_MIN(length, PBUF_POOL_BUFSIZE - offset);
    /* set reference count (needed here in case we fail) */
    p->ref = 1;

    /* now allocate the tail of the pbuf chain */

    /* remember first pbuf for linkage in next iteration */
    r = p;
    /* remaining length to be allocated */
    rem_len = length - p->len;
    /* any remaining pbufs to be allocated? */
    while (rem_len > 0) {
      q = (struct pbuf *)memp_malloc(MEMP_PBUF_POOL);
      if (q == NULL) {
        /* free chain so far allocated */
        pbuf_free(p);
        /* bail out unsuccessfully */
        return NULL;
      }
      q->type = type;
      q->flags = 0;
      q->next = NULL;
      /* make previous pbuf point to this pbuf */
      r->next = q;
      /* set total length of this pbuf and next in chain */
      q->tot_len = rem_len;
      /* this pbuf length is pool size, unless smaller sized tail */
      q->len = LWIP_MIN(rem_len, PBUF_POOL_BUFSIZE);
      q->payload = pbuf_pool_payload(q);
      q->ref = 1;
      /* calculate remaining length to be allocated */
      rem_len -= q->len;
      /* remember this pbuf for linkage in next iteration */
      r = q;
    }

    break;
    
  case PBUF_TCP:
    LWIP_ASSERT("pbuf_alloc: layer for PBUF_TCP is PBUF_TRANSPORT", layer == PBUF_TRANSPORT);
    LWIP_ASSERT("pbuf_alloc: length for PBUF_TCP is <= LWIP_TCP_MAX_OPT_LENGTH", length <= LWIP_TCP_MAX_OPT_LENGTH);
    
    p = (struct pbuf *)memp_malloc(MEMP_PBUF_TCP);
    if (p == NULL) {
      return NULL;
    }
    p->payload = pbuf_pool_payload(p) + offset;
    p->len = length;
    p->tot_len = length;
    p->next = NULL;
    p->type = type;
    break;
    
  /* pbuf references existing (non-volatile static constant) ROM payload? */
  case PBUF_ROM:
  /* pbuf references existing (externally allocated) RAM payload? */
  case PBUF_REF:
    /* only allocate memory for the pbuf structure */
    p = (struct pbuf *)memp_malloc(MEMP_PBUF);
    if (p == NULL) {
      return NULL;
    }
    /* caller must set this field properly, afterwards */
    p->payload = NULL;
    p->len = length;
    p->tot_len = length;
    p->next = NULL;
    p->type = type;
    break;
    
  default:
    LWIP_ASSERT("pbuf_alloc: erroneous type", 0);
    return NULL;
  }
  
  /* set reference count */
  p->ref = 1;
  /* set flags */
  p->flags = 0;
  
  LWIP_DEBUGF(PBUF_DEBUG | LWIP_DBG_TRACE, ("pbuf_alloc(length=%"U16_F") == %p\n", length, (void *)p));
  return p;
}

/**
 * Like pbuf_alloc, but also checks that the length (after header space)
 * of the first pbuf in the chain is at least min_first_len.
 */
struct pbuf *
pbuf_alloc_pool(pbuf_layer layer, u16_t length, u16_t min_first_len)
{
  struct pbuf *p = pbuf_alloc(layer, length, PBUF_POOL);
  if (p && p->len < min_first_len) {
    LWIP_DEBUGF(PBUF_DEBUG | LWIP_DBG_TRACE, ("pbuf_alloc_pool: min_first_len not satisifed\n"));
    pbuf_free(p);
    p = NULL;
  }
  return p;
}

#if LWIP_SUPPORT_CUSTOM_PBUF
/** Initialize a custom pbuf (already allocated).
 *
 * @param l flag to define header size
 * @param length size of the pbuf's payload
 * @param type type of the pbuf (only used to treat the pbuf accordingly, as
 *        this function allocates no memory)
 * @param p pointer to the custom pbuf to initialize (already allocated)
 * @param payload_mem pointer to the buffer that is used for payload and headers,
 *        must be at least big enough to hold 'length' plus the header size,
 *        may be NULL if set later.
 *        ATTENTION: The caller is responsible for correct alignment of this buffer!!
 * @param payload_mem_len the size of the 'payload_mem' buffer, must be at least
 *        big enough to hold 'length' plus the header size
 */
struct pbuf*
pbuf_alloced_custom(pbuf_layer l, u16_t length, pbuf_type type, struct pbuf_custom *p,
                    void *payload_mem, u16_t payload_mem_len)
{
  u16_t offset;
  LWIP_DEBUGF(PBUF_DEBUG | LWIP_DBG_TRACE, ("pbuf_alloced_custom(length=%"U16_F")\n", length));
  LWIP_ASSERT("pbuf_alloced_custom: sane type", pbuf_type_sane(type));

  if (!pbuf_get_offset_for_layer(l, &offset)) {
    LWIP_ASSERT("pbuf_alloced_custom: bad pbuf layer", 0);
    return NULL;
  }

  if (offset + length > payload_mem_len) {
    LWIP_DEBUGF(PBUF_DEBUG | LWIP_DBG_LEVEL_WARNING, ("pbuf_alloced_custom(length=%"U16_F") buffer too short\n", length));
    return NULL;
  }

  p->pbuf.next = NULL;
  p->pbuf.payload = (payload_mem != NULL) ? ((u8_t *)payload_mem + offset) : NULL;
  p->pbuf.flags = PBUF_FLAG_IS_CUSTOM;
  p->pbuf.len = length;
  p->pbuf.tot_len = length;
  p->pbuf.type = type;
  p->pbuf.ref = 1;
  
  return &p->pbuf;
}
#endif /* LWIP_SUPPORT_CUSTOM_PBUF */

/**
 * Shrink a pbuf chain to a desired length.
 *
 * @param p pbuf to shrink.
 * @param new_len desired new length of pbuf chain
 *
 * Depending on the desired length, the first few pbufs in a chain might
 * be skipped and left unchanged. The new last pbuf in the chain will be
 * resized, and any remaining pbufs will be freed.
 *
 * @note If the pbuf is ROM/REF, only the ->tot_len and ->len fields are adjusted.
 *
 * @note Despite its name, pbuf_realloc cannot grow the size of a pbuf (chain).
 */
void
pbuf_realloc(struct pbuf *p, u16_t new_len)
{
  struct pbuf *q;
  u16_t rem_len; /* remaining length */
  u16_t shrink;
  pbuf_sanity_decl(sanity)

  LWIP_ASSERT("pbuf_realloc: p != NULL", p != NULL);
  pbuf_basic_sanity(p);

  /* can only shrink */
  if (new_len >= p->tot_len) {
    return;
  }

  /* by how much we are shrinking */
  shrink = p->tot_len - new_len;

  /* first, step over any pbufs that should remain in the chain */
  rem_len = new_len;
  q = p;
  pbuf_sanity_start_second(sanity, q);
  
  /* should this pbuf be kept? */
  while (rem_len > q->len) {
    /* decrease remaining length by pbuf length */
    rem_len -= q->len;
    /* decrease total length indicator */
    q->tot_len -= shrink;
    /* proceed to next pbuf in chain */
    q = q->next;
    
    LWIP_ASSERT("pbuf_realloc: q != NULL", q != NULL);
    pbuf_sanity_visit(sanity, q);
  }
  
  pbuf_sanity_end(sanity, q->next);
  
  /* we have now reached the new last pbuf (in q) */
  /* rem_len == desired length for pbuf q */

  /* adjust length fields for new last pbuf */
  q->len = rem_len;
  q->tot_len = q->len;

  /* any remaining pbufs in chain? */
  if (q->next != NULL) {
    /* free remaining pbufs in chain */
    pbuf_free(q->next);
  }
  /* q is last packet in chain */
  q->next = NULL;
}

/**
 * Common implementation of pbuf_header and pbuf_header_force.
 */
static u8_t
pbuf_header_impl(struct pbuf *p, u16_t header_size, u8_t force)
{
  void *payload;

  LWIP_ASSERT("pbuf_header: p != NULL", p != NULL);
  pbuf_basic_sanity(p);
  
  if (header_size == 0) {
    return 0;
  }

  if (p->type == PBUF_POOL || p->type == PBUF_TCP) {
    u8_t *payload_start = pbuf_pool_payload(p);
    LWIP_ASSERT("payload>=payload_start",  (u8_t *)p->payload >= payload_start);
    LWIP_ASSERT("payload<=payload_end",    (u8_t *)p->payload <= payload_start + PBUF_POOL_BUFSIZE);
    
    if (header_size > (u8_t *)p->payload - payload_start) {
      if (force) {
        LWIP_ASSERT("pbuf_header: not enough space for new header size", 0);
      } else {
        LWIP_DEBUGF(PBUF_DEBUG | LWIP_DBG_TRACE, ("pbuf_header: not enough space for new header size\n"));
        return 1;
      }
    }
  } else {
    if (!force) {
      /* cannot expand payload to front */
      return 1;
    }
  }
  
  /* modify pbuf payload and length fields */
  payload = p->payload;
  p->payload = (u8_t *)p->payload - header_size;
  p->len     += header_size;
  p->tot_len += header_size;

  LWIP_DEBUGF(PBUF_DEBUG | LWIP_DBG_TRACE, ("pbuf_header: old %p new %p (%"U16_F")\n",
    payload, p->payload, header_size));

  return 0;
}

/**
 * Adjusts the payload pointer to reveal headers in the payload
 * after checking that sufficient space is available.
 * 
 * This is safe to use to without knowing if space is available
 * for the headers, to check if it is. But note that this check can
 * only be done for pbuf types that store data in the pools
 * (PBUF_POOL, PBUF_TCP). For other pbuf types this will always fail
 * unless header_size==0.
 * 
 * @param p pbuf in which to reveal headers (not null)
 * @param header_size number of bytes of headers to reveal
 * @return non-zero on failure, zero on success
 */
u8_t
pbuf_header(struct pbuf *p, u16_t header_size)
{
   return pbuf_header_impl(p, header_size, 0);
}

/**
 * Adjusts the payload pointer to reveal headers in the payload.
 * 
 * This may only be called when the caller knows that sufficient
 * space is available for the headers.
 * 
 * @param p pbuf in which to reveal headers (not null)
 * @param header_size number of bytes of headers to reveal
 */
void
pbuf_header_force(struct pbuf *p, u16_t header_size)
{
   u8_t res = pbuf_header_impl(p, header_size, 1);
   /* res is always 0 when calling with force=1 */
}

/**
 * Adjusts the payload pointer to hide headers in the payload.
 * 
 * @param p pbuf to change the header size (not null).
 * @param header_size Number of bytes of headers to hide.
 *        Must not exceed the number of bytes available in this
 *        pbuf (->len).
 */
void
pbuf_unheader(struct pbuf *p, u16_t header_size)
{
  void *payload;

  LWIP_ASSERT("pbuf_unheader: p != NULL", p != NULL);
  pbuf_basic_sanity(p);
  
  /* Check that we aren't going to move off the end of the pbuf */
  LWIP_ASSERT("header_size <= p->len", header_size <= p->len);

  /* modify pbuf payload and length fields */
  payload = p->payload;
  p->payload = (u8_t *)p->payload + header_size;
  p->len     -= header_size;
  p->tot_len -= header_size;

  LWIP_DEBUGF(PBUF_DEBUG | LWIP_DBG_TRACE, ("pbuf_unheader: old %p new %p (%"U16_F")\n",
    payload, p->payload, header_size));
}

/**
 * Dereference a pbuf chain and deallocate any no-longer-used
 * pbufs at the head of this chain.
 *
 * Decrements the pbuf reference count. If it reaches zero, the pbuf is
 * deallocated.
 *
 * For a pbuf chain, this is repeated for each pbuf in the chain,
 * up to the first pbuf which has a non-zero reference count after
 * decrementing. So, when all reference counts are one, the whole
 * chain is free'd.
 *
 * @param p The pbuf (chain) to be dereferenced.
 *
 * @note the reference counter of a pbuf equals the number of pointers
 * that refer to the pbuf (or into the pbuf).
 *
 * @internal examples:
 *
 * Assuming existing chains a->b->c with the following reference
 * counts, calling pbuf_free(a) results in:
 * 
 * 1->2->3 becomes ...1->3
 * 3->3->3 becomes 2->3->3
 * 1->1->2 becomes ......1
 * 2->1->1 becomes 1->1->1
 * 1->1->1 becomes .......
 *
 */
void
pbuf_free(struct pbuf *p)
{
  u16_t type;
  struct pbuf *q;
  u16_t ref;
  SYS_ARCH_DECL_PROTECT(old_level);
  pbuf_sanity_decl(sanity)

  LWIP_ERROR("pbuf_free: p != NULL", p != NULL, return;);
  
  LWIP_DEBUGF(PBUF_DEBUG | LWIP_DBG_TRACE, ("pbuf_free(%p)\n", (void *)p));

  PERF_START;

  pbuf_sanity_start(sanity, p);
  
  /* de-allocate all consecutive pbufs from the head of the chain that
   * obtain a zero reference count after decrementing*/
  do {
    pbuf_sanity_visit(sanity, p);
    
    /* Since decrementing ref cannot be guaranteed to be a single machine operation
     * we must protect it. We put the new ref into a local variable to prevent
     * further protection. */
    SYS_ARCH_PROTECT(old_level);
    /* all pbufs in a chain are referenced at least once */
    LWIP_ASSERT("pbuf_free: p->ref > 0", p->ref > 0);
    /* decrease reference count (number of pointers to pbuf) */
    ref = --(p->ref);
    SYS_ARCH_UNPROTECT(old_level);
    
    /* if this pbuf is still referenced, end here */
    if (ref > 0) {
      LWIP_DEBUGF( PBUF_DEBUG | LWIP_DBG_TRACE, ("pbuf_free: %p has ref %"U16_F", ending here.\n", (void *)p, ref));
      break;
    }
    
    /* remember next pbuf in chain for next iteration */
    q = p->next;
    
    LWIP_DEBUGF( PBUF_DEBUG | LWIP_DBG_TRACE, ("pbuf_free: deallocating %p\n", (void *)p));
    
    type = p->type;
#if LWIP_SUPPORT_CUSTOM_PBUF
    if ((p->flags & PBUF_FLAG_IS_CUSTOM) != 0) {
      struct pbuf_custom *pc = (struct pbuf_custom*)p;
      LWIP_ASSERT("pc->custom_free_function != NULL", pc->custom_free_function != NULL);
      pc->custom_free_function(p);
    } else
#endif
    {
      if (type == PBUF_POOL) {
        memp_free(MEMP_PBUF_POOL, p);
      } else if (type == PBUF_ROM || type == PBUF_REF) {
        memp_free(MEMP_PBUF, p);
      } else if (type == PBUF_TCP) {
        memp_free(MEMP_PBUF_TCP, p);
      } else {
        LWIP_ASSERT("pbuf_free: bad type", 0);
      }
    }
    
    /* proceed to next pbuf */
    p = q;
  } while (p != NULL);
  
  /* no pbuf_sanity_end because this can be called from pbuf_alloc with an incomplete chain */
  
  PERF_STOP("pbuf_free");
}

/**
 * Count number of pbufs in a chain
 *
 * @param p first pbuf of chain
 * @return the number of pbufs in a chain
 */
u8_t
pbuf_clen(struct pbuf *p)
{
  u8_t len;

  len = 0;
  while (p != NULL) {
    ++len;
    p = p->next;
  }
  return len;
}

/**
 * Increment the reference count of the pbuf.
 *
 * @param p pbuf to increase reference counter of
 */
void
pbuf_ref(struct pbuf *p)
{
  SYS_ARCH_DECL_PROTECT(old_level);
  
  LWIP_ERROR("p != NULL", p != NULL, return;);
  pbuf_basic_sanity(p);
  
  SYS_ARCH_PROTECT(old_level);
  ++(p->ref);
  SYS_ARCH_UNPROTECT(old_level);
}

/**
 * Concatenate two pbufs (each may be a pbuf chain) and take over
 * the caller's reference of the tail pbuf.
 * 
 * @note The caller MAY NOT reference the tail pbuf afterwards.
 * Use pbuf_chain() for that purpose.
 * 
 * @see pbuf_chain()
 */

void
pbuf_cat(struct pbuf *h, struct pbuf *t)
{
  struct pbuf *p;
  pbuf_sanity_decl(sanity)

  LWIP_ERROR("(h != NULL) && (t != NULL)", ((h != NULL) && (t != NULL)), return;);
  
  pbuf_sanity_start(sanity, h);
  
  /* proceed to last pbuf of chain */
  for (p = h; ; p = p->next) {
    pbuf_sanity_visit(sanity, p);
    
    if (p->next == NULL) {
      break;
    }
    
    /* add total length of second chain to all totals of first chain */
    p->tot_len += t->tot_len;
  }
  
  pbuf_sanity_end(sanity, p->next);
  
  /* { p is last pbuf of first h chain, p->next == NULL } */
  LWIP_ASSERT("p->tot_len == p->len (of last pbuf in chain)", p->tot_len == p->len);
  /* add total length of second chain to last pbuf total of first chain */
  p->tot_len += t->tot_len;
  /* chain last pbuf of head (p) with first of tail (t) */
  p->next = t;
  /* p->next now references t, but the caller will drop its reference to t,
   * so netto there is no change to the reference count of t.
   */
}

/**
 * Chain two pbufs (or pbuf chains) together.
 * 
 * The caller MUST call pbuf_free(t) once it has stopped
 * using it. Use pbuf_cat() instead if you no longer use t.
 * 
 * @param h head pbuf (chain)
 * @param t tail pbuf (chain)
 * @note The pbufs MUST belong to the same packet.
 *
 * The ->tot_len fields of all pbufs of the head chain are adjusted.
 * The ->next field of the last pbuf of the head chain is adjusted.
 * The ->ref field of the first pbuf of the tail chain is adjusted.
 */
void
pbuf_chain(struct pbuf *h, struct pbuf *t)
{
  pbuf_cat(h, t);
  /* t is now referenced by h */
  pbuf_ref(t);
  LWIP_DEBUGF(PBUF_DEBUG | LWIP_DBG_TRACE, ("pbuf_chain: %p references %p\n", (void *)h, (void *)t));
}

/**
 * Create copies of pbufs.
 *
 * Used to queue packets on behalf of the lwIP stack, such as
 * ARP based queueing.
 *
 * @note You MUST explicitly use p = pbuf_take(p);
 *
 * @param p_to pbuf destination of the copy
 * @param p_from pbuf source of the copy
 *
 * @return ERR_OK if pbuf was copied
 *         ERR_ARG if one of the pbufs is NULL or p_to is not big
 *                 enough to hold p_from
 */
err_t
pbuf_copy(struct pbuf *p_to, struct pbuf *p_from)
{
  u16_t offset_to=0, offset_from=0, len;
  pbuf_sanity_decl(sanity_to)
  pbuf_sanity_decl(sanity_from)

  LWIP_DEBUGF(PBUF_DEBUG | LWIP_DBG_TRACE, ("pbuf_copy(%p, %p)\n",
    (void*)p_to, (void*)p_from));

  /* is the target big enough to hold the source? */
  LWIP_ERROR("pbuf_copy: target not big enough to hold source", ((p_to != NULL) &&
             (p_from != NULL) && (p_to->tot_len >= p_from->tot_len)), return ERR_ARG;);

  pbuf_sanity_start_visit(sanity_to, p_to);
  pbuf_sanity_start_visit(sanity_from, p_from);
  
  /* iterate through pbuf chain */
  do {
    /* copy one part of the original chain */
    len = LWIP_MIN(p_to->len - offset_to, p_from->len - offset_from);
    MEMCPY((u8_t*)p_to->payload + offset_to, (u8_t*)p_from->payload + offset_from, len);
    offset_to += len;
    offset_from += len;
    LWIP_ASSERT("offset_to <= p_to->len", offset_to <= p_to->len);
    LWIP_ASSERT("offset_from <= p_from->len", offset_from <= p_from->len);
    
    /* advance p_from/p_to as needed */
    if (offset_from >= p_from->len) {
      offset_from = 0;
      p_from = p_from->next;
      pbuf_sanity_next(sanity_from, p_from);
    }
    if (offset_to >= p_to->len) {
      offset_to = 0;
      p_to = p_to->next;
      pbuf_sanity_next(sanity_to, p_to);
      LWIP_ERROR("p_to != NULL", (p_from == NULL) || (p_to != NULL), return ERR_ARG;);
    }
  } while (p_from);
  
  LWIP_DEBUGF(PBUF_DEBUG | LWIP_DBG_TRACE, ("pbuf_copy: end of chain reached.\n"));
  
  return ERR_OK;
}

/**
 * Copy (part of) the contents of a packet buffer
 * to an application supplied buffer.
 *
 * @param buf the pbuf from which to copy data
 * @param dataptr the application supplied buffer
 * @param len length of data to copy (dataptr must be big enough). No more 
 * than buf->tot_len will be copied, irrespective of len
 * @param offset offset into the packet buffer from where to begin copying len bytes
 * @return the number of bytes copied, or 0 on failure
 */
u16_t
pbuf_copy_partial(struct pbuf *buf, void *dataptr, u16_t len, u16_t offset)
{
  struct pbuf *p;
  u16_t pos = 0;
  u16_t buf_copy_len;
  pbuf_sanity_decl(sanity)

  LWIP_ERROR("pbuf_copy_partial: invalid buf", (buf != NULL), return 0;);
  LWIP_ERROR("pbuf_copy_partial: invalid dataptr", (dataptr != NULL), return 0;);

  pbuf_sanity_start(sanity, buf);
  
  for (p = buf; len != 0 && p != NULL; p = p->next) {
    pbuf_sanity_visit(sanity, p);
    
    if (offset >= p->len) {
      /* don't copy from this buffer -> on to the next */
      offset -= p->len;
    } else {
      /* copy from this buffer. maybe only partially. */
      buf_copy_len = LWIP_MIN(len, p->len - offset);
      /* copy the necessary parts of the buffer */
      MEMCPY(&((char*)dataptr)[pos], &((char*)p->payload)[offset], buf_copy_len);
      pos += buf_copy_len;
      len -= buf_copy_len;
      offset = 0;
    }
  }
  
  pbuf_sanity_end(sanity, p);
  
  return pos;
}

/**
 * Skip a number of bytes at the start of a pbuf.
 * 
 * This will return non-null if and only if the chain has
 * MORE than in_offset bytes. In that case it is safe to
 * access return[out_offset].
 *
 * @param p input pbuf
 * @param in_offset offset to skip
 * @param out_offset resulting offset in the returned pbuf
 * @return the pbuf in the chain where the offset is
 */
static struct pbuf*
pbuf_skip(struct pbuf* p, u16_t in_offset, u16_t* out_offset)
{
  pbuf_sanity_decl(sanity)
  
  LWIP_ERROR("pbuf_skip: p != NULL", p != NULL, return NULL;);

  /* get the correct pbuf */
  pbuf_sanity_start_visit(sanity, p);
  while ((p != NULL) && (p->len <= in_offset)) {
    in_offset -= p->len;
    p = p->next;
    pbuf_sanity_next(sanity, p);
  }
  
  *out_offset = in_offset;
  return p;
}

/**
 * Copy application supplied data into a pbuf.
 * This function can only be used to copy the equivalent of buf->tot_len data.
 *
 * @param buf pbuf to fill with data
 * @param dataptr application supplied data buffer
 * @param len length of the application supplied data buffer
 *
 * @return ERR_OK if successful, ERR_MEM if the pbuf is not big enough
 */
err_t
pbuf_take(struct pbuf *buf, const void *dataptr, u16_t len)
{
  struct pbuf *p;
  u16_t buf_copy_len;
  u16_t total_copy_len = len;
  u16_t copied_total = 0;
  pbuf_sanity_decl(sanity)

  LWIP_ERROR("pbuf_take: invalid buf", (buf != NULL), return ERR_ARG;);
  LWIP_ERROR("pbuf_take: invalid dataptr", (dataptr != NULL), return ERR_ARG;);
  LWIP_ERROR("pbuf_take: buf not large enough", (buf->tot_len >= len), return ERR_ARG;);

  pbuf_sanity_start(sanity, buf);
  
  for (p = buf; total_copy_len != 0; p = p->next) {
    LWIP_ASSERT("pbuf_take: invalid pbuf", p != NULL);
    pbuf_sanity_visit(sanity, p);
    
    buf_copy_len = LWIP_MIN(total_copy_len, p->len);
    MEMCPY(p->payload, &((const char*)dataptr)[copied_total], buf_copy_len);
    total_copy_len -= buf_copy_len;
    copied_total += buf_copy_len;
  }
  
  pbuf_sanity_end(sanity, p);
  
  LWIP_ASSERT("did not copy all data", total_copy_len == 0 && copied_total == len);
  
  return ERR_OK;
}

/**
 * Same as pbuf_take() but puts data at an offset
 *
 * @param buf pbuf to fill with data (not NULL)
 * @param dataptr application supplied data buffer
 * @param len length of the application supplied data buffer
 * @param offset offset in pbuf where to copy dataptr to
 *
 * @return ERR_OK if successful, ERR_MEM if the pbuf is not big enough
 */
err_t
pbuf_take_at(struct pbuf *buf, const void *dataptr, u16_t len, u16_t offset)
{
  u16_t target_offset;
  struct pbuf* q = pbuf_skip(buf, offset, &target_offset);

  /* check if desired range is valid */
  if (q == NULL || q->tot_len < target_offset + len) {
    return (target_offset == 0 && len == 0) ? ERR_OK : ERR_MEM;
  }
  
  /* copy the part that goes into the first pbuf */
  u16_t first_copy_len = LWIP_MIN(len, q->len - target_offset);
  MEMCPY((u8_t*)q->payload + target_offset, dataptr, first_copy_len);
  
  /* copy to remainder of chain, if any */
  if (len > first_copy_len) {
    return pbuf_take(q->next, (const u8_t*)dataptr + first_copy_len, len - first_copy_len);
  } else {
    return ERR_OK;
  }
}

 /** Get one byte from the specified position in a pbuf
 * WARNING: returns zero for offset >= p->tot_len
 *
 * @param p pbuf to parse (not NULL)
 * @param offset offset into p of the byte to return
 * @return byte at an offset into p OR ZERO IF 'offset' >= p->tot_len
 */
u8_t
pbuf_get_at(struct pbuf* p, u16_t offset)
{
  u16_t q_idx;
  struct pbuf* q = pbuf_skip(p, offset, &q_idx);

  if (q != NULL) {
    return ((u8_t*)q->payload)[q_idx];
  }
  return 0;
}

 /** Put one byte to the specified position in a pbuf
 * WARNING: silently ignores offset >= p->tot_len
 *
 * @param p pbuf to fill (not NULL)
 * @param offset offset into p of the byte to write
 * @param data byte to write at an offset into p
 */
void
pbuf_put_at(struct pbuf* p, u16_t offset, u8_t data)
{
  u16_t q_idx;
  struct pbuf* q = pbuf_skip(p, offset, &q_idx);

  if (q != NULL) {
    ((u8_t*)q->payload)[q_idx] = data;
  }
}

/**
 * Returns the last pbuf in a chain.
 * 
 * @param p pbuf chain (not NULL)
 * @return the last pbuf in the chain
 */
struct pbuf* pbuf_last(struct pbuf* p)
{
  while (p->next != NULL) {
    p = p->next;
  }
  return p;
}
