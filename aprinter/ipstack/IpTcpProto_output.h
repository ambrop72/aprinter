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

#ifndef APRINTER_IPSTACK_IP_TCP_PROTO_OUTPUT_H
#define APRINTER_IPSTACK_IP_TCP_PROTO_OUTPUT_H

#include <stdint.h>
#include <stddef.h>

#include <aprinter/meta/MinMax.h>
#include <aprinter/base/Preprocessor.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Hints.h>
#include <aprinter/ipstack/Buf.h>
#include <aprinter/ipstack/Chksum.h>
#include <aprinter/ipstack/proto/Tcp4Proto.h>
#include <aprinter/ipstack/proto/TcpUtils.h>

#include <aprinter/BeginNamespace.h>

template <typename TcpProto>
class IpTcpProto_output
{
    APRINTER_USE_TYPES2(TcpUtils, (FlagsType, SeqType, PortType, TcpState, TcpSegMeta,
                                   OptionFlags, TcpOptions))
    APRINTER_USE_VALS(TcpUtils, (seq_add, seq_diff, seq_lte, seq_lt, tcplen,
                                 can_output_in_state, accepting_data_in_state,
                                 snd_open_in_state))
    APRINTER_USE_TYPES1(TcpProto, (Context, Ip4DgramMeta, TcpPcb, PcbFlags, BufAllocator,
                                   Input, Clock, TimeType, RttType, RttNextType))
    APRINTER_USE_VALS(TcpProto::TheIpStack, (HeaderBeforeIp4Dgram, RttTypeMax))
    
public:
    // Check if our FIN has been ACKed.
    static bool pcb_fin_acked (TcpPcb *pcb)
    {
        return pcb->hasFlag(PcbFlags::FIN_SENT) && pcb->snd_una == pcb->snd_nxt;
    }
    
    // Calculate the offset of the current send buffer position.
    static size_t pcb_snd_offset (TcpPcb *pcb)
    {
        AMBRO_ASSERT(pcb->snd_buf_cur.tot_len <= pcb->snd_buf.tot_len)
        
        return pcb->snd_buf.tot_len - pcb->snd_buf_cur.tot_len;
    }
    
    // Send SYN+ACK packet in SYN_RCVD state, with MSS option.
    static void pcb_send_syn_ack (TcpProto *tcp, TcpPcb *pcb)
    {
        AMBRO_ASSERT(pcb->state == TcpState::SYN_RCVD)
        
        TcpOptions tcp_opts;
        tcp_opts.options = OptionFlags::MSS;
        tcp_opts.mss = pcb->rcv_mss;
        TcpSegMeta tcp_meta = {pcb->local_port, pcb->remote_port, pcb->snd_una, pcb->rcv_nxt,
                               Input::pcb_ann_wnd(pcb), Tcp4FlagSyn|Tcp4FlagAck, &tcp_opts};
        send_tcp(tcp, pcb->local_addr, pcb->remote_addr, tcp_meta);
    }
    
    // Send an empty ACK (which may be a window update).
    static void pcb_send_empty_ack (TcpProto *tcp, TcpPcb *pcb)
    {
        TcpSegMeta tcp_meta = {pcb->local_port, pcb->remote_port, pcb->snd_nxt, pcb->rcv_nxt,
                               Input::pcb_ann_wnd(pcb), Tcp4FlagAck};
        send_tcp(tcp, pcb->local_addr, pcb->remote_addr, tcp_meta);
    }
    
    // Send an RST for this PCB.
    static void pcb_send_rst (TcpProto *tcp, TcpPcb *pcb)
    {
        send_rst(tcp, pcb->local_addr, pcb->remote_addr,
                 pcb->local_port, pcb->remote_port,
                 pcb->snd_nxt, true, pcb->rcv_nxt);
    }
    
