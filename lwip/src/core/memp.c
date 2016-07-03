/**
 * @file
 * Dynamic pool memory manager
 *
 * lwIP has dedicated pools for many structures (netconn, protocol control blocks,
 * packet buffers, ...). All these pools are managed here.
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
#include "lwip/memp.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"
#include "lwip/raw.h"
#include "lwip/tcp_impl.h"
#include "lwip/igmp.h"
#include "lwip/sys.h"
#include "lwip/timers.h"
#include "lwip/stats.h"
#include "netif/etharp.h"
#include "lwip/ip4_frag.h"
#include "lwip/dns.h"
#include "lwip/nd6.h"
#include "lwip/ip6_frag.h"
#include "lwip/mld6.h"

#include <string.h>
#include <stddef.h>

struct memp {
  struct memp *next;
#if MEMP_OVERFLOW_CHECK
  const char *file;
  int line;
#endif /* MEMP_OVERFLOW_CHECK */
};

/* if MEMP_OVERFLOW_CHECK is turned on, we reserve some bytes at the beginning
 * and at the end of each element, initialize them as 0xcd and check
 * them later. */
/* If MEMP_OVERFLOW_CHECK is >= 2, on every call to memp_malloc or memp_free,
 * every single element in each pool is checked!
 * This is VERY SLOW but also very helpful. */
/* MEMP_SANITY_REGION_BEFORE and MEMP_SANITY_REGION_AFTER can be overridden in
 * lwipopts.h to change the amount reserved for checking. */

#if MEMP_OVERFLOW_CHECK
#define MEMP_FULL_ELEM(elem_type) \
struct { \
    struct memp memp; \
    u8_t underflow[MEMP_UNDERFLOW_REGION]; \
    elem_type elem; \
    u8_t overflow[MEMP_OVERFLOW_REGION]; \
}
#else
#define MEMP_FULL_ELEM(elem_type) \
union { \
    struct memp memp; \
    elem_type elem; \
}
#endif

// Helper macro used from memp_std.h when the element is an array.
#define MEMP_ARRAY_STRUCT(type, count) struct { type data[(count)]; }

/** This array holds the first free element of each pool.
 *  Elements form a linked list. */
static struct memp *memp_tab[MEMP_MAX];

/** This array holds the full element sizes of each pool. */
static const u16_t memp_fullsize[MEMP_MAX] = {
#define LWIP_MEMPOOL(name,num,elem_type,desc) sizeof(MEMP_FULL_ELEM(elem_type)),
#include "lwip/memp_std.h"
};

#if MEMP_OVERFLOW_CHECK

/** This array holds the underflow region offsets of each pool. */
static const u16_t memp_underflow_offset[MEMP_MAX] = {
#define LWIP_MEMPOOL(name,num,elem_type,desc) offsetof(MEMP_FULL_ELEM(elem_type), underflow),
#include "lwip/memp_std.h"
};

/** This array holds the element offsets of each pool. */
static const u16_t memp_elem_offset[MEMP_MAX] = {
#define LWIP_MEMPOOL(name,num,elem_type,desc) offsetof(MEMP_FULL_ELEM(elem_type), elem),
#include "lwip/memp_std.h"
};

/** This array holds the overflow region offsets of each pool. */
static const u16_t memp_overflow_offset[MEMP_MAX] = {
#define LWIP_MEMPOOL(name,num,elem_type,desc) offsetof(MEMP_FULL_ELEM(elem_type), overflow),
#include "lwip/memp_std.h"
};

#endif

/** This array holds the number of elements in each pool. */
static const u16_t memp_num[MEMP_MAX] = {
#define LWIP_MEMPOOL(name,num,elem_type,desc)  (num),
#include "lwip/memp_std.h"
};

/** This array holds a textual description of each pool. */
#ifdef LWIP_DEBUG
static const char *const memp_desc[MEMP_MAX] = {
#define LWIP_MEMPOOL(name,num,elem_type,desc)  (desc),
#include "lwip/memp_std.h"
};
#endif /* LWIP_DEBUG */

/** This creates each memory pool. These are named memp_memory_XXX_base (where
 * XXX is the name of the pool defined in memp_std.h).
 */
#define LWIP_MEMPOOL(name,num,elem_type,desc) MEMP_FULL_ELEM(elem_type) memp_memory_ ## name ## _base [(num)];
#include "lwip/memp_std.h"

