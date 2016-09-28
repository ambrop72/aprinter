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

#include <stdint.h>

#define FOLD_U32T(u) (((u) >> 16) + ((u) & 0x0000ffffUL))
#define SWAP_BYTES_IN_WORD(w) (((w) & 0xff) << 8) | (((w) & 0xff00) >> 8)

typedef uint8_t u8_t;
typedef uint16_t u16_t;
typedef uint32_t u32_t;
typedef uintptr_t mem_ptr_t;

/*
 * Curt McDowell
 * Broadcom Corp.
 * csm@broadcom.com
 *
 * IP checksum two bytes at a time with support for
 * unaligned buffer.
 * Works for len up to and including 0x20000.
 * by Curt McDowell, Broadcom Corp. 12/08/2005
 *
 * @param dataptr points to start of data to be summed at any boundary
 * @param len length of data to be summed
 * @return host order (!) lwip checksum (non-inverted Internet sum)
 */
u16_t
lwip_standard_chksum(const void *dataptr, int len)
{
  const u8_t *pb = (const u8_t *)dataptr;
  const u16_t *ps;
  u16_t t = 0;
  u32_t sum = 0;
  int odd = ((mem_ptr_t)pb & 1);

  /* Get aligned to u16_t */
  if (odd && len > 0) {
    ((u8_t *)&t)[1] = *pb++;
    len--;
  }

  /* Add the bulk of the data */
  ps = (const u16_t *)(const void *)pb;
  while (len > 1) {
    sum += *ps++;
    len -= 2;
  }

  /* Consume left-over byte, if any */
  if (len > 0) {
    ((u8_t *)&t)[0] = *(const u8_t *)ps;
  }

  /* Add end bytes */
  sum += t;

  /* Fold 32-bit sum to 16 bits
     calling this twice is probably faster than if statements... */
  sum = FOLD_U32T(sum);
  sum = FOLD_U32T(sum);

  /* Swap if alignment was odd */
  if (odd) {
    sum = SWAP_BYTES_IN_WORD(sum);
  }

  return (u16_t)sum;
}
