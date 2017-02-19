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

#include <aprinter/meta/MinMax.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/BinaryTools.h>
#include <aprinter/base/OneOf.h>
#include <aipstack/misc/Buf.h>
#include <aipstack/proto/Tcp4Proto.h>

#include <aipstack/BeginNamespace.h>

class TcpUtils {
    APRINTER_USE_ONEOF
    
public:
    using FlagsType = uint16_t;
    using SeqType = uint32_t;
    using PortType = uint16_t;
    
    // TCP states.
    // Note, FIN_WAIT_2_TIME_WAIT is used transiently when we
    // were in FIN_WAIT_2 and have just received a FIN, but we
    // we will only go to TIME_WAIT after calling callbacks.
    enum class TcpState : uint8_t {
        CLOSED,
        SYN_SENT,
        SYN_RCVD,
        ESTABLISHED,
        CLOSE_WAIT,
        LAST_ACK,
        FIN_WAIT_1,
        FIN_WAIT_2,
        FIN_WAIT_2_TIME_WAIT,
        CLOSING,
        TIME_WAIT
    };
    
    struct TcpOptions;
    
    // Container for data in the TCP header (used both in RX and TX).
    struct TcpSegMeta {
        PortType local_port;
        PortType remote_port;
        SeqType seq_num;
        SeqType ack_num;
        uint16_t window_size;
        FlagsType flags;
        TcpOptions const *opts; // may be null for TX
    };
    
    // TCP options flags used in TcpOptions options field.
    struct OptionFlags { enum : uint8_t {
        MSS       = 1 << 0,
        WND_SCALE = 1 << 1,
    }; };
    
    // Container for TCP options that we care about.
    struct TcpOptions {
        uint8_t options;
        uint16_t mss;
        uint8_t wnd_scale;
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
    
    static inline size_t tcplen (FlagsType flags, size_t tcp_data_len)
    {
        return tcp_data_len + ((flags & Tcp4SeqFlags) != 0);
    }
    
    static inline bool state_is_active (TcpState state)
    {
        return state != OneOf(TcpState::CLOSED, TcpState::SYN_SENT,
                              TcpState::SYN_RCVD, TcpState::TIME_WAIT);
    }
    
    static inline bool accepting_data_in_state (TcpState state)
    {
        return state == OneOf(TcpState::ESTABLISHED, TcpState::FIN_WAIT_1,
                              TcpState::FIN_WAIT_2);
    }
    
    static inline bool can_output_in_state (TcpState state)
    {
        return state == OneOf(TcpState::ESTABLISHED, TcpState::FIN_WAIT_1,
                              TcpState::CLOSING, TcpState::CLOSE_WAIT,
                              TcpState::LAST_ACK);
    }
    
    static inline bool snd_open_in_state (TcpState state)
    {
        return state == OneOf(TcpState::ESTABLISHED, TcpState::CLOSE_WAIT);
    }
    
    static inline void parse_options (IpBufRef buf, uint8_t opts_len, TcpOptions *out_opts, IpBufRef *out_data)
    {
        AMBRO_ASSERT(opts_len <= buf.tot_len)
        
        // Truncate the buffer temporarily while we're parsing the options.
        size_t data_len = buf.tot_len - opts_len;
        buf.tot_len = opts_len;
        
        // Clear options flags. Below we will set flags for options that we find.
        out_opts->options = 0;
        
        while (buf.tot_len > 0) {
            // Read the option kind.
            uint8_t kind = buf.takeByte();
            
            // Hanlde end option and nop option.
            if (kind == TcpOptionEnd) {
                break;
            }
            else if (kind == TcpOptionNop) {
                continue;
            }
            
            // Read the option length.
            if (buf.tot_len == 0) {
                break;
            }
            uint8_t length = buf.takeByte();
            
            // Check the option length.
            if (length < 2) {
                break;
            }
            uint8_t opt_data_len = length - 2;
            if (buf.tot_len < opt_data_len) {
                break;
            }
            
            // Handle different options, consume option data.
            switch (kind) {
                // Maximum Segment Size
                case TcpOptionMSS: {
                    if (opt_data_len != 2) {
                        goto skip_option;
                    }
                    char opt_data[2];
                    buf.takeBytes(opt_data_len, opt_data);
                    out_opts->options |= OptionFlags::MSS;
                    out_opts->mss = APrinter::ReadBinaryInt<uint16_t, APrinter::BinaryBigEndian>(opt_data);
                } break;
                
                // Window Scale
                case TcpOptionWndScale: {
                    if (opt_data_len != 1) {
                        goto skip_option;
                    }
                    uint8_t value = buf.takeByte();
                    out_opts->options |= OptionFlags::WND_SCALE;
                    out_opts->wnd_scale = value;
                } break;
                
                // Unknown option (also used to handle bad options).
                skip_option:
                default: {
                    buf.skipBytes(opt_data_len);
                } break;
            }
        }
        
        // Skip any remaining option data (we might not have consumed all of it).
        buf.skipBytes(buf.tot_len);
        
        // The buf now points to the start of segment data but is empty.
        // Extend it to reference the entire segment data and return it to the caller.
        buf.tot_len = data_len;
        *out_data = buf;
    }
    