    static void pcb_need_ack (TcpProto *tcp, TcpPcb *pcb)
    {
        AMBRO_ASSERT(pcb->state != TcpState::CLOSED)
        
        // If we're in input processing just set a flag that ACK is
        // needed which will be picked up at the end, otherwise send
        // an ACK ourselves.
        if (tcp->m_current_pcb == pcb) {
            pcb->setFlag(PcbFlags::ACK_PENDING);
        } else {
            pcb_send_empty_ack(tcp, pcb);
        }
    }
    
    static void pcb_snd_buf_extended (TcpPcb *pcb)
    {
        AMBRO_ASSERT(snd_open_in_state(pcb->state))
        
        // Start the output timer if not running.
        if (!pcb->output_timer.isSet(Context())) {
            pcb->output_timer.appendAfter(Context(), TcpProto::OutputTimerTicks);
        }
    }
    
    static void pcb_end_sending (TcpProto *tcp, TcpPcb *pcb)
    {
        AMBRO_ASSERT(snd_open_in_state(pcb->state))
        
        // Make the appropriate state transition, effectively
        // queuing a FIN for sending.
        if (pcb->state == TcpState::ESTABLISHED) {
            pcb->state = TcpState::FIN_WAIT_1;
        } else {
            AMBRO_ASSERT(pcb->state == TcpState::CLOSE_WAIT)
            pcb->state = TcpState::LAST_ACK;
        }
        
        // Queue a FIN for sending.
        pcb->setFlag(PcbFlags::FIN_PENDING);
        
        // Push output.
        pcb_push_output(tcp, pcb);
    }
    
    static void pcb_push_output (TcpProto *tcp, TcpPcb *pcb)
    {
        AMBRO_ASSERT(can_output_in_state(pcb->state))
        
        // Ignore this if it we're in the connectionAborted callback.
        if (pcb->hasFlag(PcbFlags::ABORTING)) {
            return;
        }
        
        // Set the push index to the end of the send buffer.
        pcb->snd_psh_index = pcb->snd_buf.tot_len;
        
        // Schedule a call to pcb_output soon.
        if (pcb == tcp->m_current_pcb) {
            pcb->setFlag(PcbFlags::OUT_PENDING);
        } else {
            if (!pcb->output_timer.isSet(Context())) {
                pcb->output_timer.appendAfter(Context(), TcpProto::OutputTimerTicks);
            }
        }
    }
    
    // Check if there is any unacknowledged or unsent data or FIN.
    static bool pcb_has_snd_outstanding (TcpPcb *pcb)
    {
        AMBRO_ASSERT(can_output_in_state(pcb->state))
        
        return pcb->snd_buf.tot_len > 0 || !snd_open_in_state(pcb->state);
    }
    
    // Enqueue any data or FIN to be sent even if already sent.
    static void pcb_requeue_snd (TcpPcb *pcb)
    {
        AMBRO_ASSERT(can_output_in_state(pcb->state))
        
        // Enqueue all data.
        pcb->snd_buf_cur = pcb->snd_buf;
        
        // Enqueue a FIN if sending is closed.
        if (!snd_open_in_state(pcb->state)) {
            pcb->setFlag(PcbFlags::FIN_PENDING);
        }
    }
    
