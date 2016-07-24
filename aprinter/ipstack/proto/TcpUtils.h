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

#ifndef APRINTER_IPSTACK_TCP_UTILS_H
#define APRINTER_IPSTACK_TCP_UTILS_H

#include <stdint.h>
#include <stddef.h>

#include <aprinter/ipstack/proto/Tcp4Proto.h>

#include <aprinter/BeginNamespace.h>

namespace TcpUtils
{
    using FlagsType = uint16_t;
    using SeqType = uint32_t;
    
    enum class TcpState : uint8_t {
        CLOSED,
        SYN_SENT,
        SYN_RCVD,
        ESTABLISHED,
        CLOSE_WAIT,
        LAST_ACK,
        FIN_WAIT_1,
        FIN_WAIT_2,
        CLOSING,
        TIME_WAIT
    };
    
    static inline SeqType seq_add (SeqType op1, SeqType op2)
    {
        return (SeqType)(op1 + op2);
    }
    
    static inline SeqType seq_diff (SeqType op1, SeqType op2)
    {
        return (SeqType)(op1 - op2);
    }
    
    static inline bool seq_lte (SeqType op1, SeqType op2, SeqType ref)
    {
        return (seq_diff(op1, ref) <= seq_diff(op2, ref));
    }
    
    static inline bool seq_lt (SeqType op1, SeqType op2, SeqType ref)
    {
        return (seq_diff(op1, ref) < seq_diff(op2, ref));
    }
}

#include <aprinter/EndNamespace.h>

#endif
