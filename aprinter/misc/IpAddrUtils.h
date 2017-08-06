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

#ifndef APRINTER_IP_ADDR_UTILS_H
#define APRINTER_IP_ADDR_UTILS_H

#include <stdio.h>
#include <stdlib.h>

#include <aprinter/base/LoopUtils.h>

namespace APrinter {

struct IpAddrUtils
{
    inline static void FormatIp4Addr (char const *bytes, char *out_str)
    {
        auto *ub = reinterpret_cast<unsigned char const *>(bytes);
        
        ::sprintf(out_str, "%d.%d.%d.%d",
                  (int)ub[0], (int)ub[1], (int)ub[2], (int)ub[3]);
    }
    
    inline static bool ParseIp4Addr (char const *str, char *out_bytes)
    {
        auto *ub = reinterpret_cast<unsigned char *>(out_bytes);
        
        for (auto i : LoopRange<size_t>(4)) {
            if (*str == '\0') {
                return false;
            }
            
            char *end;
            long int val = ::strtol(str, &end, 10);
            
            if (!(*end == '.' || *end == '\0')) {
                return false;
            }
            
            if (!(val >= 0 && val <= 255)) {
                return false;
            }
            
            ub[i] = val;
            
            str = (*end == '\0') ? end : (end + 1);
        }
        
        if (*str != '\0') {
            return false;
        }
        
        return true;
    }
};

}

#endif
