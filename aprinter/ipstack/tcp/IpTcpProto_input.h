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

#ifndef APRINTER_IPSTACK_IP_TCP_PROTO_INPUT_H
#define APRINTER_IPSTACK_IP_TCP_PROTO_INPUT_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <limits.h>

#include <aprinter/meta/MinMax.h>
#include <aprinter/base/Preprocessor.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/OneOf.h>
#include <aprinter/base/Hints.h>
#include <aprinter/ipstack/misc/Buf.h>
#include <aprinter/ipstack/misc/Chksum.h>
#include <aprinter/ipstack/proto/Tcp4Proto.h>
#include <aprinter/ipstack/proto/TcpUtils.h>

#include <aprinter/BeginNamespace.h>

template <typename TcpProto>
class IpTcpProto_input
{
    APRINTER_USE_TYPES2(TcpUtils, (FlagsType, SeqType, TcpState, TcpSegMeta, TcpOptions))
    APRINTER_USE_VALS(TcpUtils, (seq_add, seq_diff, seq_lte, seq_lt, tcplen,
                                 can_output_in_state, accepting_data_in_state))
    APRINTER_USE_TYPES1(TcpProto, (Context, Ip4DgramMeta, TcpListener, TcpPcb, PcbFlags,
                                   Output, OosSeg))
    
public:
    static void recvIp4Dgram (TcpProto *tcp, Ip4DgramMeta const &ip_meta, IpBufRef dgram)
    {
        // The local address must be the address of the incoming interface.
        if (!ip_meta.iface->ip4AddrIsLocalAddr(ip_meta.local_addr)) {
            return;
        }
        
        // Check header size, must fit in first buffer.
        if (!dgram.hasHeader(Tcp4Header::Size)) {
            return;
        }
        
        TcpSegMeta tcp_meta;
        
        // Read header fields.
        auto tcp_header = Tcp4Header::MakeRef(dgram.getChunkPtr());
        tcp_meta.remote_port = tcp_header.get(Tcp4Header::SrcPort());
        tcp_meta.local_port  = tcp_header.get(Tcp4Header::DstPort());
        tcp_meta.seq_num     = tcp_header.get(Tcp4Header::SeqNum());
        tcp_meta.ack_num     = tcp_header.get(Tcp4Header::AckNum());
        tcp_meta.flags       = tcp_header.get(Tcp4Header::OffsetFlags());
        tcp_meta.window_size = tcp_header.get(Tcp4Header::WindowSize());
        
        // Check data offset.
        uint8_t data_offset = (tcp_meta.flags >> TcpOffsetShift) * 4;
        if (data_offset < Tcp4Header::Size || data_offset > dgram.tot_len) {
            return;
        }
        
        // Check TCP checksum.
        IpChksumAccumulator chksum_accum;
        chksum_accum.addWords(&ip_meta.local_addr.data);
        chksum_accum.addWords(&ip_meta.remote_addr.data);
        chksum_accum.addWord(WrapType<uint16_t>(), Ip4ProtocolTcp);
        chksum_accum.addWord(WrapType<uint16_t>(), dgram.tot_len);
        chksum_accum.addIpBuf(dgram);
        if (chksum_accum.getChksum() != 0) {
            return;
        }
        
        // Get a buffer reference starting at the option data and compute
        // how many bytes of options there options are.
        IpBufRef opts_and_data = dgram.hideHeader(Tcp4Header::Size);
        uint8_t opts_len = data_offset - Tcp4Header::Size;
        
        // Parse TCP options and get a reference to the data.
        TcpOptions tcp_opts;
        IpBufRef tcp_data;
        TcpUtils::parse_options(opts_and_data, opts_len, &tcp_opts, &tcp_data);
        tcp_meta.opts = &tcp_opts;
        
        // Try to handle using a PCB.
        for (TcpPcb &pcb : tcp->m_pcbs) {
            if (pcb_try_input(tcp, &pcb, ip_meta, tcp_meta, tcp_data)) {
                return;
            }
        }
        
        // Try to handle using a listener.
        for (TcpListener *lis = tcp->m_listeners_list.first(); lis != nullptr; lis = tcp->m_listeners_list.next(lis)) {
            AMBRO_ASSERT(lis->m_listening)
            if (lis->m_port == tcp_meta.local_port &&
                (lis->m_addr == ip_meta.local_addr || lis->m_addr == Ip4Addr::ZeroAddr()))
            {
                return listen_input(lis, ip_meta, tcp_meta, tcp_data.tot_len);
            }
        }
        
        // Reply with RST, unless this is an RST.
        if ((tcp_meta.flags & Tcp4FlagRst) == 0) {
            Output::send_rst_reply(tcp, ip_meta, tcp_meta, tcp_data.tot_len);
        }
    }
    
    // Get a window size value to be put into a segment being sent.
    static uint16_t pcb_ann_wnd (TcpPcb *pcb)
    {
        uint16_t wnd_to_ann = MinValue(pcb->rcv_wnd, (SeqType)UINT16_MAX);
        pcb->rcv_ann = seq_add(pcb->rcv_nxt, wnd_to_ann);
        return wnd_to_ann;
    }
    
