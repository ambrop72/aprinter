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
#include <limits.h>

#include <aprinter/meta/MinMax.h>
#include <aprinter/base/Preprocessor.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/OneOf.h>
#include <aprinter/base/Hints.h>
#include <aprinter/ipstack/Buf.h>
#include <aprinter/ipstack/Chksum.h>
#include <aprinter/ipstack/proto/Tcp4Proto.h>
#include <aprinter/ipstack/proto/TcpUtils.h>

#include <aprinter/BeginNamespace.h>

template <typename TcpProto>
class IpTcpProto_input
{
    APRINTER_USE_TYPES2(TcpUtils, (FlagsType, SeqType, TcpState, TcpSegMeta, TcpOptions))
    APRINTER_USE_VALS(TcpUtils, (seq_add, seq_diff, seq_lte, seq_lt, tcplen,
                                 can_output_in_state, accepting_data_in_state))
    APRINTER_USE_TYPES1(TcpProto, (Context, Ip4DgramMeta, TcpListener, TcpPcb, PcbFlags, Output))
    APRINTER_USE_VALS(TcpProto, (pcb_has_flag, pcb_set_flag, pcb_clear_flag))
    
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
                return listen_input(tcp, lis, ip_meta, tcp_meta, tcp_data.tot_len);
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
    
    static void pcb_extend_receive_window (TcpProto *tcp, TcpPcb *pcb, SeqType wnd_ext, bool force_wnd_update)
    {
        AMBRO_ASSERT(pcb->state != TcpState::CLOSED)
        
        // Extend the receive window.
        pcb->rcv_wnd += wnd_ext;
        
        // Don't do anything else if we're in the connectionAborted callback.
        if (pcb_has_flag(pcb, PcbFlags::ABORTING)) {
            return;
        }
        
        // If we already received a FIN, there's no need to send window updates.
        if (!accepting_data_in_state(pcb->state)) {
            return;
        }
        
        // Force window update if the threshold has exceeded.
        if (pcb_get_wnd_ann_incr(pcb) >= pcb->rcv_ann_thres) {
            force_wnd_update = true;
        }
        
        // Generate a window update if needed.
        if (force_wnd_update) {
            Output::pcb_need_ack(tcp, pcb);
        }
    }
    
private:
    static void listen_input (TcpProto *tcp, TcpListener *lis, Ip4DgramMeta const &ip_meta,
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
            TcpPcb *pcb = tcp->allocate_pcb();
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
            pcb->snd_buf = IpBufRef::NullRef();
            pcb->snd_buf_cur = IpBufRef::NullRef();
            pcb->snd_psh_index = 0;
            pcb->snd_mss = snd_mss;
            
            // These will be initialized at transition to ESTABLISHED:
            // snd_wnd, snd_wl1, snd_wl2
            
            // Increment the listener's PCB count.
            AMBRO_ASSERT(lis->m_num_pcbs < INT_MAX)
            lis->m_num_pcbs++;
            
            // Start the SYN_RCVD abort timeout.
            pcb->abrt_timer.appendAfter(Context(), TcpProto::SynRcvdTimeoutTicks);
            
            // Reply with a SYN+ACK.
            Output::pcb_send_syn_ack(tcp, pcb);
            return;
        } while (false);
        