/** This array holds the base of each memory pool. */
static void *const memp_bases[] = {
#define LWIP_MEMPOOL(name,num,elem_type,desc) memp_memory_ ## name ## _base,
#include "lwip/memp_std.h"
};

/** Gets the next memp in the pool, used for iterating though all elements of a pool. */
#define memp_next_in_array(memp_ptr, type) (struct memp *)((u8_t *)(memp_ptr) + memp_fullsize[(type)]);

/* Offset from memp pointer to data for the element.
 * For !MEMP_OVERFLOW_CHECK it's zero because MEMP_FULL_ELEM is a union. */
#if MEMP_OVERFLOW_CHECK
#define memp_elem_offset(type) memp_elem_offset[(type)]
#else
#define memp_elem_offset(type) 0
#endif

#if MEMP_SANITY_CHECK
/**
 * Check that memp-lists don't form a circle, using "Floyd's cycle-finding algorithm".
 */
static int
memp_sanity(void)
{
  s16_t i;
  struct memp *t, *h;

  for (i = 0; i < MEMP_MAX; i++) {
    t = memp_tab[i];
    if (t != NULL) {
      for (h = t->next; (t != NULL) && (h != NULL); t = t->next,
        h = ((h->next != NULL) ? h->next->next : NULL)) {
        if (t == h) {
          return 0;
        }
      }
    }
  }
  return 1;
}
#endif /* MEMP_SANITY_CHECK*/

#if MEMP_OVERFLOW_CHECK

/**
 * Check if a memp element was victim of an underflow
 * (e.g. the restricted area before it has been altered)
 *
 * @param p the memp element to check
 * @param memp_type the pool p comes from
 */
static void
memp_overflow_check_element_underflow(struct memp *p, u16_t memp_type)
{
  u16_t k;
  u8_t *m;
  m = (u8_t*)p + memp_underflow_offset[memp_type];
  for (k = 0; k < MEMP_UNDERFLOW_REGION; k++) {
    LWIP_ASSERT("detected memp underflow", m[k] == 0xcd);
  }
}

/**
 * Check if a memp element was victim of an overflow
 * (e.g. the restricted area after it has been altered)
 *
 * @param p the memp element to check
 * @param memp_type the pool p comes from
 */
static void
memp_overflow_check_element_overflow(struct memp *p, u16_t memp_type)
{
  u16_t k;
  u8_t *m;
  m = (u8_t*)p + memp_overflow_offset[memp_type];
  for (k = 0; k < MEMP_OVERFLOW_REGION; k++) {
    LWIP_ASSERT("detected memp overflow", m[k] == 0xcd);
  }
}

/**
 * Do an underflow and overflow check for all elements in every pool.
 */
static void
memp_overflow_check_all(void)
{
  u16_t i, j;
  struct memp *p;

  for (i = 0; i < MEMP_MAX; ++i) {
    p = (struct memp *)(memp_bases[i]);
    for (j = 0; j < memp_num[i]; ++j) {
      memp_overflow_check_element_underflow(p, i);
      memp_overflow_check_element_overflow(p, i);
      p = memp_next_in_array(p, i);
    }
  }
}

/**
 * Initialize the restricted areas of all memp elements in every pool.
 */
static void
memp_overflow_init(void)
{
  u16_t i, j;
  struct memp *p;

  for (i = 0; i < MEMP_MAX; ++i) {
    p = (struct memp *)(memp_bases[i]);
    for (j = 0; j < memp_num[i]; ++j) {
      memset((u8_t*)p + memp_underflow_offset[i], 0xcd, MEMP_UNDERFLOW_REGION);
      memset((u8_t*)p + memp_overflow_offset[i],  0xcd, MEMP_OVERFLOW_REGION);
      p = memp_next_in_array(p, i);
    }
  }
}

#endif /* MEMP_OVERFLOW_CHECK */

#ifdef LWIP_NOASSERT
#define memp_ptr_sanity(p, memp_type)
#else
static void
memp_ptr_sanity(struct memp *p, memp_t memp_type)
{
    mem_ptr_t p_mem = (mem_ptr_t)p;
    mem_ptr_t pool_start = (mem_ptr_t)memp_bases[memp_type];
    LWIP_ASSERT("p_mem>=pool_start", p_mem >= pool_start);
    LWIP_ASSERT("p_mem<pool_end",    p_mem <  pool_start + ((size_t)memp_num[memp_type] * memp_fullsize[memp_type]));
    LWIP_ASSERT("(p_mem-pool_start)%num==0", (p_mem - pool_start) % memp_fullsize[memp_type] == 0);
}
#endif