    static size_t const OptWriteLenMSS = 4;
    static size_t const OptWriteLenWndScale = 4;
    
    static size_t const MaxOptionsWriteLen = OptWriteLenMSS + OptWriteLenWndScale;
    
    static inline uint8_t calc_options_len (TcpOptions const &tcp_opts)
    {
        uint8_t opts_len = 0;
        if ((tcp_opts.options & OptionFlags::MSS) != 0) {
            opts_len += OptWriteLenMSS;
        }
        if ((tcp_opts.options & OptionFlags::WND_SCALE) != 0) {
            opts_len += OptWriteLenWndScale;
        }
        AMBRO_ASSERT(opts_len <= MaxOptionsWriteLen)
        AMBRO_ASSERT(opts_len % 4 == 0) // caller needs padding to 4-byte alignment
        return opts_len;
    }
    
    static inline void write_options (TcpOptions const &tcp_opts, char *out)
    {
        if ((tcp_opts.options & OptionFlags::MSS) != 0) {
            APrinter::WriteBinaryInt<uint8_t,  APrinter::BinaryBigEndian>(TcpOptionMSS,       out + 0);
            APrinter::WriteBinaryInt<uint8_t,  APrinter::BinaryBigEndian>(4,                  out + 1);
            APrinter::WriteBinaryInt<uint16_t, APrinter::BinaryBigEndian>(tcp_opts.mss,       out + 2);
            out += OptWriteLenMSS;
        }
        if ((tcp_opts.options & OptionFlags::WND_SCALE) != 0) {
            APrinter::WriteBinaryInt<uint8_t,  APrinter::BinaryBigEndian>(TcpOptionNop     ,  out + 0);
            APrinter::WriteBinaryInt<uint8_t,  APrinter::BinaryBigEndian>(TcpOptionWndScale,  out + 1);
            APrinter::WriteBinaryInt<uint8_t,  APrinter::BinaryBigEndian>(3,                  out + 2);
            APrinter::WriteBinaryInt<uint8_t,  APrinter::BinaryBigEndian>(tcp_opts.wnd_scale, out + 3);
            out += OptWriteLenWndScale;
        }
    }
    
    template <uint16_t MinAllowedMss>
    static bool calc_snd_mss (uint16_t iface_mss, TcpOptions const &tcp_opts, uint16_t *out_mss)
    {
        uint16_t req_mss = ((tcp_opts.options & OptionFlags::MSS) != 0) ? tcp_opts.mss : 536;
        uint16_t mss = APrinter::MinValue(iface_mss, req_mss);
        if (mss < MinAllowedMss) {
            return false;
        }
        *out_mss = mss;
        return true;
    }
    
    static SeqType calc_initial_cwnd (uint16_t snd_mss)
    {
        if (snd_mss > 2190) {
            return (snd_mss > UINT32_MAX / 2) ? UINT32_MAX : (2 * snd_mss);
        }
        else if (snd_mss > 1095) {
            return 3 * snd_mss;
        }
        else {
            return 4 * snd_mss;
        }
    }
};

#include <aipstack/EndNamespace.h>

#endif