    // Determine how much new window would be anounced if we sent a window update.
    static SeqType pcb_get_wnd_ann_incr (TcpPcb *pcb)
    {
        uint16_t wnd_to_ann = MinValue(pcb->rcv_wnd, (SeqType)UINT16_MAX);
        SeqType new_rcv_ann = seq_add(pcb->rcv_nxt, wnd_to_ann);
        return seq_diff(new_rcv_ann, pcb->rcv_ann);
    }
    
    static void pcb_rcv_buf_extended (TcpPcb *pcb)
    {
        AMBRO_ASSERT(pcb->state != TcpState::CLOSED)
        AMBRO_ASSERT(pcb->con != nullptr)
        
        // If we haven't received a FIN yet, update the receive window.
        if (accepting_data_in_state(pcb->state)) {
            pcb_update_rcv_wnd(pcb);
        }
    }
    
    static void pcb_update_rcv_wnd (TcpPcb *pcb)
    {
        AMBRO_ASSERT(accepting_data_in_state(pcb->state))
        
        // Update the receive window based on the receive buffer.
        SeqType avail_wnd = MinValueU(pcb->rcvBufLen(), TcpProto::MaxRcvWnd);
        if (pcb->rcv_wnd < avail_wnd) {
            pcb->rcv_wnd = avail_wnd;
        }
        
        // Generate a window update if needed.
        if (pcb_get_wnd_ann_incr(pcb) >= pcb->rcv_ann_thres) {
            Output::pcb_need_ack(pcb);
        }
    }
    
private:
    static void listen_input (TcpListener *lis, Ip4DgramMeta const &ip_meta,
                              TcpSegMeta const &tcp_meta, size_t tcp_data_len)
    {
        do {
            // For a new connection we expect SYN flag and no FIN, RST, ACK.
            if ((tcp_meta.flags & Tcp4BasicFlags) != Tcp4FlagSyn) {
                // If the segment has no RST and has ACK, reply with RST; otherwise drop.
                // This includes dropping SYN+FIN packets though RFC 793 does not say this
                // should be done.
                if ((tcp_meta.flags & (Tcp4FlagRst|Tcp4FlagAck)) == Tcp4FlagAck) {
                    goto refuse;
                }
                return;
            }
            
            // Check maximum number of PCBs for this listener.
            if (lis->m_num_pcbs >= lis->m_max_pcbs) {
                goto refuse;
            }
            
            // Calculate MSSes.
            uint16_t iface_mss = TcpProto::get_iface_mss(ip_meta.iface);
            uint16_t snd_mss;
            if (!TcpUtils::calc_snd_mss<TcpProto::MinAllowedMss>(iface_mss, *tcp_meta.opts, &snd_mss)) {
                goto refuse;
            }
            
            // Allocate a PCB.
            TcpPcb *pcb = lis->m_tcp->allocate_pcb();
            if (pcb == nullptr) {
                goto refuse;
            }
            
            // Generate an initial sequence number.
            SeqType iss = TcpProto::make_iss();
            
            // Initialize the PCB.
            pcb->state = TcpState::SYN_RCVD;
            pcb->flags = 0;
            pcb->lis = lis;
            pcb->local_addr = ip_meta.local_addr;
            pcb->remote_addr = ip_meta.remote_addr;
            pcb->local_port = tcp_meta.local_port;
            pcb->remote_port = tcp_meta.remote_port;
            pcb->rcv_nxt = seq_add(tcp_meta.seq_num, 1);
            pcb->rcv_wnd = lis->m_initial_rcv_wnd;
            pcb->rcv_ann = pcb->rcv_nxt;
            pcb->rcv_ann_thres = TcpProto::DefaultWndAnnThreshold;
            pcb->rcv_mss = iface_mss;
            pcb->snd_una = iss;
            pcb->snd_nxt = seq_add(iss, 1);
            pcb->snd_buf_cur = IpBufRef{};
            pcb->snd_psh_index = 0;
            pcb->snd_mss = snd_mss;
            pcb->rto = TcpProto::InitialRtxTime;
            pcb->num_ooseq = 0;
            
            // These will be initialized at transition to ESTABLISHED:
            // snd_wnd, snd_wl1, snd_wl2
            
            // Increment the listener's PCB count.
            AMBRO_ASSERT(lis->m_num_pcbs < INT_MAX)
            lis->m_num_pcbs++;
            
            // Start the SYN_RCVD abort timeout.
            pcb->abrt_timer.appendAfter(Context(), TcpProto::SynRcvdTimeoutTicks);
            
            // Reply with a SYN+ACK.
            Output::pcb_send_syn_ack(pcb);
            return;
        } while (false);
        
    refuse:
        // Refuse connection by RST.
        Output::send_rst_reply(lis->m_tcp, ip_meta, tcp_meta, tcp_data_len);
    }
    
    inline static bool pcb_try_input (TcpProto *tcp, TcpPcb *pcb, Ip4DgramMeta const &ip_meta,
                                      TcpSegMeta const &tcp_meta, IpBufRef tcp_data)
    {
        if (pcb->state != TcpState::CLOSED &&
            pcb->local_addr  == ip_meta.local_addr &&
            pcb->remote_addr == ip_meta.remote_addr &&
            pcb->local_port  == tcp_meta.local_port &&
            pcb->remote_port == tcp_meta.remote_port)
        {
            AMBRO_ASSERT(tcp->m_current_pcb == nullptr)
            tcp->m_current_pcb = pcb;
            pcb_input_core(pcb, tcp_meta, tcp_data);
            tcp->m_current_pcb = nullptr;
            return true;
        }
        return false;
    }
    
