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

#include <limits>

#include <aprinter/meta/MinMax.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/BinaryTools.h>
#include <aprinter/base/OneOf.h>

#include <aipstack/misc/Buf.h>
#include <aipstack/proto/IpAddr.h>
#include <aipstack/proto/Tcp4Proto.h>

namespace AIpStack {

class TcpUtils {
    APRINTER_USE_ONEOF
    
public:
    using FlagsType = uint16_t;
    using SeqType = uint32_t;
    using PortType = uint16_t;
    
private:
    static uint8_t const Bit0 = 1 << 0;
    static uint8_t const Bit1 = 1 << 1;
    static uint8_t const Bit2 = 1 << 2;
    static uint8_t const Bit3 = 1 << 3;
    
public:
    /**
     * TCP states.
     * 
     * ATTENTION: The bit values are carefully crafted to allow
     * efficient implementation of the following state predicates:
     * - state_is_synsent_synrcvd,
     * - accepting_data_in_state,
     * - can_output_in_state,
     * - snd_open_in_state.
     * 
     * NOTE: the FIN_WAIT_2_TIME_WAIT state is not a standard TCP
     * state but is used transiently when we were in FIN_WAIT_2 and
     * have just received a FIN, but we will only go to TIME_WAIT
     * after calling callbacks.
     */
    enum TcpState : uint8_t {
        CLOSED               = 0   |Bit2|0   |Bit0,
        SYN_SENT             = Bit3|Bit2|0   |Bit0,
        SYN_RCVD             = Bit3|Bit2|0   |0   ,
        ESTABLISHED          = 0   |0   |0   |0   ,
        CLOSE_WAIT           = 0   |0   |0   |Bit0,
        LAST_ACK             = Bit3|0   |0   |0   ,
        FIN_WAIT_1           = 0   |0   |Bit1|0   ,
        FIN_WAIT_2           = 0   |Bit2|0   |0   ,
        FIN_WAIT_2_TIME_WAIT = Bit3|Bit2|Bit1|Bit0,
        CLOSING              = Bit3|0   |Bit1|Bit0,
        TIME_WAIT            = Bit3|Bit2|Bit1|0   ,
    };
    static int const TcpStateBits = 4;
    
    struct TcpOptions;
    
    // Container for data in the TCP header (used both in RX and TX).
    struct TcpSegMeta {
        PortType local_port;
        PortType remote_port;
        SeqType seq_num;
        SeqType ack_num;
        uint16_t window_size;
        FlagsType flags;
        TcpOptions *opts; // not used for RX (undefined), may be null for TX
    };
    
    // TCP options flags used in TcpOptions options field.
    struct OptionFlags { enum : uint8_t {
        MSS       = 1 << 0,
        WND_SCALE = 1 << 1,
    }; };
    
    // Container for TCP options that we care about.
    struct TcpOptions {
        uint8_t options;
        uint8_t wnd_scale;
        uint16_t mss;
    };
    
    static SeqType const SeqMSB = (SeqType)1 << 31;
    
    static inline SeqType seq_add (SeqType op1, SeqType op2)
    {
        return (SeqType)(op1 + op2);
    }
    
    static inline SeqType seq_diff (SeqType op1, SeqType op2)
    {
        return (SeqType)(op1 - op2);
    }
    
    static inline SeqType seq_add_sat (SeqType op1, SeqType op2)
    {
        SeqType sum = op1 + op2;
        if (sum < op2) {
            sum = std::numeric_limits<uint32_t>::max();
        }
        return sum;
    }
    
    static inline bool seq_lte (SeqType op1, SeqType op2, SeqType ref)
    {
        return (seq_diff(op1, ref) <= seq_diff(op2, ref));
    }
    
    static inline bool seq_lt (SeqType op1, SeqType op2, SeqType ref)
    {
        return (seq_diff(op1, ref) < seq_diff(op2, ref));
    }
    
    static inline bool seq_lt2 (SeqType op1, SeqType op2)
    {
        return seq_diff(op1, op2) >= SeqMSB;
    }
    
    static inline size_t tcplen (FlagsType flags, size_t tcp_data_len)
    {
        return tcp_data_len + ((flags & Tcp4SeqFlags) != 0);
    }
    
    static inline bool state_is_active (uint8_t state)
    {
        return state != OneOf(TcpState::CLOSED, TcpState::SYN_SENT,
                              TcpState::SYN_RCVD, TcpState::TIME_WAIT);
    }
    
    static inline bool state_is_synsent_synrcvd (uint8_t state)
    {
        //return state == OneOf(TcpState::SYN_SENT, TcpState::SYN_RCVD);
        return (state >> 1) == ((Bit3|Bit2) >> 1);
    }
    
    static inline bool accepting_data_in_state (uint8_t state)
    {
        //return state == OneOf(TcpState::ESTABLISHED, TcpState::FIN_WAIT_1,
        //                      TcpState::FIN_WAIT_2);
        return (state & (Bit3|Bit0)) == 0;
    }
    