/**
 * Initialize this module.
 *
 * Carves out memp_memory into linked lists for each pool-type.
 */
void
memp_init(void)
{
  struct memp *p;
  u16_t i, j;

  for (i = 0; i < MEMP_MAX; ++i) {
    MEMP_STATS_AVAIL(used, i, 0);
    MEMP_STATS_AVAIL(max, i, 0);
    MEMP_STATS_AVAIL(err, i, 0);
    MEMP_STATS_AVAIL(avail, i, memp_num[i]);
  }

  /* for every pool: */
  for (i = 0; i < MEMP_MAX; ++i) {
    memp_tab[i] = NULL;
    p = (struct memp *)(memp_bases[i]);
    /* create a linked list of memp elements */
    for (j = 0; j < memp_num[i]; ++j) {
      p->next = memp_tab[i];
      memp_tab[i] = p;
      p = memp_next_in_array(p, i);
    }
  }
  
#if MEMP_OVERFLOW_CHECK
  memp_overflow_init();
  /* check everything a first time to see if it worked */
  memp_overflow_check_all();
#endif
}

/**
 * Get an element from a specific pool.
 *
 * @param type the pool to get an element from
 *
 * the debug version has two more parameters:
 * @param file file name calling this function
 * @param line number of line where this function is called
 *
 * @return a pointer to the allocated memory or a NULL pointer on error
 */
void *
#if !MEMP_OVERFLOW_CHECK
memp_malloc(memp_t type)
#else
memp_malloc_fn(memp_t type, const char* file, int line)
#endif
{
  void *res;
  SYS_ARCH_DECL_PROTECT(old_level);

  LWIP_ERROR("memp_malloc: type < MEMP_MAX", (type < MEMP_MAX), return NULL;);

  SYS_ARCH_PROTECT(old_level);
  
#if MEMP_OVERFLOW_CHECK >= 2
  memp_overflow_check_all();
#endif /* MEMP_OVERFLOW_CHECK >= 2 */

  if (memp_tab[type] != NULL) {
    struct memp *memp = memp_tab[type];
    memp_ptr_sanity(memp, type);
    memp_tab[type] = memp->next;
#if MEMP_OVERFLOW_CHECK
    memp->next = NULL;
    memp->file = file;
    memp->line = line;
#endif /* MEMP_OVERFLOW_CHECK */
    MEMP_STATS_INC_USED(used, type);
    res = (u8_t *)memp + memp_elem_offset(type);
  } else {
    LWIP_DEBUGF(MEMP_DEBUG | LWIP_DBG_LEVEL_SERIOUS, ("memp_malloc: out of memory in pool %s\n", memp_desc[type]));
    MEMP_STATS_INC(err, type);
    res = NULL;
  }

  SYS_ARCH_UNPROTECT(old_level);

  return res;
}

/**
 * Put an element back into its pool.
 *
 * @param type the pool where to put mem
 * @param mem the memp element to free
 */
void
memp_free(memp_t type, void *mem)
{
  struct memp *memp;
  SYS_ARCH_DECL_PROTECT(old_level);

  if (mem == NULL) {
    return;
  }

  memp = (struct memp *)((u8_t *)mem - memp_elem_offset(type));
  memp_ptr_sanity(memp, type);

  SYS_ARCH_PROTECT(old_level);
  
#if MEMP_OVERFLOW_CHECK
#if MEMP_OVERFLOW_CHECK >= 2
  memp_overflow_check_all();
#else
  memp_overflow_check_element_overflow(memp, type);
  memp_overflow_check_element_underflow(memp, type);
#endif /* MEMP_OVERFLOW_CHECK >= 2 */
#endif /* MEMP_OVERFLOW_CHECK */

  MEMP_STATS_DEC(used, type);

  memp->next = memp_tab[type];
  memp_tab[type] = memp;

#if MEMP_SANITY_CHECK
  LWIP_ASSERT("memp sanity", memp_sanity());
#endif /* MEMP_SANITY_CHECK */

  SYS_ARCH_UNPROTECT(old_level);
}