    static void pcb_input_core (TcpPcb *pcb, TcpSegMeta const &tcp_meta, IpBufRef tcp_data)
    {
        AMBRO_ASSERT(pcb->state != OneOf(TcpState::CLOSED, TcpState::SYN_SENT))
        AMBRO_ASSERT(pcb->tcp->m_current_pcb == pcb)
        
        // Do basic processing, e.g.:
        // - Handle RST and SYN.
        // - Check acceptability.
        // - Trim segment into window.
        // - Check ACK validity.
        SeqType eff_seq;
        bool seg_fin;
        bool new_ack;
        if (!pcb_input_basic_processing(pcb, tcp_meta, tcp_data, eff_seq, seg_fin, new_ack)) {
            return;
        }
        
        if (AMBRO_UNLIKELY(pcb->state == TcpState::SYN_RCVD)) {
            // Do SYN-RCVD specific processing.
            // Normally we transition to ESTABLISHED state here.
            if (!pcb_input_syn_rcvd_processing(pcb, tcp_meta, new_ack)) {
                return;
            }
        } else {
            // Process acknowledgements and window updates.
            if (!pcb_input_ack_wnd_processing(pcb, tcp_meta, new_ack)) {
                return;
            }
        }
        
        if (AMBRO_LIKELY(accepting_data_in_state(pcb->state))) {
            // Process received data or FIN.
            if (!pcb_input_rcv_processing(pcb, eff_seq, seg_fin, tcp_data)) {
                return;
            }
        }
        else if (pcb->state == TcpState::TIME_WAIT) {
            // Reply with an ACK and restart the timeout.
            pcb->setFlag(PcbFlags::ACK_PENDING);
            pcb->abrt_timer.appendAfter(Context(), TcpProto::TimeWaitTimeTicks);
        }
        
        // Try to output if desired.
        if (pcb->hasFlag(PcbFlags::OUT_PENDING)) {
            pcb->clearFlag(PcbFlags::OUT_PENDING);
            if (can_output_in_state(pcb->state)) {
                bool sent_ack = Output::pcb_output(pcb);
                if (sent_ack) {
                    pcb->clearFlag(PcbFlags::ACK_PENDING);
                }
            }
        }
        
        // Send an empty ACK if desired.
        // Note, ACK_PENDING will have been cleared above if pcb_output sent anything,
        // in that case we don't need an empty ACK here.
        if (pcb->hasFlag(PcbFlags::ACK_PENDING)) {
            pcb->clearFlag(PcbFlags::ACK_PENDING);
            Output::pcb_send_empty_ack(pcb);
        }
    }
    
