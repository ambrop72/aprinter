/*
 * Copyright (c) 2014 Ambroz Bizjak
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

#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>

#include <aprinter/meta/ConstexprCrc32.h>
#include <aprinter/meta/ConstexprHash.h>

using namespace APrinter;

constexpr char const crctest1[] = "\x00\x01\x02\x03";
constexpr char const crctest2[] = "\x04\x05\x06\x07";

int main ()
{
    using Hash = ConstexprHash<ConstexprCrc32>;
    
    static constexpr uint32_t CrcVal = Hash().addString(crctest1, sizeof(crctest1) - 1).addString(crctest2, sizeof(crctest2) - 1).end();
    printf("%" PRIx32 "\n", CrcVal);
    
    static constexpr uint32_t CrcVal1 = Hash().addUint32(UINT32_C(0x03020100)).addUint32(UINT32_C(0x07060504)).end();
    printf("%" PRIx32 "\n", CrcVal1);
}