    static inline bool can_output_in_state (uint8_t state)
    {
        //return state == OneOf(TcpState::ESTABLISHED, TcpState::FIN_WAIT_1,
        //                      TcpState::CLOSING, TcpState::CLOSE_WAIT,
        //                      TcpState::LAST_ACK);
        return (state & Bit2) == 0;
    }
    
    static inline bool snd_open_in_state (uint8_t state)
    {
        //return state == OneOf(TcpState::ESTABLISHED, TcpState::CLOSE_WAIT);
        return (state >> 1) == 0;
    }
    
    static inline void parse_options (IpBufRef buf, TcpOptions *out_opts)
    {
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
                    out_opts->mss = APrinter::ReadBinaryInt<uint16_t,
                                            APrinter::BinaryBigEndian>(opt_data);
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
            APrinter::WriteBinaryInt<uint8_t,  APrinter::BinaryBigEndian>(
                                                            TcpOptionMSS,       out + 0);
            APrinter::WriteBinaryInt<uint8_t,  APrinter::BinaryBigEndian>(
                                                            4,                  out + 1);
            APrinter::WriteBinaryInt<uint16_t, APrinter::BinaryBigEndian>(
                                                            tcp_opts.mss,       out + 2);
            out += OptWriteLenMSS;
        }
        if ((tcp_opts.options & OptionFlags::WND_SCALE) != 0) {
            APrinter::WriteBinaryInt<uint8_t,  APrinter::BinaryBigEndian>(
                                                            TcpOptionNop     ,  out + 0);
            APrinter::WriteBinaryInt<uint8_t,  APrinter::BinaryBigEndian>(
                                                            TcpOptionWndScale,  out + 1);
            APrinter::WriteBinaryInt<uint8_t,  APrinter::BinaryBigEndian>(
                                                            3,                  out + 2);
            APrinter::WriteBinaryInt<uint8_t,  APrinter::BinaryBigEndian>(
                                                            tcp_opts.wnd_scale, out + 3);
            out += OptWriteLenWndScale;
        }
    }
    
    template <uint16_t MinAllowedMss>
    static bool calc_snd_mss (uint16_t iface_mss,
                              TcpOptions const &tcp_opts, uint16_t *out_mss)
    {
        uint16_t req_mss = ((tcp_opts.options & OptionFlags::MSS) != 0) ?
            tcp_opts.mss : 536;
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
            return 2 * (SeqType)snd_mss;
        }
        else if (snd_mss > 1095) {
            return 3 * (SeqType)snd_mss;
        }
        else {
            return 4 * (SeqType)snd_mss;
        }
    }
    
    // Lookup key for TCP PCBs
    struct PcbKey : public Ip4Addrs {
        PcbKey () = default;
        
        inline PcbKey (Ip4Addr local_addr_, Ip4Addr remote_addr_,
                       PortType local_port_, PortType remote_port_)
        : Ip4Addrs{local_addr_, remote_addr_},
          local_port(local_port_), remote_port(remote_port_)
        {
        }
        
        PortType local_port;
        PortType remote_port;
    };
    
    // Provides comparison functions for PcbKey
    class PcbKeyCompare {
    public:
        static int CompareKeys (PcbKey const &op1, PcbKey const &op2)
        {
            // Compare in an order that would need least
            // comparisons with typical server usage.
            
            if (op1.remote_port < op2.remote_port) {
                return -1;
            }
            if (op1.remote_port > op2.remote_port) {
                return 1;
            }
            
            if (op1.remote_addr < op2.remote_addr) {
                return -1;
            }
            if (op1.remote_addr > op2.remote_addr) {
                return 1;
            }
            
            if (op1.local_port < op2.local_port) {
                return -1;
            }
            if (op1.local_port > op2.local_port) {
                return 1;
            }

            if (op1.local_addr < op2.local_addr) {
                return -1;
            }
            if (op1.local_addr > op2.local_addr) {
                return 1;
            }
            
            return 0;
        }
        
        static bool KeysAreEqual (PcbKey const &op1, PcbKey const &op2)
        {
            return op1.remote_port == op2.remote_port &&
                   op1.remote_addr == op2.remote_addr &&
                   op1.local_port  == op2.local_port  &&
                   op1.local_addr  == op2.local_addr;
        }
    };
    
    /**
     * Determine if x is in the half-open interval (start, start+length].
     * IntType must be an unsigned integer type.
     * 
     * Note that the interval is understood in terms of modular
     * arithmetic, so if a+b is not representable in this type
     * the result may not be what you expect.
     * 
     * Thanks to Simon Stienen for this most efficient formula.
     */
    template <typename IntType>
    inline static bool InOpenClosedIntervalStartLen (
                            IntType start, IntType length, IntType x)
    {
        static_assert(std::numeric_limits<IntType>::is_integer, "");
        static_assert(!std::numeric_limits<IntType>::is_signed, "");
        
        return (IntType)(x + ~start) < length;
    }
};

}

#endif