    static SeqType pcb_output_segment (TcpPcb *pcb, IpBufRef data, bool fin_pending, SeqType rem_wnd)
    {
        AMBRO_ASSERT(can_output_in_state(pcb->state))
        AMBRO_ASSERT(data.tot_len <= pcb->snd_buf.tot_len)
        AMBRO_ASSERT(!fin_pending || !snd_open_in_state(pcb->state))
        AMBRO_ASSERT(data.tot_len > 0 || fin_pending)
        AMBRO_ASSERT(rem_wnd > 0)
        
        // Determine offset from start of send buffer.
        size_t offset = pcb->snd_buf.tot_len - data.tot_len;
        
        // Determine segment data.
        auto min_wnd_mss = MinValueU(rem_wnd, pcb->snd_mss);
        size_t seg_data_len = MinValueU(min_wnd_mss, data.tot_len);
        IpBufRef seg_data = data.subTo(seg_data_len);
        
        // Check if the segment contains pushed data.
        bool push = pcb->snd_psh_index > offset;
        
        // Determine segment flags.
        FlagsType seg_flags = Tcp4FlagAck;
        if (seg_data_len == data.tot_len && fin_pending && rem_wnd > seg_data_len) {
            seg_flags |= Tcp4FlagFin|Tcp4FlagPsh;
            push = true;
        }
        else if (push && pcb->snd_psh_index <= offset + seg_data_len) {
            seg_flags |= Tcp4FlagPsh;
        }
        
        // Avoid sending small segments for better efficiency.
        // This is tricky because we must prevent lockup:
        // - There must be no additional delay for any data before snd_psh_index.
        // - There must be no additional delay for the FIN.
        // - There must be progress even if the send window is always below snd_mss.
        if (!push && seg_data_len < min_wnd_mss) {
            return 0;
        }
        
        // Send the segment.
        SeqType seq_num = seq_add(pcb->snd_una, offset);
        TcpSegMeta tcp_meta = {pcb->local_port, pcb->remote_port, seq_num, pcb->rcv_nxt,
                               Input::pcb_ann_wnd(pcb), seg_flags};
        send_tcp(pcb->tcp, pcb->local_addr, pcb->remote_addr, tcp_meta, seg_data);
        
        // Calculate the sequence length of segment, if we sent FIN set the FIN_SENT flag.
        SeqType seg_seqlen = seg_data_len;
        if ((seg_flags & Tcp4FlagFin) != 0) {
            seg_seqlen++;
            pcb->setFlag(PcbFlags::FIN_SENT);
        }
        
        // Calculate the end sequence number of the sent segment.
        SeqType seg_endseq = seq_add(seq_num, seg_seqlen);
        
        // Stop a round-trip-time measurement if we have retransmitted
        // a segment containing the associated sequence number.
        if (pcb->hasFlag(PcbFlags::RTT_PENDING) &&
            seq_lte(seq_num, pcb->rtt_test_seq, pcb->snd_una) &&
            seq_lt(pcb->rtt_test_seq, seg_endseq, pcb->snd_una))
        {
            pcb->clearFlag(PcbFlags::RTT_PENDING);
        }
        
        // Did we send anything new?
        if (seq_lt(pcb->snd_nxt, seg_endseq, pcb->snd_una)) {
            // Start a round-trip-time measurement if not already started.
            if (!pcb->hasFlag(PcbFlags::RTT_PENDING)) {
                pcb->setFlag(PcbFlags::RTT_PENDING);
                pcb->rtt_test_seq = pcb->snd_nxt;
                pcb->rtt_test_time = Clock::getTime(Context());
            }
            
            // Bump snd_nxt.
            pcb->snd_nxt = seg_endseq;
        }
        
        return seg_seqlen;
    }
    