    static bool pcb_input_basic_processing (TcpPcb *pcb, TcpSegMeta const &tcp_meta,
                                            IpBufRef &tcp_data, SeqType &eff_seq,
                                            bool &seg_fin, bool &new_ack)
    {
        // Get right edge of receive window.
        SeqType nxt_wnd = seq_add(pcb->rcv_nxt, pcb->rcv_wnd);
        
        // Handle uncommon flags.
        if (AMBRO_UNLIKELY((tcp_meta.flags & (Tcp4FlagRst|Tcp4FlagSyn|Tcp4FlagAck)) != Tcp4FlagAck)) {
            if ((tcp_meta.flags & Tcp4FlagRst) != 0) {
                // RST, handle as per RFC 5961.
                if (tcp_meta.seq_num == pcb->rcv_nxt) {
                    TcpProto::pcb_abort(pcb, false);
                }
                else if (seq_lte(tcp_meta.seq_num, nxt_wnd, pcb->rcv_nxt)) {
                    // We're slightly violating RFC 5961 by allowing seq_num at nxt_wnd.
                    Output::pcb_send_empty_ack(pcb);
                }
            }
            else if ((tcp_meta.flags & Tcp4FlagSyn) != 0) {
                // SYN, handle as per RFC 5961.
                if (pcb->state == TcpState::SYN_RCVD &&
                    tcp_meta.seq_num == seq_add(pcb->rcv_nxt, -1))
                {
                    // This seems to be a retransmission of the SYN, retransmit our
                    // SYN+ACK and bump the abort timeout.
                    Output::pcb_send_syn_ack(pcb);
                    pcb->abrt_timer.appendAfter(Context(), TcpProto::SynRcvdTimeoutTicks);
                } else {
                    Output::pcb_send_empty_ack(pcb);
                }
            }
            // Segment without none of RST, SYN and ACK should never be sent.
            // Just drop it here. Note that RFC 793 would have us check the
            // sequence number and possibly send an empty ACK if the segment
            // is outside the window, but we don't do that for perfomance.
            return false;
        }
        
        // Sequence length of segment (data+flags).
        size_t seqlen = tcplen(tcp_meta.flags, tcp_data.tot_len);
        
        // Determine acceptability of segment.
        bool acceptable;
        bool left_edge_in_window;
        bool right_edge_in_window;
        if (seqlen == 0) {
            // Empty segment is acceptable if the sequence number is within or at
            // the right edge of the receive window. Allowing the latter with
            // nonzero receive window violates RFC 793, but seems to make sense.
            acceptable = seq_lte(tcp_meta.seq_num, nxt_wnd, pcb->rcv_nxt);
        } else {
            // Nonzero-length segment is acceptable if its left or right edge
            // is within the receive window. Except for SYN_RCVD, we are not expecting
            // any data to the left.
            SeqType last_seq = seq_add(tcp_meta.seq_num, seq_add(seqlen, -1));
            left_edge_in_window = seq_lt(tcp_meta.seq_num, nxt_wnd, pcb->rcv_nxt);
            right_edge_in_window = seq_lt(last_seq, nxt_wnd, pcb->rcv_nxt);
            if (AMBRO_UNLIKELY(pcb->state == TcpState::SYN_RCVD)) {
                acceptable = left_edge_in_window;
            } else {
                acceptable = left_edge_in_window || right_edge_in_window;
            }
        }
        
        // If not acceptable, send any appropriate response and drop.
        if (AMBRO_UNLIKELY(!acceptable)) {
            Output::pcb_send_empty_ack(pcb);
            return false;
        }
        
        // Trim the segment on the left or right so that it fits into the receive window.
        eff_seq = tcp_meta.seq_num;
        seg_fin = (tcp_meta.flags & Tcp4FlagFin) != 0;
        if (AMBRO_LIKELY(seqlen > 0)) {
            SeqType left_keep;
            if (AMBRO_UNLIKELY(!left_edge_in_window)) {
                // The segment contains some already received data (seq_num < rcv_nxt).
                SeqType left_trim = seq_diff(pcb->rcv_nxt, tcp_meta.seq_num);
                AMBRO_ASSERT(left_trim > 0)   // because !left_edge_in_window
                AMBRO_ASSERT(left_trim < seqlen) // because right_edge_in_window
                eff_seq = pcb->rcv_nxt;
                seqlen -= left_trim;
                // No change to seg_fin: for SYN we'd have bailed out earlier,
                // and FIN could not be trimmed because left_trim < seqlen.
                tcp_data.skipBytes(left_trim);
            }
            else if (AMBRO_UNLIKELY(seqlen > (left_keep = seq_diff(nxt_wnd, tcp_meta.seq_num)))) {
                // The segment contains some extra data beyond the receive window.
                AMBRO_ASSERT(left_keep > 0)   // because left_edge_in_window
                AMBRO_ASSERT(left_keep < seqlen) // because of condition
                seqlen = left_keep;
                seg_fin = false; // a FIN would be outside the window
                tcp_data.tot_len = left_keep;
            }
        }
        
        // Check ACK validity as per RFC 5961.
        // For this arithemtic to work we're relying on snd_nxt not wrapping around
        // over snd_una-MaxAckBefore. Currently this cannot happen because we don't
        // support window scaling so snd_nxt-snd_una cannot even exceed 2^16-1.
        SeqType past_ack_num = seq_diff(pcb->snd_una, TcpProto::MaxAckBefore);
        bool valid_ack = seq_lte(tcp_meta.ack_num, pcb->snd_nxt, past_ack_num);
        if (AMBRO_UNLIKELY(!valid_ack)) {
            Output::pcb_send_empty_ack(pcb);
            return false;
        }
        
        // Bump the last-time of the PCB.
        pcb->last_time = TcpProto::Clock::getTime(Context());
        
        // Check if the ACK acknowledges anything new.
        new_ack = !seq_lte(tcp_meta.ack_num, pcb->snd_una, past_ack_num);
        
        return true;
    }
    
    static bool pcb_input_syn_rcvd_processing (TcpPcb *pcb, TcpSegMeta const &tcp_meta, bool new_ack)
    {
        // If our SYN is not acknowledged, send RST and drop.
        // RFC 793 seems to allow ack_num==snd_una which doesn't make sense.
        if (!new_ack) {
            Output::send_rst(pcb->tcp, pcb->local_addr, pcb->remote_addr,
                                pcb->local_port, pcb->remote_port,
                                tcp_meta.ack_num, false, 0);
            return false;
        }
        
        // In SYN_RCVD the remote acks only our SYN no more.
        // Otherwise we would have bailed out already (valid_ack, new_ack).
        AMBRO_ASSERT(tcp_meta.ack_num == seq_add(pcb->snd_una, 1))
        AMBRO_ASSERT(tcp_meta.ack_num == pcb->snd_nxt)
        
        // Stop the SYN_RCVD abort timer.
        pcb->abrt_timer.unset(Context());
        
        // Go to ESTABLISHED state.
        pcb->state = TcpState::ESTABLISHED;
        
        // Update snd_una due to one sequence count having been ACKed.
        pcb->snd_una = tcp_meta.ack_num;
        
        // Remember the initial send window.
        pcb->snd_wnd = tcp_meta.window_size;
        pcb->snd_wl1 = tcp_meta.seq_num;
        pcb->snd_wl2 = tcp_meta.ack_num;
        
        // We have a TcpListener (if it went away the PCB would have been aborted).
        // We also have no TcpConnection.
        TcpListener *lis = pcb->lis;
        AMBRO_ASSERT(lis != nullptr)
        AMBRO_ASSERT(lis->m_listening)
        AMBRO_ASSERT(lis->m_accept_pcb == nullptr)
        AMBRO_ASSERT(pcb->con == nullptr)
        
        // Inform the user that the connection has been established
        // and allow association with a TcpConnection to be done.
        TcpProto *tcp = pcb->tcp;
        lis->m_accept_pcb = pcb;
        lis->m_callback->connectionEstablished(lis);
        
        // Not referencing the listener after this, it might have been deinited from
        // the callback. In any case we will not leave lis->m_accept_pcb pointing
        // to this PCB, that is assured by acceptConnection or pcb_abort below.
        
        // Handle abort of PCB.
        if (AMBRO_UNLIKELY(tcp->m_current_pcb == nullptr)) {
            return false;
        }
        
        // Possible transitions in callback (except to CLOSED):
        // - ESTABLISHED->FIN_WAIT_1
        
        // If the connection has not been accepted or has been accepted but
        // already abandoned, abort with RST.
        if (AMBRO_UNLIKELY(pcb->con == nullptr)) {
            TcpProto::pcb_abort(pcb, true);
            return false;
        }
        
        return true;
    }
    