    refuse:
        // Refuse connection by RST.
        Output::send_rst_reply(tcp, ip_meta, tcp_meta, tcp_data_len);
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
            pcb_input_core(tcp, pcb, tcp_meta, tcp_data);
            tcp->m_current_pcb = nullptr;
            return true;
        }
        return false;
    }
    
    static void pcb_input_core (TcpProto *tcp, TcpPcb *pcb, TcpSegMeta const &tcp_meta, IpBufRef tcp_data)
    {
        AMBRO_ASSERT(pcb->state != OneOf(TcpState::CLOSED, TcpState::SYN_SENT))
        AMBRO_ASSERT(tcp->m_current_pcb == pcb)
        
        // Sequence length of segment (data+flags).
        size_t len = tcplen(tcp_meta.flags, tcp_data.tot_len);
        // Right edge of receive window.
        SeqType nxt_wnd = seq_add(pcb->rcv_nxt, pcb->rcv_wnd);
        
        // Handle uncommon flags.
        if (AMBRO_UNLIKELY((tcp_meta.flags & (Tcp4FlagRst|Tcp4FlagSyn|Tcp4FlagAck)) != Tcp4FlagAck)) {
            if ((tcp_meta.flags & Tcp4FlagRst) != 0) {
                // RST, handle as per RFC 5961.
                if (tcp_meta.seq_num == pcb->rcv_nxt) {
                    tcp->pcb_abort(pcb, false);
                }
                else if (seq_lte(tcp_meta.seq_num, nxt_wnd, pcb->rcv_nxt)) {
                    // We're slightly violating RFC 5961 by allowing seq_num at nxt_wnd.
                    Output::pcb_send_empty_ack(tcp, pcb);
                }
            }
            else if ((tcp_meta.flags & Tcp4FlagSyn) != 0) {
                // SYN, handle as per RFC 5961.
                if (pcb->state == TcpState::SYN_RCVD &&
                    tcp_meta.seq_num == seq_add(pcb->rcv_nxt, -1))
                {
                    // This seems to be a retransmission of the SYN, retransmit our
                    // SYN+ACK and bump the abort timeout.
                    Output::pcb_send_syn_ack(tcp, pcb);
                    pcb->abrt_timer.appendAfter(Context(), TcpProto::SynRcvdTimeoutTicks);
                } else {
                    Output::pcb_send_empty_ack(tcp, pcb);
                }
            }
            else {
                // Segment without none of RST, SYN and ACK should never be sent.
                // Just drop it here. Note that RFC 793 would have us check the
                // sequence number and possibly send an empty ACK if the segment
                // is outside the window, but we don't do that for perfomance.
            }
            return;
        }
        
        // Determine acceptability of segment.
        bool acceptable;
        bool left_edge_in_window;
        bool right_edge_in_window;
        if (len == 0) {
            // Empty segment is acceptable if the sequence number is within or at
            // the right edge of the receive window. Allowing the latter with
            // nonzero receive window violates RFC 793, but seems to make sense.
            acceptable = seq_lte(tcp_meta.seq_num, nxt_wnd, pcb->rcv_nxt);
        } else {
            // Nonzero-length segment is acceptable if its left or right edge
            // is within the receive window. Except for SYN_RCVD, we are not expecting
            // any data to the left.
            SeqType last_seq = seq_add(tcp_meta.seq_num, seq_add(len, -1));
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
            return Output::pcb_send_empty_ack(tcp, pcb);
        }
        
        // Trim the segment on the left or right so that it fits into the receive window.
        SeqType eff_seq = tcp_meta.seq_num;
        FlagsType eff_flags = tcp_meta.flags;
        if (AMBRO_LIKELY(len > 0)) {
            SeqType left_keep;
            if (AMBRO_UNLIKELY(!left_edge_in_window)) {
                // The segment contains some already received data (seq_num < rcv_nxt).
                SeqType left_trim = seq_diff(pcb->rcv_nxt, tcp_meta.seq_num);
                AMBRO_ASSERT(left_trim > 0)   // because !left_edge_in_window
                AMBRO_ASSERT(left_trim < len) // because right_edge_in_window
                eff_seq = pcb->rcv_nxt;
                len -= left_trim;
                // No change to eff_flags: for SYN we'd have bailed out earlier,
                // and FIN could not be trimmed because left_trim < len.
                tcp_data.skipBytes(left_trim);
            }
            else if (AMBRO_UNLIKELY(len > (left_keep = seq_diff(nxt_wnd, tcp_meta.seq_num)))) {
                // The segment contains some extra data beyond the receive window.
                AMBRO_ASSERT(left_keep > 0)   // because left_edge_in_window
                AMBRO_ASSERT(left_keep < len) // because of condition
                len = left_keep;
                eff_flags &= ~Tcp4FlagFin; // a FIN would be outside the window
                tcp_data.tot_len = left_keep;
            }
        }
        
        // If after trimming the segment does not start at the left edge of the 
        // receive window, drop it but send an ACK. The spec does not permit us
        // to further process such an out-of-sequence segment at this time.
        // In the future we may implement queuing.
        if (AMBRO_UNLIKELY(eff_seq != pcb->rcv_nxt)) {
            return Output::pcb_send_empty_ack(tcp, pcb);
        }
        
        // Check ACK validity as per RFC 5961.
        // For this arithemtic to work we're relying on snd_nxt not wrapping around
        // over snd_una-MaxAckBefore. Currently this cannot happen because we don't
        // support window scaling so snd_nxt-snd_una cannot even exceed 2^16-1.
        SeqType past_ack_num = seq_diff(pcb->snd_una, TcpProto::MaxAckBefore);
        bool valid_ack = seq_lte(tcp_meta.ack_num, pcb->snd_nxt, past_ack_num);
        if (AMBRO_UNLIKELY(!valid_ack)) {
            return Output::pcb_send_empty_ack(tcp, pcb);
        }
        
        // Check if the ACK acknowledges anything new.
        bool new_ack = !seq_lte(tcp_meta.ack_num, pcb->snd_una, past_ack_num);
        
        // Bump the last-time of the PCB.
        pcb->last_time = TcpProto::Clock::getTime(Context());
        
        if (AMBRO_UNLIKELY(pcb->state == TcpState::SYN_RCVD)) {
            // If our SYN is not acknowledged, send RST and drop.
            // RFC 793 seems to allow ack_num==snd_una which doesn't make sense.
            if (!new_ack) {
                Output::send_rst(tcp, pcb->local_addr, pcb->remote_addr,
                                 pcb->local_port, pcb->remote_port,
                                 tcp_meta.ack_num, false, 0);
                return;
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
            lis->m_accept_pcb = pcb;
            lis->m_callback->connectionEstablished(lis);
            
            // Not referencing the listener after this, it might have been deinited from
            // the callback. In any case we will not leave lis->m_accept_pcb pointing
            // to this PCB, that is assured by acceptConnection or pcb_abort below.
            
            // Handle abort of PCB.
            if (AMBRO_UNLIKELY(tcp->m_current_pcb == nullptr)) {
                return;
            }
            
            // Possible transitions in callback (except to CLOSED):
            // - ESTABLISHED->FIN_WAIT_1
            
            // If the connection has not been accepted or has been accepted but
            // already abandoned, abort with RST.
            if (AMBRO_UNLIKELY(pcb->con == nullptr)) {
                return tcp->pcb_abort(pcb, true);
            }
        } else {
            // Handle new acknowledgments.
            if (new_ack) {
                // We can only get here if there was anything pending acknowledgement.
                AMBRO_ASSERT(can_output_in_state(pcb->state))
                
                // Calculate the amount of acknowledged sequence counts.
                // This can be data or FIN (but not SYN, as SYN_RCVD state is handled above).
                SeqType acked = seq_diff(tcp_meta.ack_num, pcb->snd_una);
                AMBRO_ASSERT(acked > 0)
                
                // Update snd_una due to sequences having been ACKed.
                pcb->snd_una = tcp_meta.ack_num;
                
                // Because snd_wnd is relative to snd_una, a matching decrease of snd_wnd
                // is neeeded.
                if (AMBRO_LIKELY(acked <= pcb->snd_wnd)) {
                    pcb->snd_wnd -= acked;
                } else {
                    // More has been ACKed than the send window. As snd_wnd annot be made
                    // negative, this is effectively an increase of the send window.
                    // This warrants a pcb_output but we're setting that up anyway next.
                    pcb->snd_wnd = 0;
                }
                
                // Clear the retransmission timeout and schedule pcb_output to
                // be done shortly, which will res-set the timeout as needed.
                pcb->rtx_timer.unset(Context());
                pcb_set_flag(pcb, PcbFlags::OUT_PENDING);
                
                // Check if our FIN has just been acked.
                bool fin_acked = Output::pcb_fin_acked(pcb);
                
                // Calculate the amount of acknowledged data (without ACK of FIN).
                SeqType data_acked_seq = acked - (SeqType)fin_acked;
                AMBRO_ASSERT(data_acked_seq <= SIZE_MAX)
                size_t data_acked = data_acked_seq;
                
                if (data_acked > 0) {
                    AMBRO_ASSERT(data_acked <= pcb->snd_buf.tot_len)
                    AMBRO_ASSERT(pcb->snd_buf_cur.tot_len <= pcb->snd_buf.tot_len)
                    AMBRO_ASSERT(pcb->snd_psh_index <= pcb->snd_buf.tot_len)
                    
                    // Advance the send buffer.
                    size_t cur_offset = Output::pcb_snd_cur_offset(pcb);
                    if (data_acked >= cur_offset) {
                        pcb->snd_buf_cur.skipBytes(data_acked - cur_offset);
                        pcb->snd_buf = pcb->snd_buf_cur;
                    } else {
                        pcb->snd_buf.skipBytes(data_acked);
                    }
                    
                    // Adjust the push index.
                    pcb->snd_psh_index -= MinValue(pcb->snd_psh_index, data_acked);
                    
                    // Report data-sent event to the user.
                    if (!tcp->pcb_callback(pcb, [&](auto cb) { cb->dataSent(data_acked); })) {
                        return;
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
                    
                    // Report end-sent event to the user.
                    if (!tcp->pcb_callback(pcb, [&](auto cb) { cb->endSent(); })) {
                        return;
                    }
                    // Possible transitions in callback (except to CLOSED): none.
                    
                    // In states where the endReceived has already been called
                    // (CLOSING and LAST_ACK), we need to now disassociate the PCB
                    // and the TcpConnection, using pcb_unlink_con.
                    
                    if (pcb->state == TcpState::FIN_WAIT_1) {
                        // FIN is accked in FIN_WAIT_1, transition to FIN_WAIT_2.
                        pcb->state = TcpState::FIN_WAIT_2;
                        // At this transition output_timer and rtx_timer must
                        // be unset due to assert in their handlers.
                        pcb->output_timer.unset(Context());
                        // rtx_timer was unset above already.
                    }
                    else if (pcb->state == TcpState::CLOSING) {
                        // Transition to TIME_WAIT; pcb_unlink_con is done by pcb_go_to_time_wait.
                        // Further processing below is not applicable so return here.
                        return TcpProto::pcb_go_to_time_wait(pcb);
                    }
                    else {
                        AMBRO_ASSERT(pcb->state == TcpState::LAST_ACK)
                        // Close the PCB; do pcb_unlink_con first to prevent connectionAborted.
                        TcpProto::pcb_unlink_con(pcb); 
                        return tcp->pcb_abort(pcb, false);
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
                SeqType wnd_seq_ref = seq_add(nxt_wnd, 1);
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
                    
                    // If the window has increased, pcb_output is warranted.
                    if (pcb->snd_wnd > old_snd_wnd) {
                        pcb_set_flag(pcb, PcbFlags::OUT_PENDING);
                    }
                }
            }
        }
        
        if (AMBRO_LIKELY(accepting_data_in_state(pcb->state))) {
            // Any data or flags that don't fit in the receive window would have
            // been trimmed already. It's true that rcv_wnd might have been changed
            // from callbacks but it could only have been made larger.
            AMBRO_ASSERT(len <= pcb->rcv_wnd)
            
            // Check if we have just recevied a FIN.
            bool fin_recved = (eff_flags & Tcp4FlagFin) != 0;
            
            // This assert holds because of how "len" was initially calculated
            // and any trimming of the segment preserves this.
            AMBRO_ASSERT(tcp_data.tot_len + (size_t)fin_recved == len)
            
            // Accept anything newly received.
            if (len > 0) {
                // Adjust rcv_nxt and rcv_wnd due to newly received data.
                SeqType old_rcv_nxt = pcb->rcv_nxt;
                pcb->rcv_nxt = seq_add(pcb->rcv_nxt, len);
                pcb->rcv_wnd -= len;
                
                // Make sure we're not leaving rcv_ann behind rcv_nxt.
                // This can happen when the peer sends data before receiving
                // a window update permitting that.
                if (len > seq_diff(pcb->rcv_ann, old_rcv_nxt)) {
                    pcb->rcv_ann = pcb->rcv_nxt;
                }
                
                // Send an ACK (below).
                pcb_set_flag(pcb, PcbFlags::ACK_PENDING);
                
                // If a FIN was received, make appropriate state transitions, except the
                // FIN_WAIT_2->TIME_WAIT transition which is done after the callbacks.
                if (AMBRO_UNLIKELY(fin_recved)) {
                    if (pcb->state == TcpState::ESTABLISHED) {
                        pcb->state = TcpState::CLOSE_WAIT;
                    }
                    else if (pcb->state == TcpState::FIN_WAIT_1) {
                        pcb->state = TcpState::CLOSING;
                    }
                }
                
                // Reveived any actual data?
                if (tcp_data.tot_len > 0) {
                    // If the user has abandoned the connection, abort with RST.
                    if (pcb->con == nullptr) {
                        return tcp->pcb_abort(pcb, true);
                    }
                    
                    // Give any the data to the user.
                    if (!tcp->pcb_callback(pcb, [&](auto cb) { cb->dataReceived(tcp_data); })) {
                        return;
                    }
                    // Possible transitions in callback (except to CLOSED):
                    // - ESTABLISHED->FIN_WAIT_1
                    // - CLOSE_WAIT->LAST_ACK
                }
                
                if (AMBRO_UNLIKELY(fin_recved)) {
                    // Report end-received event to the user.
                    if (!tcp->pcb_callback(pcb, [&](auto cb) { cb->endReceived(); })) {
                        return;
                    }
                    // Possible transitions in callback (except to CLOSED):
                    // - CLOSE_WAIT->LAST_ACK
                    
                    // In FIN_WAIT_2 state we need to transition the PCB to TIME_WAIT.
                    // This includes pcb_unlink_con since both endSent and endReceived
                    // callbacks have now been called.
                    if (pcb->state == TcpState::FIN_WAIT_2) {
                        TcpProto::pcb_go_to_time_wait(pcb);
                    }
                }
            }
        }
        else if (pcb->state == TcpState::TIME_WAIT) {
            // Reply with an ACK, and restart the timeout.
            pcb_set_flag(pcb, PcbFlags::ACK_PENDING);
            pcb->abrt_timer.appendAfter(Context(), TcpProto::TimeWaitTimeTicks);
        }
        
        // Try to output if desired.
        if (pcb_has_flag(pcb, PcbFlags::OUT_PENDING)) {
            pcb_clear_flag(pcb, PcbFlags::OUT_PENDING);
            if (can_output_in_state(pcb->state)) {
                bool sent_something = Output::pcb_output(tcp, pcb);
                if (sent_something) {
                    pcb_clear_flag(pcb, PcbFlags::ACK_PENDING);
                }
            }
        }
        
        // Send an empty ACK if desired.
        // Note, ACK_PENDING will have been cleared above if pcb_output sent anything,
        // in that case we don't need an empty ACK here.
        if (pcb_has_flag(pcb, PcbFlags::ACK_PENDING)) {
            pcb_clear_flag(pcb, PcbFlags::ACK_PENDING);
            Output::pcb_send_empty_ack(tcp, pcb);
        }
    }
};

#include <aprinter/EndNamespace.h>

#endif