    /**
     * Drives transmission of data including FIN.
     * Returns whether anything has been sent, excluding a possible zero-window probe.
     */
    static bool pcb_output (TcpPcb *pcb)
    {
        AMBRO_ASSERT(can_output_in_state(pcb->state))
        
        // If there is nothing outstanding, stop the retransmission timer and return.
        if (!pcb_has_snd_outstanding(pcb)) {
            pcb->rtx_timer.unset(Context());
            return false;
        }
        
        // Check window.
        SeqType rem_wnd;
        bool zero_window = (pcb->snd_wnd == 0);
        if (AMBRO_UNLIKELY(zero_window)) {
            // Zero window:
            // - Pretend that there is one count available so that we allow ourselves
            //   to send a zero-window probe.
            // - Ensure that all outstanding data or FIN is queued for transmission.
            //   This is needed due to possible window shrinking.
            rem_wnd = 1;
            pcb_requeue_snd(pcb);
        } else {
            rem_wnd = pcb->snd_wnd - MinValueU(pcb->snd_wnd, pcb_snd_offset(pcb));
        }
        
        // Will need to know if we sent anything.
        bool sent = false;
        
        // While we have something to send and some window is available...
        while ((pcb->snd_buf_cur.tot_len > 0 || pcb->hasFlag(PcbFlags::FIN_PENDING)) && rem_wnd > 0) {
            // Send a segment.
            // NOTE: watch out for pcb_output_segment preconditions.
            SeqType seg_seqlen = pcb_output_segment(
                pcb, pcb->snd_buf_cur, pcb->hasFlag(PcbFlags::FIN_PENDING), rem_wnd);
            AMBRO_ASSERT(seg_seqlen <= rem_wnd)
            
            // If we sent nothing this must be because we are hoping to send a larger
            // segment later.
            if (seg_seqlen == 0) {
                break;
            }
            
            // Advance snd_buf_cur over any data just sent.
            size_t data_sent = MinValueU(seg_seqlen, pcb->snd_buf_cur.tot_len);
            pcb->snd_buf_cur.skipBytes(data_sent);
            
            // Clear FIN_PENDING flag if we sent a FIN.
            if (seg_seqlen > data_sent) {
                AMBRO_ASSERT(pcb->hasFlag(PcbFlags::FIN_PENDING))
                AMBRO_ASSERT(seg_seqlen - 1 == data_sent)
                pcb->clearFlag(PcbFlags::FIN_PENDING);
            }
            
            // Update local state.
            rem_wnd -= seg_seqlen;
            sent = true;
        }
        
        if (AMBRO_UNLIKELY(zero_window)) {
            // Zero window => setup timer for zero window probe.
            if (!pcb->rtx_timer.isSet(Context()) || !pcb->hasFlag(PcbFlags::ZERO_WINDOW)) {
                pcb->rtx_timer.appendAfter(Context(), TcpProto::ZeroWindowTimeTicks);
                pcb->setFlag(PcbFlags::ZERO_WINDOW);
            }
        }
        else if (pcb->snd_buf_cur.tot_len < pcb->snd_buf.tot_len || !snd_open_in_state(pcb->state))
        {
            // Unacked data or sending closed => setup timer for retransmission.
            if (!pcb->rtx_timer.isSet(Context()) || pcb->hasFlag(PcbFlags::ZERO_WINDOW)) {
                TimeType rtx_time = (TimeType)pcb->rto << TcpProto::RttShift;
                pcb->rtx_timer.appendAfter(Context(), rtx_time);
                pcb->clearFlag(PcbFlags::ZERO_WINDOW);
            }
        }
        else {
            // No zero window, no unacked data, sending not closed => stop the timer.
            // This can only happen because we are delaying transmission in hope of
            // sending a larger segment later.
            AMBRO_ASSERT(pcb->snd_wnd > 0)
            AMBRO_ASSERT(pcb->snd_buf_cur.tot_len == pcb->snd_buf.tot_len)
            AMBRO_ASSERT(snd_open_in_state(pcb->state))
            AMBRO_ASSERT(pcb->snd_psh_index == 0)
            AMBRO_ASSERT(pcb->snd_buf.tot_len < pcb->snd_mss)
            
            pcb->rtx_timer.unset(Context());
        }
        
        // We return whether we sent something except that we exclude zero-window
        // probes. This is because the return value is used to determine whether
        // we sent a valid ACK, but a zero-window probe will likely be discarded
        // by the receiver before ACK-processing.
        return (sent && !zero_window);
    }
    
    static void pcb_output_timer_handler (TcpPcb *pcb)
    {
        AMBRO_ASSERT(can_output_in_state(pcb->state))
        
        // Drive the transmission.
        pcb_output(pcb);
    }
    