    static bool pcb_input_ack_wnd_processing (TcpPcb *pcb, TcpSegMeta const &tcp_meta, bool new_ack)
    {
        // Handle new acknowledgments.
        if (new_ack) {
            // We can only get here if there was anything pending acknowledgement.
            AMBRO_ASSERT(can_output_in_state(pcb->state))
            
            // Calculate the amount of acknowledged sequence counts.
            // This can be data or FIN (but not SYN, as SYN_RCVD state is handled above).
            SeqType acked = seq_diff(tcp_meta.ack_num, pcb->snd_una);
            AMBRO_ASSERT(acked > 0)
            
            // Handle end of round-trip-time measurement.
            if (pcb->hasFlag(PcbFlags::RTT_PENDING) &&
                seq_lt(pcb->rtt_test_seq, tcp_meta.ack_num, pcb->snd_una))
            {
                Output::pcb_rtt_test_seq_acked(pcb);
            }
            
            // Update snd_una due to sequences having been ACKed.
            pcb->snd_una = tcp_meta.ack_num;
            
            // The snd_wnd needs adjustment because it is relative to snd_una.
            pcb->snd_wnd -= MinValue(pcb->snd_wnd, acked);
            
            // Stop the rtx_timer. Consider that by adjusting the
            // window here and in handling the ACK below, we might be
            // invalidating the asserts in pcb_rtx_timer_handler.
            pcb->rtx_timer.unset(Context());
            
            // Schedule pcb_output, so that the rtx_timer will be restarted
            // if needed (for retransmission or zero-window probe).
            pcb->setFlag(PcbFlags::OUT_PENDING);
            
            // Check if our FIN has just been acked.
            bool fin_acked = Output::pcb_fin_acked(pcb);
            
            // Calculate the amount of acknowledged data (without ACK of FIN).
            SeqType data_acked_seq = acked - (SeqType)fin_acked;
            AMBRO_ASSERT(data_acked_seq <= SIZE_MAX)
            size_t data_acked = data_acked_seq;
            
            if (data_acked > 0) {
                // We necessarily still have a TcpConnection, if the connection was
                // abandoned with unsent/unacked data, it would have been aborted.
                AMBRO_ASSERT(pcb->con != nullptr)
                AMBRO_ASSERT(data_acked <= pcb->con->m_snd_buf.tot_len)
                AMBRO_ASSERT(pcb->snd_buf_cur.tot_len <= pcb->con->m_snd_buf.tot_len)
                AMBRO_ASSERT(pcb->snd_psh_index <= pcb->con->m_snd_buf.tot_len)
                
                // Advance the send buffer.
                size_t cur_offset = Output::pcb_snd_offset(pcb);
                if (data_acked >= cur_offset) {
                    pcb->snd_buf_cur.skipBytes(data_acked - cur_offset);
                    pcb->con->m_snd_buf = pcb->snd_buf_cur;
                } else {
                    pcb->con->m_snd_buf.skipBytes(data_acked);
                }
                
                // Adjust the push index.
                pcb->snd_psh_index -= MinValue(pcb->snd_psh_index, data_acked);
                
                // Report data-sent event to the user.
                if (AMBRO_UNLIKELY(!TcpProto::pcb_event(pcb, [&](auto con) { con->data_sent(data_acked); }))) {
                    return false;
                }
                // Possible transitions in callback (except to CLOSED):
                // - ESTABLISHED->FIN_WAIT_1
                // - CLOSE_WAIT->LAST_ACK
            }
            
            if (AMBRO_UNLIKELY(fin_acked)) {
                // We must be in a state where we had queued a FIN for sending
                // but have not send it or received its acknowledgement yet.
                AMBRO_ASSERT(pcb->state == OneOf(TcpState::FIN_WAIT_1, TcpState::CLOSING,
                                                    TcpState::LAST_ACK))
                
                // Tell TcpConnection and application about end sent.
                if (AMBRO_UNLIKELY(!TcpProto::pcb_event(pcb, [&](auto con) { con->end_sent(); }))) {
                    return false;
                }
                // Possible transitions in callback (except to CLOSED): none.
                
                if (pcb->state == TcpState::FIN_WAIT_1) {
                    // FIN is accked in FIN_WAIT_1, transition to FIN_WAIT_2.
                    pcb->state = TcpState::FIN_WAIT_2;
                    // At this transition output_timer and rtx_timer must be unset
                    // due to assert in their handlers (rtx_timer was unset above).
                    pcb->output_timer.unset(Context());
                }
                else if (pcb->state == TcpState::CLOSING) {
                    // Transition to TIME_WAIT.
                    // Further processing below is not applicable so return here.
                    TcpProto::pcb_go_to_time_wait(pcb);
                    return false;
                }
                else {
                    AMBRO_ASSERT(pcb->state == TcpState::LAST_ACK)
                    // Close the PCB.
                    TcpProto::pcb_abort(pcb, false);
                    return false;
                }
            }
        }
        
        // Handle window updates.
        // But inhibit for old acknowledgements (ack_num<snd_una). Note that
        // for new acknowledgements, snd_una has been bumped to ack_num.
        if (AMBRO_LIKELY(pcb->snd_una == tcp_meta.ack_num)) {
            // Compare sequence and ack numbers with respect to nxt_wnd+1
            // and snd_nxt+1 as the minimum value, respectively.
            // Note that we use the original non-trimmed sequence number.
            SeqType wnd_seq_ref = seq_add(pcb->rcv_nxt, pcb->rcv_wnd+1);
            SeqType wnd_seq_seg = seq_diff(tcp_meta.seq_num, wnd_seq_ref);
            SeqType wnd_seq_old = seq_diff(pcb->snd_wl1, wnd_seq_ref);
            if (wnd_seq_seg > wnd_seq_old || (wnd_seq_seg == wnd_seq_old &&
                seq_lte(pcb->snd_wl2, tcp_meta.ack_num, seq_add(pcb->snd_nxt, 1))))
            {
                // Update the window.
                SeqType old_snd_wnd = pcb->snd_wnd;
                pcb->snd_wnd = tcp_meta.window_size;
                pcb->snd_wl1 = tcp_meta.seq_num;
                pcb->snd_wl2 = tcp_meta.ack_num;
                
                // If the window has increased, schedule pcb_output because it may
                // be possible to send something more.
                if (pcb->snd_wnd > old_snd_wnd) {
                    pcb->setFlag(PcbFlags::OUT_PENDING);
                }
                
                // Check if we need to end widow probing.
                // This is done by checking if the assert pcb_need_rtx_timer
                // has been invalidated while the rtx_timer was running.
                if (pcb->rtx_timer.isSet(Context()) && !Output::pcb_need_rtx_timer(pcb)) {
                    // Stop the timer to stop window probes and avoid hitting the assert.
                    pcb->rtx_timer.unset(Context());
                    
                    // Undo any increase of the retransmission time.
                    Output::pcb_update_rto(pcb);
                    
                    // Schedule pcb_output which should now send something.
                    pcb->setFlag(PcbFlags::OUT_PENDING);
                }
            }
        }
        
        return true;
    }
    