    static void pcb_rtx_timer_handler (TcpPcb *pcb)
    {
        AMBRO_ASSERT(can_output_in_state(pcb->state))
        AMBRO_ASSERT(pcb_has_snd_outstanding(pcb))
        AMBRO_ASSERT((pcb->snd_wnd == 0) == pcb->hasFlag(PcbFlags::ZERO_WINDOW))
        
        // If the timer was for a retransmission, try to send at most one
        // segment from the start of the send buffer.
        if (!pcb->hasFlag(PcbFlags::ZERO_WINDOW)) {
            // NOTE: watch out for pcb_output_segment preconditions.
            SeqType rem_wnd = MinValueU(pcb->snd_wnd, pcb->snd_mss);
            SeqType seg_seqlen = pcb_output_segment(
                pcb, pcb->snd_buf, !snd_open_in_state(pcb->state), rem_wnd);
            
            // If we sent something, double the retransmission time and
            // restart the retransmission timer.
            if (seg_seqlen > 0) {
                RttType doubled_rto = (pcb->rto > RttTypeMax / 2) ? RttTypeMax : (2 * pcb->rto);
                pcb->rto = MinValue(TcpProto::MaxRtxTime, doubled_rto);
                TimeType rtx_time = (TimeType)pcb->rto << TcpProto::RttShift;
                pcb->rtx_timer.appendAfter(Context(), rtx_time);
                return;
            }
            // We can only get here if pcb_output_segment sent nothing because
            // it's hoping for a larger segment. This probably can't happen for
            // a retransmission but let's be safe and fall through to pcb_output.
        }
        
        // If the timer was for a zero-window probe or it was for a retransmission
        // whish is no longer applicable, call pcb_output to keep sending going.
        pcb_output(pcb);
    }
    
    static void pcb_rtt_test_seq_acked (TcpPcb *pcb)
    {
        AMBRO_ASSERT(pcb->hasFlag(PcbFlags::RTT_PENDING))
        
        // Clear the flag to indicate end of RTT measurement.
        pcb->clearFlag(PcbFlags::RTT_PENDING);
        
        // Calculate how much time has passed, also in RTT units.
        TimeType time_diff = Clock::getTime(Context()) - pcb->rtt_test_time;
        RttType this_rtt = MinValueU(RttTypeMax, time_diff >> TcpProto::RttShift);
        
        // Update RTTVAR and SRTT.
        if (!pcb->hasFlag(PcbFlags::RTT_VALID)) {
            pcb->setFlag(PcbFlags::RTT_VALID);
            pcb->rttvar = this_rtt/2;
            pcb->srtt = this_rtt;
        } else {
            RttType rtt_diff = AbsoluteDiff(pcb->srtt, this_rtt);
            pcb->rttvar = ((RttNextType)3 * pcb->rttvar + rtt_diff) / 4;
            pcb->srtt = ((RttNextType)7 * pcb->srtt + this_rtt) / 8;
        }
        
        // Update RTO.
        pcb_update_rto(pcb);
    }
    
    // Update the RTO from RTTVAR and SRTT.
    static void pcb_update_rto (TcpPcb *pcb)
    {
        int const k = 4;
        RttType k_rttvar = (pcb->rttvar > RttTypeMax / k) ? RttTypeMax : (k * pcb->rttvar);
        RttType var_part = MaxValue((RttType)1, k_rttvar);
        RttType base_rto = (var_part > RttTypeMax - pcb->srtt) ? RttTypeMax : (pcb->srtt + var_part);
        pcb->rto = MaxValue(TcpProto::MinRtxTime, MinValue(TcpProto::MaxRtxTime, base_rto));
    }
    
    // Send an RST as a reply to a received segment.
    // This conforms to RFC 793 handling of segments not belonging to a known
    // connection.
    static void send_rst_reply (TcpProto *tcp, Ip4DgramMeta const &ip_meta,
                                TcpSegMeta const &tcp_meta, size_t tcp_data_len)
    {
        SeqType rst_seq_num;
        bool rst_ack;
        SeqType rst_ack_num;
        if ((tcp_meta.flags & Tcp4FlagAck) != 0) {
            rst_seq_num = tcp_meta.ack_num;
            rst_ack = false;
            rst_ack_num = 0;
        } else {
            rst_seq_num = 0;
            rst_ack = true;
            rst_ack_num = tcp_meta.seq_num + tcplen(tcp_meta.flags, tcp_data_len);
        }
        
        send_rst(tcp, ip_meta.local_addr, ip_meta.remote_addr,
                 tcp_meta.local_port, tcp_meta.remote_port,
                 rst_seq_num, rst_ack, rst_ack_num);
    }
    