    static bool pcb_input_rcv_processing (TcpPcb *pcb, SeqType eff_seq, bool seg_fin,
                                          IpBufRef tcp_data)
    {
        AMBRO_ASSERT(accepting_data_in_state(pcb->state))
        
        // We only get here if the segment fits into the receive window.
        size_t data_offset = seq_diff(eff_seq, pcb->rcv_nxt);
        AMBRO_ASSERT(data_offset <= pcb->rcv_wnd)
        AMBRO_ASSERT(tcp_data.tot_len + seg_fin <= pcb->rcv_wnd - data_offset)
        
        // Abort the connection if we have no place to put received data.
        // This includes when the connection was abandoned.
        if (AMBRO_UNLIKELY(tcp_data.tot_len > 0 &&
                           pcb->rcvBufLen() < data_offset + tcp_data.tot_len)) {
            TcpProto::pcb_abort(pcb, true);
            return false;
        }
        
        // If this is an out-of-sequence segment, an ACK needs to be sent.
        if (AMBRO_UNLIKELY(data_offset > 0)) {
            pcb->setFlag(PcbFlags::ACK_PENDING);
        }
        
        // This will be the in-sequence data or FIN that we will process.
        size_t rcv_datalen;
        bool rcv_fin;
        
        // Fast path is that recevied segment is in sequence and there
        // is no out-of-sequence data or FIN buffered.
        if (AMBRO_LIKELY(data_offset == 0 && pcb->num_ooseq == 0 &&
                         !pcb->hasFlag(PcbFlags::OOSEQ_FIN))) {
            // Processing the in-sequence segment.
            rcv_datalen = tcp_data.tot_len;
            rcv_fin = seg_fin;
            
            // Copy any received data into the receive buffer, shifting it.
            if (rcv_datalen > 0) {
                AMBRO_ASSERT(pcb->con != nullptr)
                pcb->con->m_rcv_buf.giveBuf(tcp_data);
            }
        } else {
            // Remember information about out-of-sequence data and FIN.
            bool update_ok = update_ooseq(pcb, eff_seq, tcp_data.tot_len, seg_fin);
            
            // If there was an inconsistency, abort.
            if (AMBRO_UNLIKELY(!update_ok)) {
                TcpProto::pcb_abort(pcb, true);
                return false;
            }
            
            // Copy any received data into the receive buffer.
            if (tcp_data.tot_len > 0) {
                AMBRO_ASSERT(pcb->con != nullptr)
                IpBufRef dst_buf = pcb->con->m_rcv_buf;
                dst_buf.skipBytes(data_offset);
                dst_buf.giveBuf(tcp_data);
            }
            
            // Get data or FIN from the out-of-sequence buffer.
            pop_ooseq(pcb, rcv_datalen, rcv_fin);
            
            // If we got any data out of OOS buffering here then the receive buffer
            // can necessarily be shifted by that much. This is because the data was
            // written into the receive buffer when it was received, and the only
            // concern is if the connection was then abandoned. But in that case we
            // would not get any more data out since we have not put any more data
            // in and we always consume all available data after adding some.
            AMBRO_ASSERT(pcb->rcvBufLen() >= rcv_datalen)
            
            // Shift any processed data out of the receive buffer.
            if (rcv_datalen > 0) {
                AMBRO_ASSERT(pcb->con != nullptr)
                pcb->con->m_rcv_buf.skipBytes(rcv_datalen);
            }
        }
        
        // Accept anything newly received.
        if (rcv_datalen > 0 || rcv_fin) {
            // Compute the amount of processed sequence numbers.
            SeqType rcv_seqlen = rcv_datalen + rcv_fin;
            AMBRO_ASSERT(rcv_seqlen <= pcb->rcv_wnd)
            
            // Adjust rcv_nxt and rcv_wnd due to newly received data.
            SeqType old_rcv_nxt = pcb->rcv_nxt;
            pcb->rcv_nxt = seq_add(pcb->rcv_nxt, rcv_seqlen);
            pcb->rcv_wnd -= rcv_seqlen;
            
            // Make sure we're not leaving rcv_ann behind rcv_nxt.
            // This can happen when the peer sends data before receiving
            // a window update permitting that.
            if (rcv_seqlen > seq_diff(pcb->rcv_ann, old_rcv_nxt)) {
                pcb->rcv_ann = pcb->rcv_nxt;
            }
            
            // Send an ACK later.
            pcb->setFlag(PcbFlags::ACK_PENDING);
            
            if (AMBRO_UNLIKELY(rcv_fin)) {
                // Make appropriate state transitions due to receiving a FIN.
                if (pcb->state == TcpState::ESTABLISHED) {
                    pcb->state = TcpState::CLOSE_WAIT;
                }
                else if (pcb->state == TcpState::FIN_WAIT_1) {
                    pcb->state = TcpState::CLOSING;
                }
                else {
                    AMBRO_ASSERT(pcb->state == TcpState::FIN_WAIT_2)
                    // Go to FIN_WAIT_2_TIME_WAIT and below continue to TIME_WAIT.
                    // This way we inhibit any cb_rcv_buf_extended processing from
                    // the dataReceived callback below.
                    pcb->state = TcpState::FIN_WAIT_2_TIME_WAIT;
                }
            }
            // It may be possible to enlarge rcv_wnd as it may have been bounded to MaxRcvWnd.
            else if (AMBRO_UNLIKELY(pcb->rcv_wnd < pcb->rcvBufLen())) {
                // The if is redundant but reduces overhead since this is needed rarely.
                pcb_update_rcv_wnd(pcb);
            }
            
            if (rcv_datalen > 0) {
                // Give any data to the user.
                if (AMBRO_UNLIKELY(!TcpProto::pcb_event(pcb, [&](auto con) { con->data_received(rcv_datalen); }))) {
                    return false;
                }
                // Possible transitions in callback (except to CLOSED):
                // - ESTABLISHED->FIN_WAIT_1
                // - CLOSE_WAIT->LAST_ACK
            }
            
            if (AMBRO_UNLIKELY(rcv_fin)) {
                // Tell TcpConnection and application about end received.
                if (AMBRO_UNLIKELY(!TcpProto::pcb_event(pcb, [&](auto con) { con->end_received(); }))) {
                    return false;
                }
                // Possible transitions in callback (except to CLOSED):
                // - CLOSE_WAIT->LAST_ACK
                
                // Complete transition from FIN_WAIT_2 to TIME_WAIT.
                if (pcb->state == TcpState::FIN_WAIT_2_TIME_WAIT) {
                    TcpProto::pcb_go_to_time_wait(pcb);
                }
            }
        }
        
        return true;
    }
    