    static void send_rst (TcpProto *tcp, Ip4Addr local_addr, Ip4Addr remote_addr,
                          PortType local_port, PortType remote_port,
                          SeqType seq_num, bool ack, SeqType ack_num)
    {
        FlagsType flags = Tcp4FlagRst | (ack ? Tcp4FlagAck : 0);
        TcpSegMeta tcp_meta = {local_port, remote_port, seq_num, ack_num, 0, flags};
        send_tcp(tcp, local_addr, remote_addr, tcp_meta);
    }
    
    static void send_tcp (TcpProto *tcp, Ip4Addr local_addr, Ip4Addr remote_addr,
                          TcpSegMeta const &tcp_meta, IpBufRef data=IpBufRef{})
    {
        // Compute length of TCP options.
        uint8_t opts_len = (tcp_meta.opts != nullptr) ? TcpUtils::calc_options_len(*tcp_meta.opts) : 0;
        
        // Allocate memory for headers.
        TxAllocHelper<BufAllocator, Tcp4Header::Size+TcpUtils::MaxOptionsWriteLen, HeaderBeforeIp4Dgram>
            dgram_alloc(Tcp4Header::Size+opts_len);
        
        // Caculate the offset+flags field.
        FlagsType offset_flags = ((FlagsType)(5+opts_len/4) << TcpOffsetShift) | tcp_meta.flags;
        
        // Write the TCP header.
        auto tcp_header = Tcp4Header::MakeRef(dgram_alloc.getPtr());
        tcp_header.set(Tcp4Header::SrcPort(),     tcp_meta.local_port);
        tcp_header.set(Tcp4Header::DstPort(),     tcp_meta.remote_port);
        tcp_header.set(Tcp4Header::SeqNum(),      tcp_meta.seq_num);
        tcp_header.set(Tcp4Header::AckNum(),      tcp_meta.ack_num);
        tcp_header.set(Tcp4Header::OffsetFlags(), offset_flags);
        tcp_header.set(Tcp4Header::WindowSize(),  tcp_meta.window_size);
        tcp_header.set(Tcp4Header::Checksum(),    0);
        tcp_header.set(Tcp4Header::UrgentPtr(),   0);
        
        // Write any TCP options.
        if (tcp_meta.opts != nullptr) {
            TcpUtils::write_options(*tcp_meta.opts, dgram_alloc.getPtr() + Tcp4Header::Size);
        }
        
        // Construct the datagram reference including any data.
        IpBufNode data_node;
        if (data.tot_len > 0) {
            data_node = data.toNode();
            dgram_alloc.setNext(&data_node, data.tot_len);
        }
        IpBufRef dgram = dgram_alloc.getBufRef();
        
        // Calculate TCP checksum.
        IpChksumAccumulator chksum_accum;
        chksum_accum.addWords(&local_addr.data);
        chksum_accum.addWords(&remote_addr.data);
        chksum_accum.addWord(WrapType<uint16_t>(), Ip4ProtocolTcp);
        chksum_accum.addWord(WrapType<uint16_t>(), dgram.tot_len);
        chksum_accum.addIpBuf(dgram);
        uint16_t calc_chksum = chksum_accum.getChksum();
        tcp_header.set(Tcp4Header::Checksum(), calc_chksum);
        
        // Send the datagram.
        Ip4DgramMeta meta = {local_addr, remote_addr, TcpProto::TcpTTL, Ip4ProtocolTcp};
        tcp->m_stack->sendIp4Dgram(meta, dgram);
    }
};

#include <aprinter/EndNamespace.h>

#endif