    // Updates out-of-sequence information due to arrival of a new segment.
    // The segment is assumed to fit within the receive window, this is assured
    // in pcb_input_core before calling this.
    static bool update_ooseq (TcpPcb *pcb, SeqType seg_start, size_t seg_datalen, bool seg_fin)
    {
        AMBRO_ASSERT(pcb->num_ooseq <= TcpProto::NumOosSegs)
        
        // Calculate sequence number for end of data.
        SeqType seg_end = seq_add(seg_start, seg_datalen);
        
        // Check for FIN-related inconsistencies.
        if (pcb->hasFlag(PcbFlags::OOSEQ_FIN)) {
            if (seg_datalen > 0 && !seq_lte(seg_end, pcb->ooseq_fin, pcb->rcv_nxt)) {
                return false;
            }
            if (seg_fin && seg_end != pcb->ooseq_fin) {
                return false;
            }
        } else {
            if (seg_fin && pcb->num_ooseq > 0 &&
                !seq_lte(pcb->ooseq[pcb->num_ooseq-1].end, seg_end, pcb->rcv_nxt)) {
                return false;
            }
        }
        
        // If the new segment has any data, update the ooseq segments.
        if (seg_datalen > 0) {
            // Skip over segments strictly before this one.
            uint8_t pos = 0;
            while (pos < pcb->num_ooseq && seq_lt(pcb->ooseq[pos].end, seg_start, pcb->rcv_nxt)) {
                pos++;
            }
            
            // If there are no more segments or the segment [pos] is strictly
            // after the new segment, we insert a new segment here. Otherwise
            // the new segment intersects or touches [pos] and we merge the
            // new segment with [pos] and possibly subsequent segments.
            if (pos >= pcb->num_ooseq || seq_lt(seg_end, pcb->ooseq[pos].start, pcb->rcv_nxt)) {
                // If all segment slots are used and we are not inserting to the end,
                // release the last slot. This ensures that we can always accept
                // in-sequence data, and not stall after all slots are exhausted.
                if (pcb->num_ooseq >= TcpProto::NumOosSegs && pos < TcpProto::NumOosSegs) {
                    pcb->num_ooseq--;
                }
                
                // Try to insert a segment to this spot.
                if (pcb->num_ooseq < TcpProto::NumOosSegs) {
                    if (pos < pcb->num_ooseq) {
                        ::memmove(&pcb->ooseq[pos+1], &pcb->ooseq[pos], (pcb->num_ooseq - pos) * sizeof(OosSeg));
                    }
                    pcb->ooseq[pos] = OosSeg{seg_start, seg_end};
                    pcb->num_ooseq++;
                }
            } else {
                // Extend the existing segment to the left if needed.
                if (seq_lt(seg_start, pcb->ooseq[pos].start, pcb->rcv_nxt)) {
                    pcb->ooseq[pos].start = seg_start;
                }
                
                // Extend the existing segment to the right if needed.
                if (!seq_lte(seg_end, pcb->ooseq[pos].end, pcb->rcv_nxt)) {
                    pcb->ooseq[pos].end = seg_end;
                    
                    // Merge the extended segment [pos] with any subsequent segments
                    // that it now intersects or touches.
                    uint8_t merge_pos = pos+1;
                    while (merge_pos < pcb->num_ooseq && !seq_lt(pcb->ooseq[pos].end, pcb->ooseq[merge_pos].start, pcb->rcv_nxt)) {
                        if (seq_lte(pcb->ooseq[pos].end, pcb->ooseq[merge_pos].end, pcb->rcv_nxt)) {
                            pcb->ooseq[pos].end = pcb->ooseq[merge_pos].end;
                            merge_pos++;
                            break;
                        }
                        merge_pos++;
                    }
                    
                    // If we merged [pos] with any subsequent segments, move any
                    // remaining segments left over those merged segments and adjust
                    // num_ooseq.
                    uint8_t num_merged = merge_pos - (pos+1);
                    if (num_merged > 0) {
                        if (merge_pos < pcb->num_ooseq) {
                            ::memmove(&pcb->ooseq[pos+1], &pcb->ooseq[merge_pos], (pcb->num_ooseq - merge_pos) * sizeof(OosSeg));
                        }
                        pcb->num_ooseq -= num_merged;
                    }
                }
            }
        }
        
        // If the segment has a FIN, remember it.
        if (seg_fin) {
            pcb->setFlag(PcbFlags::OOSEQ_FIN);
            pcb->ooseq_fin = seg_end;
        }
        
        return true;
    }
    
    static void pop_ooseq (TcpPcb *pcb, size_t &datalen, bool &fin)
    {
        // Consume out-of-sequence data.
        if (pcb->num_ooseq > 0 && pcb->ooseq[0].start == pcb->rcv_nxt) {
            AMBRO_ASSERT(pcb->num_ooseq == 1 || !seq_lte(pcb->ooseq[1].start, pcb->ooseq[0].end, pcb->rcv_nxt))
            datalen = seq_diff(pcb->ooseq[0].end, pcb->ooseq[0].start);
            if (pcb->num_ooseq > 1) {
                ::memmove(&pcb->ooseq[0], &pcb->ooseq[1], (pcb->num_ooseq - 1) * sizeof(OosSeg));
            }
            pcb->num_ooseq--;
        } else {
            datalen = 0;
        }
        
        // Get out-of-sequence FIN (no need to consume it).
        fin = pcb->hasFlag(PcbFlags::OOSEQ_FIN) && pcb->ooseq_fin == seq_add(pcb->rcv_nxt, datalen);
    }
};

#include <aprinter/EndNamespace.h>

#endif
