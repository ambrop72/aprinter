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
#include <aprinter/base/BinaryTools.h>

#include <aipstack/misc/Buf.h>
#include <aipstack/misc/Chksum.h>
#include <aipstack/proto/Ip4Proto.h>
#include <aipstack/proto/Tcp4Proto.h>
#include <aipstack/proto/Icmp4Proto.h>
#include <aipstack/ip/IpStack.h>
#include <aipstack/tcp/TcpUtils.h>

#include <aipstack/BeginNamespace.h>

template <typename TcpProto>
class IpTcpProto_input
{
    APRINTER_USE_TYPES1(TcpUtils, (FlagsType, SeqType, TcpState, TcpSegMeta, TcpOptions,
                                   OptionFlags, PortType))
    APRINTER_USE_VALS(TcpUtils, (seq_add, seq_diff, seq_lte, seq_lt2, tcplen,
                                 can_output_in_state, accepting_data_in_state,
                                 state_is_synsent_synrcvd, snd_open_in_state))
    APRINTER_USE_TYPES1(TcpProto, (Context, Ip4DgramMeta, TcpListener, TcpConnection,
                                   TcpPcb, PcbFlags, Output, Constants,
                                   AbrtTimer, RtxTimer, OutputTimer, TheIpStack))
    APRINTER_USE_VALS(TcpProto, (pcb_aborted_in_callback))
    APRINTER_USE_ONEOF
    
public:
    static void recvIp4Dgram (TcpProto *tcp, Ip4DgramMeta const &ip_meta, IpBufRef dgram)
    {
        // The destination address must be the address of the incoming interface.
        if (AMBRO_UNLIKELY(!ip_meta.iface->ip4AddrIsLocalAddr(ip_meta.dst_addr))) {
            return;
        }
        
        // Check header size, must fit in first buffer.
        if (AMBRO_UNLIKELY(!dgram.hasHeader(Tcp4Header::Size))) {
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
        
        // Check TCP checksum.
        IpChksumAccumulator chksum_accum;
        chksum_accum.addWords(&ip_meta.src_addr.data);
        chksum_accum.addWords(&ip_meta.dst_addr.data);
        chksum_accum.addWord(APrinter::WrapType<uint16_t>(), Ip4ProtocolTcp);
        chksum_accum.addWord(APrinter::WrapType<uint16_t>(), dgram.tot_len);
        if (AMBRO_UNLIKELY(chksum_accum.getChksum(dgram) != 0)) {
            return;
        }
        
        // Get a buffer reference starting at the option data.
        IpBufRef tcp_data = dgram.hideHeader(Tcp4Header::Size);
        
        // Get the data offset and calculate many bytes of options there options are.
        // Note that if data_offset is less than Tcp4Header::Size, the difference
        // will wrap around which the next check relies on.
        size_t data_offset = (tcp_meta.flags >> TcpOffsetShift) * 4;
        size_t opts_len = data_offset - Tcp4Header::Size;

        // Check that data offset is within [Tcp4Header::Size, dgram.tot_len].
        // The former bound is checked indirectly since opts_len would have
        // wrapped around.
        if (AMBRO_UNLIKELY(opts_len > tcp_data.tot_len)) {
            return;
        }
        
        // Remember the options region and skip over the options.
        // The options will only be parsed when they are needed,
        // using parse_received_opts.
        tcp->m_received_opts_buf = tcp_data.subTo(opts_len);
        tcp_data.skipBytes(opts_len);
        
        // Try to handle using a PCB.
        TcpPcb *pcb = tcp->find_pcb({ip_meta.dst_addr, ip_meta.src_addr,
                                     tcp_meta.local_port, tcp_meta.remote_port});
        if (AMBRO_LIKELY(pcb != nullptr)) {
            AMBRO_ASSERT(tcp->m_current_pcb == nullptr)
            tcp->m_current_pcb = pcb;
            pcb_input(pcb, tcp_meta, tcp_data);
            tcp->m_current_pcb = nullptr;
            return;
        }
        
        // Sanity check source address - reject broadcast addresses.
        // We do this after looking up the PCB for performance, since
        // the PCBs already have sanity checked addresses. There is a
        // minor detail that the original check might have been against
        // a different subnet broadcast address but we prefer speed to
        // completeness of this check.
        if (AMBRO_UNLIKELY(!TheIpStack::checkUnicastSrcAddr(ip_meta))) {
            return;
        }
        
        // Try to handle using a listener.
        TcpListener *lis = tcp->find_listener_for_rx(ip_meta.dst_addr, tcp_meta.local_port);
        if (lis != nullptr) {
            return listen_input(lis, ip_meta, tcp_meta, tcp_data.tot_len);
        }
        
        // Reply with RST, unless this is an RST.
        if ((tcp_meta.flags & Tcp4FlagRst) == 0) {
            Output::send_rst_reply(tcp, ip_meta, tcp_meta, tcp_data.tot_len);
        }
    }
    
    static void handleIp4DestUnreach (TcpProto *tcp, Ip4DestUnreachMeta const &du_meta,
                                Ip4DgramMeta const &ip_meta, IpBufRef const &dgram_initial)
    {
        // We only care about ICMP code "fragmentation needed and DF set".
        if (du_meta.icmp_code != Icmp4CodeDestUnreachFragNeeded) {
            return;
        }
        
        // Check that at least the first 8 bytes of the TCP header
        // are available. This gives us SrcPort, DstPort and SeqNum.
        if (!dgram_initial.hasHeader(8)) {
            return;
        }
        
        // Read header fields.
        // NOTE: No other header fields must be read, that would
        // be out-of-bound memory access!
        auto tcp_header = Tcp4Header::MakeRef(dgram_initial.getChunkPtr());
        PortType local_port  = tcp_header.get(Tcp4Header::SrcPort());
        PortType remote_port = tcp_header.get(Tcp4Header::DstPort());
        SeqType seq_num      = tcp_header.get(Tcp4Header::SeqNum());
        
        // Look for a PCB associated with these addresses.
        TcpPcb *pcb = tcp->find_pcb({ip_meta.src_addr, ip_meta.dst_addr,
                                     local_port, remote_port});
        if (pcb == nullptr) {
            return;
        }
        
        // Check that the PCB state is one where output is possible and that the
        // received sequence number is between snd_una and snd_nxt inclusive.
        if (!can_output_in_state(pcb->state) ||
            !seq_lte(seq_num, pcb->snd_nxt, pcb->snd_una))
        {
            return;
        }
        
        // If the PCB has been abandoned, ignore.
        if (pcb->con == nullptr) {
            return;
        }
        
        // Read the field of the ICMP message where the next-hop MTU
        // is supposed to be.
        uint16_t mtu_info = Icmp4GetMtuFromRest(du_meta.icmp_rest);
        
        // Update the PMTU information.
        if (!pcb->tcp->m_stack->handleIcmpPacketTooBig(pcb->remote_addr, mtu_info)) {
            // The PMTU has not been lowered, nothing else to do.
            return;
        }
        
        // Note that the above has called TcpPcb::pmtuChanged -> Output::pcb_pmtu_changed
        // for this PCB (and possibly others), which updated the snd_mss.
        
        // We will retransmit if we have anything unacknowledged data, there
        // is some window available and and the sequence number in the ICMP
        // message is equal to the first unacknowledged sequence number.
        if (Output::pcb_has_snd_unacked(pcb) && pcb->con->m_v.snd_wnd > 0 &&
            seq_num == pcb->snd_una)
        {
            // Requeue everything for retransmission.
            Output::pcb_requeue_everything(pcb);
            
            // Retransmit using pcb_output_active.
            Output::pcb_output_active(pcb, false);
        }
    }
    
    // Get a scaled window size value to be put into the segment being sent.
    static uint16_t pcb_ann_wnd (TcpPcb *pcb)
    {
        // Note that empty ACK segments sent in SYN_SENT state will
        // contain a scaled window value. This is how it should be
        // if reading RFC 1323 literally, and it seems reasonable since
        // if it was an unscaled value it might be misinterpreted as
        // a much larger window than intended.
        
        // Try to announce as much window as is available, even if a
        // window update would otherwise be inhibited due to rcv_ann_thres.
        if (AMBRO_LIKELY(accepting_data_in_state(pcb->state))) {
            // For performance we only update rcv_ann_wnd if the RCV_WND_UPD
            // flag is set and we clear the flag when we do an update. Any
            // state change which may allow advertising more window will set
            // this flag.
            if (pcb->hasAndClearFlag(PcbFlags::RCV_WND_UPD)) {
                SeqType ann_wnd = pcb_calc_wnd_update(pcb);
                if (ann_wnd > pcb->rcv_ann_wnd) {
                    pcb->rcv_ann_wnd = ann_wnd;
                }
            }
        }
        
        // Calculate the window value by right-shifting rcv_ann_thres
        // according to the receive window scaling.
        uint32_t hdr_wnd = pcb->rcv_ann_wnd >> pcb->rcv_wnd_shift;
        
        // This must fit into 16-bits because: invariant
        // - In SYN_SENT/SYN_RCVD, rcv_ann_wnd will itself not exceed
        //   UINT16_MAX, so it will also not after right-shifting.
        // - In other states, rcv_ann_wnd would never be set to more
        //   than the maximum window that can be advertised (see
        //   max_rcv_wnd_ann), and could only be decreased upon
        //   receiving ACKs.
        AMBRO_ASSERT(hdr_wnd <= UINT16_MAX)
        
        return hdr_wnd;
    }
    
    static void pcb_rcv_buf_extended (TcpPcb *pcb)
    {
        AMBRO_ASSERT(pcb->state != OneOf(TcpState::CLOSED, TcpState::SYN_RCVD))
        AMBRO_ASSERT(pcb->con != nullptr)
        
        if (AMBRO_LIKELY(accepting_data_in_state(pcb->state))) {
            // Calculate how much window we could announce.
            SeqType ann_wnd = pcb_calc_wnd_update(pcb);
            
            // If we can announce at least rcv_ann_thres more window than
            // already announced then we need to send an ACK immediately.
            if (ann_wnd >= pcb->rcv_ann_wnd + pcb->con->m_v.rcv_ann_thres) {
                // Update rcv_ann_wnd to the calculated new value and
                // clear the flag to inhibit a redundant recalculation
                // in rcv_ann_wnd.
                pcb->rcv_ann_wnd = ann_wnd;
                pcb->clearFlag(PcbFlags::RCV_WND_UPD);
                
                // Force an ACK.
                Output::pcb_need_ack(pcb);
            }
            // If the window update theshold is not reached but we are still
            // able to announce more window, set the flag to make sure that
            // pcb_ann_wnd updates rcv_ann_wnd when the next segment is sent.
            else if (ann_wnd > pcb->rcv_ann_wnd) {
                pcb->setFlag(PcbFlags::RCV_WND_UPD);
            }
        }
    }
    
    static void pcb_update_rcv_wnd_after_abandoned (TcpPcb *pcb, SeqType rcv_ann_thres)
    {
        AMBRO_ASSERT(accepting_data_in_state(pcb->state))
        
        // This is our heuristic for the window increment.
        SeqType min_window =
            APrinter::MaxValue(rcv_ann_thres, Constants::MinAbandonRcvWndIncr);
        
        // Make sure it fits in size_t (relevant if size_t is 16-bit),
        // to ensure the invariant that rcv_ann_wnd always fits in size_t.
        if (SIZE_MAX < UINT32_MAX) {
            min_window = APrinter::MinValueU(min_window, (size_t)SIZE_MAX);
        }
        
        // Round up to the nearest window that can be advertised.
        SeqType scale_mask = ((SeqType)1 << pcb->rcv_wnd_shift) - 1;
        min_window = (min_window + scale_mask) & ~scale_mask;
        
        // Make sure we do not set rcv_ann_wnd to more than can be announced.
        min_window = APrinter::MinValue(min_window, max_rcv_wnd_ann(pcb));
        
        // Announce more window if needed.
        if (pcb->rcv_ann_wnd < min_window) {
            pcb->rcv_ann_wnd = min_window;
            Output::pcb_need_ack(pcb);
        }
    }
    
    // This is called at transition to ESTABLISHED in order to initialize
    // certain variables which can only be initialized once these is an
    // associated TcpConnection object.
    static void pcb_complete_established_transition (TcpPcb *pcb, uint16_t pmtu)
    {
        AMBRO_ASSERT(pcb->state == TcpState::ESTABLISHED)
        AMBRO_ASSERT(pcb->con != nullptr)
        
        // If this is the end of RTT measurement (there was no retransmission),
        // update the RTT vars and RTO based on the delay. Otherwise just reset RTO
        // to the initial value since it might have been increased in retransmissions.
        if (pcb->hasFlag(PcbFlags::RTT_PENDING)) {
            Output::pcb_end_rtt_measurement(pcb);
        } else {
            pcb->rto = Constants::InitialRtxTime;
        }
        
        // Update snd_mss and IpSendFlags::DontFragmentFlag now that we have an updated
        // base_snd_mss (SYN_SENT) or the mss_ref has been setup (SYN_RCVD).
        pcb->snd_mss = Output::pcb_calc_snd_mss_from_pmtu(pcb, pmtu);
        
        // Read the snd_wnd which was temporarily stuffed to snd_una
        // (pcb_input_syn_sent_rcvd_processing), and restore snd_una
        SeqType snd_wnd = pcb->snd_una;
        pcb->snd_una = pcb->snd_nxt;
        
        // Initialize some variables.
        TcpConnection *con = pcb->con;
        con->m_v.snd_wnd = snd_wnd;
        con->m_v.cwnd = TcpUtils::calc_initial_cwnd(pcb->snd_mss);
        pcb->setFlag(PcbFlags::CWND_INIT);
        con->m_v.ssthresh = Constants::MaxWindow;
        con->m_v.cwnd_acked = 0;
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
            
            // Calculate the MSS based on the interface MTU.
            uint16_t iface_mss = ip_meta.iface->getMtu() - Ip4TcpHeaderSize;
            
            TcpProto *tcp = lis->m_tcp;
            
            // Make sure received options are parsed.
            parse_received_opts(tcp);
            
            // Calculate the base_snd_mss.
            uint16_t base_snd_mss;
            if (!TcpUtils::calc_snd_mss<Constants::MinAllowedMss>(
                    iface_mss, tcp->m_received_opts, &base_snd_mss))
            {
                goto refuse;
            }
            
            // Allocate a PCB.
            TcpPcb *pcb = tcp->allocate_pcb();
            if (pcb == nullptr) {
                goto refuse;
            }
            
            // Generate an initial sequence number.
            SeqType iss = TcpProto::make_iss();
            
            // Initially advertised receive window, at most 16-bit wide since
            // SYN-ACK segments have unscaled window.
            // NOTE: rcv_ann_wnd fits into size_t as required since m_initial_rcv_wnd
            // also does (TcpListener::setInitialReceiveWindow).
            AMBRO_ASSERT(lis->m_initial_rcv_wnd <= SIZE_MAX)
            SeqType rcv_wnd =
                APrinter::MinValueU((uint16_t)UINT16_MAX, lis->m_initial_rcv_wnd);
            
            // Initialize most of the PCB.
            pcb->state = TcpState::SYN_RCVD;
            pcb->flags = 0;
            pcb->lis = lis;
            pcb->local_addr = ip_meta.dst_addr;
            pcb->remote_addr = ip_meta.src_addr;
            pcb->local_port = tcp_meta.local_port;
            pcb->remote_port = tcp_meta.remote_port;
            pcb->rcv_nxt = seq_add(tcp_meta.seq_num, 1);
            pcb->rcv_ann_wnd = rcv_wnd;
            pcb->snd_una = iss;
            pcb->snd_nxt = iss;
            pcb->snd_mss = iface_mss; // store iface_mss here temporarily
            pcb->base_snd_mss = base_snd_mss;
            pcb->rto = Constants::InitialRtxTime;
            pcb->num_dupack = 0;
            pcb->snd_wnd_shift = 0;
            pcb->rcv_wnd_shift = 0;
            
            // Note, the PCB is on the list of unreferenced PCBs and we leave
            // it since SYN_RCVD PCBs are considered unreferenced (except while
            // being accepted).
            
            // Handle window scaling option.
            if ((tcp->m_received_opts.options & OptionFlags::WND_SCALE) != 0) {
                pcb->setFlag(PcbFlags::WND_SCALE);
                pcb->snd_wnd_shift =
                    APrinter::MinValue((uint8_t)14, tcp->m_received_opts.wnd_scale);
                pcb->rcv_wnd_shift = Constants::RcvWndShift;
            }
            
            // Increment the listener's PCB count.
            AMBRO_ASSERT(lis->m_num_pcbs < INT_MAX)
            lis->m_num_pcbs++;
            
            // Add the PCB to the active index.
            tcp->m_pcb_index_active.addEntry(*tcp, {*pcb, *tcp});
            
            // Move the PCB to the front of the unreferenced list.
            tcp->move_unrefed_pcb_to_front(pcb);
            
            // Start the SYN_RCVD abort timeout.
            pcb->tim(AbrtTimer()).appendAfter(Context(), Constants::SynRcvdTimeoutTicks);
            
            // Start the retransmission timer.
            pcb->tim(RtxTimer()).appendAfter(Context(), Output::pcb_rto_time(pcb));
            
            // Reply with a SYN-ACK.
            Output::pcb_send_syn(pcb);
            return;
        } while (false);
        
    refuse:
        // Refuse connection by RST.
        Output::send_rst_reply(lis->m_tcp, ip_meta, tcp_meta, tcp_data_len);
    }
    
    static void pcb_input (TcpPcb *pcb, TcpSegMeta const &tcp_meta, IpBufRef tcp_data)
    {
        AMBRO_ASSERT(pcb->state != TcpState::CLOSED)
        AMBRO_ASSERT(pcb->tcp->m_current_pcb == pcb)
        
        // Remember original data length.
        size_t orig_data_len = tcp_data.tot_len;
        
        // Do basic processing, e.g.:
        // - Handle RST and SYN.
        // - Check acceptability.
        // - Trim segment into window.
        // - Check ACK validity.
        SeqType eff_rel_seq;
        bool seg_fin;
        SeqType acked;
        if (!pcb_input_basic_processing(pcb, tcp_meta, tcp_data, eff_rel_seq,
                                        seg_fin, acked))
        {
            return;
        }
        
        if (AMBRO_UNLIKELY(state_is_synsent_synrcvd(pcb->state))) {
            // Do SYN_SENT or SYN_RCVD specific processing.
            // Normally we transition to ESTABLISHED state here.
            if (!pcb_input_syn_sent_rcvd_processing(pcb, tcp_meta, acked)) {
                return;
            }
            
            // A successful return of pcb_input_syn_sent_rcvd_processing implies
            // that the PCB entered ESTABLISHED state (but may already have changed).
            AMBRO_ASSERT(pcb->state != OneOf(TcpState::SYN_SENT, TcpState::SYN_RCVD))
        } else {
            // Process acknowledgements and window updates.
            if (!pcb_input_ack_wnd_processing(pcb, tcp_meta, acked, orig_data_len)) {
                return;
            }
        }
        
        if (AMBRO_LIKELY(accepting_data_in_state(pcb->state))) {
            // Process received data or FIN.
            if (!pcb_input_rcv_processing(pcb, eff_rel_seq, seg_fin, tcp_data)) {
                return;
            }
        }
        else if (pcb->state == TcpState::TIME_WAIT) {
            // Reply with an ACK and restart the timeout.
            pcb->setFlag(PcbFlags::ACK_PENDING);
            pcb->tim(AbrtTimer()).appendAfter(Context(), Constants::TimeWaitTimeTicks);
        }
        
        // Output if needed.
        if (pcb->hasAndClearFlag(PcbFlags::OUT_PENDING)) {
            // These are implied by the OUT_PENDING flag.
            AMBRO_ASSERT(can_output_in_state(pcb->state))
            AMBRO_ASSERT(Output::pcb_has_snd_outstanding(pcb))
            
            // Output queued data.
            if (Output::pcb_output(pcb, false)) {
                // An ACK was sent along, no need for empty ACK.
                pcb->clearFlag(PcbFlags::ACK_PENDING);
            }
        }
        
        // Send an empty ACK if desired.
        // Note, ACK_PENDING will have been cleared above if pcb_output sent anything,
        // in that case we don't need an empty ACK here.
        if (pcb->hasAndClearFlag(PcbFlags::ACK_PENDING)) {
            Output::pcb_send_empty_ack(pcb);
        }
    }
    
    static bool pcb_input_basic_processing (TcpPcb *pcb, TcpSegMeta const &tcp_meta,
                                            IpBufRef &tcp_data, SeqType &eff_rel_seq,
                                            bool &seg_fin, SeqType &acked)
    {
        // Get the RST, SYN and ACK flags.
        FlagsType rst_syn_ack = tcp_meta.flags & (Tcp4FlagRst|Tcp4FlagSyn|Tcp4FlagAck);
        
        // Handle uncommon flags (RST set, SYN set or ACK not set).
        if (AMBRO_UNLIKELY(rst_syn_ack != Tcp4FlagAck)) {
            if (!pcb_uncommon_flags_processing(pcb, rst_syn_ack, tcp_meta, tcp_data)) {
                return false;
            }
        }
        
        if (AMBRO_UNLIKELY(pcb->state == TcpState::SYN_SENT)) {
            // In SYN_SENT we are only accepting a SYN and we have to ignore
            // any data or FIN (so that pcb_input_rcv_processing does nothing).
            eff_rel_seq = 0;
            tcp_data.tot_len = 0;
            seg_fin = false;
            
            // Check ACK validity for SYN_SENT state (RFC 793 p66).
            // We require that the ACK acknowledges the SYN. We must also
            // check that we have event sent the SYN (snd_nxt).
            if (pcb->snd_nxt == pcb->snd_una || tcp_meta.ack_num != pcb->snd_nxt) {
                Output::send_rst(pcb->tcp, pcb->local_addr, pcb->remote_addr,
                                 pcb->local_port, pcb->remote_port,
                                 tcp_meta.ack_num, false, 0);
                return false;
            }
            
            // The SYN is being acknowledged.
            acked = 1;
        } else {
            // Calculate the right edge of the receive window.
            SeqType rcv_wnd = pcb->rcv_ann_wnd;
            if (AMBRO_LIKELY(pcb->state != TcpState::SYN_RCVD)) {
                SeqType avail_wnd =
                    APrinter::MinValueU(pcb->rcvBufLen(), Constants::MaxWindow);
                rcv_wnd = APrinter::MaxValue(rcv_wnd, avail_wnd);
            }
            
            // Store the sequence number relative to rcv_nxt and FIN flag. But these
            // and also tcp_data will be modified below if the segment is trimmed.
            // At this point eff_rel_seq may be "negative" (very large) but that would
            // be resolved.
            eff_rel_seq = seq_diff(tcp_meta.seq_num, pcb->rcv_nxt);
            seg_fin = (tcp_meta.flags & Tcp4FlagFin) != 0;
            
            // Sequence length of segment (data+FIN). Note that we cannot have
            // a SYN here, we would have bailed out at the top, except in SYN_SENT
            // state which is handled just above.
            size_t seqlen = tcp_data.tot_len + seg_fin;
            
            if (seqlen == 0) {
                // Empty segment is acceptable if the sequence number is within or at
                // the right edge of the receive window. Allowing the latter with
                // nonzero receive window violates RFC 793, but seems to make sense,
                // since such segments may be generated normally when the sender
                // exhausts our receive window and may be useful window updates or
                // ACKs.
                bool acceptable = eff_rel_seq <= rcv_wnd;
                
                // If not acceptable, send any appropriate response and drop.
                if (AMBRO_UNLIKELY(!acceptable)) {
                    Output::pcb_send_empty_ack(pcb);
                    return false;
                }
            } else {
                // Nonzero-length segment is acceptable if its left or right edge
                // is within the receive window. In SYN_RCVD we could be more strict
                // and not allow data before the SYN, but for performance reasons
                // we check that later in pcb_input_syn_sent_rcvd_processing.
                SeqType last_rel_seq = seq_diff(seq_add(eff_rel_seq, seqlen), 1);
                bool left_edge_in_window = eff_rel_seq < rcv_wnd;
                bool right_edge_in_window = last_rel_seq < rcv_wnd;
                
                // Fast path is that the entire segment is in the receive window.
                // Note: if the left and right edge are in the window then it follows
                // the entire segment is; the segment cannot possibly be so long to
                // start in the window, leave it, wrap around and end in the window.
                if (AMBRO_UNLIKELY(!left_edge_in_window || !right_edge_in_window)) {
                    if (left_edge_in_window) {
                        // The segment contains some extra data beyond the receive window,
                        // remove this data from the end.
                        SeqType left_keep = seq_diff(rcv_wnd, eff_rel_seq);
                        AMBRO_ASSERT(left_keep > 0)   // because left_edge_in_window
                        AMBRO_ASSERT(left_keep < seqlen) // because !right_edge_in_window
                        seg_fin = false; // a FIN would be outside the window
                        tcp_data.tot_len = left_keep;
                    }
                    else if (right_edge_in_window) {
                        // The segment contains some already received data
                        // (seq_num < rcv_nxt), remove this data from the front.
                        SeqType left_trim = -eff_rel_seq;
                        AMBRO_ASSERT(left_trim > 0)   // because !left_edge_in_window
                        AMBRO_ASSERT(left_trim < seqlen) // because right_edge_in_window
                        eff_rel_seq = 0;
                        // No change to seg_fin: for SYN we'd have bailed out earlier,
                        // and FIN could not be trimmed because left_trim < seqlen.
                        tcp_data.skipBytes(left_trim);
                    }
                    else {
                        // The segment is completely outside the receive window.
                        // It is unacceptable -> send ACK and stop processing.
                        Output::pcb_send_empty_ack(pcb);
                        return false;
                    }
                }
            }
            
            // Check ACK validity as per RFC 5961.
            SeqType ack_minus_una = seq_diff(tcp_meta.ack_num, pcb->snd_una);
            if (AMBRO_LIKELY(ack_minus_una <= seq_diff(pcb->snd_nxt, pcb->snd_una))) {
                // Fast path: it is not an old ACK.
                acked = ack_minus_una;
            } else {
                // Slow path: it is an old or too new ACK. If it is permissibly
                // old then allow it otherwise just send an empty ACK.
                SeqType una_minus_ack = -ack_minus_una;
                if (AMBRO_UNLIKELY(una_minus_ack > Constants::MaxAckBefore)) {
                    Output::pcb_send_empty_ack(pcb);
                    return false;
                }
                acked = 0;
            }
        }
        
        return true;
    }
    
    static bool pcb_uncommon_flags_processing (TcpPcb *pcb, FlagsType flags_rst_syn_ack,
                                    TcpSegMeta const &tcp_meta, IpBufRef const &tcp_data)
    {
        bool continue_processing = false;
        
        if ((flags_rst_syn_ack & Tcp4FlagRst) != 0) {
            // RST, handle as per RFC 5961.
            if (pcb->state == TcpState::SYN_SENT) {
                // The RFC says the reset is acceptable if it acknowledges
                // the SYN. But due to the possibility that we sent an empty
                // ACK with seq_num==snd_una, accept also ack_num==snd_una.
                if ((flags_rst_syn_ack & Tcp4FlagAck) != 0 &&
                    seq_lte(tcp_meta.ack_num, pcb->snd_nxt, pcb->snd_una))
                {
                    TcpProto::pcb_abort(pcb, false);
                }
            } else {
                if (tcp_meta.seq_num == pcb->rcv_nxt) {
                    TcpProto::pcb_abort(pcb, false);
                }
                // NOTE: We check simply against rcv_ann_wnd and don't bother calculating
                // the formally correct rcv_wnd based on rcvBufLen. This means that we
                // would ignore an RST that is outside the announced window but still
                // within the actual window for which we would accept data. This is not
                // a problem.
                // NOTE: But we are slightly violating RFC 5961 by allowing seq_num at
                // exactly the right edge (same as we do for ACK, see below).
                else if (seq_diff(tcp_meta.seq_num, pcb->rcv_nxt) <= pcb->rcv_ann_wnd) {
                    Output::pcb_send_empty_ack(pcb);
                }
            }
        }
        else if ((flags_rst_syn_ack & Tcp4FlagSyn) != 0) {
            if (pcb->state == TcpState::SYN_SENT) {
                // Received a SYN in SYN-SENT state.
                if (flags_rst_syn_ack == (Tcp4FlagSyn|Tcp4FlagAck)) {
                    // Expected SYN-ACK response, continue processing.
                    continue_processing = true;
                } else {
                    // SYN without ACK, we do not support this yet, send RST.
                    size_t seqlen = tcplen(tcp_meta.flags, tcp_data.tot_len);
                    Output::send_rst(pcb->tcp, pcb->local_addr, pcb->remote_addr,
                            pcb->local_port, pcb->remote_port,
                            0, true, seq_add(tcp_meta.seq_num, seqlen));
                }
            } else {
                // Handle SYN as per RFC 5961.
                if (pcb->state == TcpState::SYN_RCVD &&
                    tcp_meta.seq_num == seq_add(pcb->rcv_nxt, -1))
                {
                    // This seems to be a retransmission of the SYN, retransmit our
                    // SYN+ACK and bump the abort timeout.
                    Output::pcb_send_syn(pcb);
                    pcb->tim(AbrtTimer()).appendAfter(Context(),
                                                      Constants::SynRcvdTimeoutTicks);
                }
                else {
                    Output::pcb_send_empty_ack(pcb);
                }
            }
        }
        else {
            // Segment has no RST
            // Segment without none of RST, SYN and ACK should never be sent.
            // Just drop it here. Note that RFC 793 would have us check the
            // sequence number and possibly send an empty ACK if the segment
            // is outside the window, but we don't do that for perfomance.
        }
        
        return continue_processing;
    }
    
    static bool pcb_input_syn_sent_rcvd_processing (
            TcpPcb *pcb, TcpSegMeta const &tcp_meta, SeqType acked)
    {
        AMBRO_ASSERT(pcb->state == OneOf(TcpState::SYN_SENT, TcpState::SYN_RCVD))
        AMBRO_ASSERT(pcb->state != TcpState::SYN_SENT || pcb->con != nullptr)
        AMBRO_ASSERT(pcb->state != TcpState::SYN_RCVD || pcb->lis != nullptr)
        
        // See if this is for SYN_SENT or SYN_RCVD. The processing here is
        // sufficiently similar that we benefit from one function handling
        // both of these states.
        bool syn_sent = pcb->state == TcpState::SYN_SENT;
        
        bool proceed = true;
        
        // In SYN_RCVD check that the sequence number is not less than
        // rcv_nxt. Note that if it was less, the segment was trimmed in
        // pcb_input_basic_processing, so we could do without this check.
        if (!syn_sent && seq_lt2(tcp_meta.seq_num, pcb->rcv_nxt)) {
            Output::pcb_send_empty_ack(pcb);
            proceed = false;
        }
        // If our SYN is not acknowledged, send RST and drop. In SYN_RCVD,
        // RFC 793 seems to allow ack_num==snd_una which doesn't make sense.
        // Note that in SYN_SENT, acked is always one here.
        else if (acked == 0) {
            Output::send_rst(pcb->tcp, pcb->local_addr, pcb->remote_addr,
                             pcb->local_port, pcb->remote_port,
                             tcp_meta.ack_num, false, 0);
            proceed = false;
        }
        // If in SYN_SENT a SYN is not received, drop the segment silently.
        else if (syn_sent && (tcp_meta.flags & Tcp4FlagSyn) == 0) {
            proceed = false;
        }
        
        if (!proceed) {
            // If this PCB is unreferenced, move it to the front of
            // the unreferenced list. Note, at this stage a SYN_SENT
            // PCB is always referenced and a SYN_RCVD PCB is never.
            if (!syn_sent) {
                pcb->tcp->move_unrefed_pcb_to_front(pcb);
            }
            return false;
        }
        
        // In SYN_SENT and SYN_RCVD the remote acks only our SYN no more.
        // Otherwise we would have bailed out already.
        AMBRO_ASSERT(pcb->snd_nxt == seq_add(pcb->snd_una, 1))
        AMBRO_ASSERT(tcp_meta.ack_num == pcb->snd_nxt)
        
        // Stop the SYN_RCVD abort timer.
        pcb->tim(AbrtTimer()).unset(Context());
        
        // Stop the retransmission timer.
        pcb->tim(RtxTimer()).unset(Context());
        
        // NOTE: Not updating snd_una here since we will use it to store
        // snd_wnd, it will be updated in pcb_complete_established_transition.
        
        // Set the flag to make sure that pcb_ann_wnd updates rcv_ann_wnd
        // when the next segment is sent.
        pcb->setFlag(PcbFlags::RCV_WND_UPD);
        
        // Calculate the initial send window (snd_wnd).
        // Stuff it into snd_una temporarily. It will be put into its
        // rightful place by pcb_complete_established_transition where
        // snd_una will also be restored to snd_nxt. We don't abuse snd_nxt
        // for this because pcb_abort->pcb_send_rst could read snd_nxt in
        // this state but there is no problem with snd_una.
        pcb->snd_una = pcb_decode_wnd_size(pcb, tcp_meta.window_size);
        
        TcpProto *tcp = pcb->tcp;
        
        if (syn_sent) {
            // Update rcv_nxt and rcv_ann_wnd now that we have received the SYN.
            AMBRO_ASSERT(pcb->rcv_nxt == 0)
            AMBRO_ASSERT(pcb->rcv_ann_wnd > 0)
            pcb->rcv_nxt = seq_add(tcp_meta.seq_num, 1);
            pcb->rcv_ann_wnd--;
            
            // Go to ESTABLISHED state.
            pcb->state = TcpState::ESTABLISHED;
            
            // Make sure received options are parsed.
            parse_received_opts(tcp);
            
            // Update the base_snd_mss based on the MSS option in this packet (if any).
            if (!TcpUtils::calc_snd_mss<Constants::MinAllowedMss>(
                pcb->base_snd_mss, tcp->m_received_opts, &pcb->base_snd_mss))
            {
                // Due to ESTABLISHED transition above, the RST will be an ACK.
                TcpProto::pcb_abort(pcb, true);
                return false;
            }
            
            // Handle the window scale option.
            AMBRO_ASSERT(pcb->snd_wnd_shift == 0)
            if ((tcp->m_received_opts.options & OptionFlags::WND_SCALE) != 0) {
                // Remote sent the window scale flag, so store the window scale
                // value that they will be using. Note that the window size in
                // this incoming segment has already been read above using
                // pcb_decode_wnd_size while snd_wnd_shift was still zero, which
                // is correct because the window size in a SYN-ACK is unscaled.
                pcb->snd_wnd_shift =
                    APrinter::MinValue((uint8_t)14, tcp->m_received_opts.wnd_scale);
            } else {
                // Remote did not send the window scale option, which means we
                // must not use any scaling, so set rcv_wnd_shift back to zero.
                pcb->rcv_wnd_shift = 0;
            }
            
            // Initialize certain sender variables.
            uint16_t pmtu = pcb->snd_mss; // pmtu was stored to snd_mss temporarily
            pcb_complete_established_transition(pcb, pmtu);
            
            // Make sure the ACK to the SYN-ACK is sent.
            pcb->setFlag(PcbFlags::ACK_PENDING);
            
            // We have a TcpConnection (if it went away the PCB would have been aborted).
            TcpConnection *con = pcb->con;
            AMBRO_ASSERT(con != nullptr)
            AMBRO_ASSERT(con->m_v.pcb == pcb)
            
            // Make sure sending of any queued data starts.
            if (con->m_v.snd_buf.tot_len > 0) {
                pcb->setFlag(PcbFlags::OUT_PENDING);
            }
            
            // If the application called closeSending already,
            // call pcb_end_sending now that this can be done.
            if (con->m_v.snd_closed) {
                Output::pcb_end_sending(pcb);
            }
            
            // Report connected event to the user.
            con->connection_established();
            
            // Handle abort of PCB.
            if (AMBRO_UNLIKELY(tcp->m_current_pcb == nullptr)) {
                return false;
            }
            
            // Possible transitions in callback (except to CLOSED):
            // - ESTABLISHED->FIN_WAIT_1
        } else {
            // We have a TcpListener (if it went away the PCB would have been aborted).
            TcpListener *lis = pcb->lis;
            AMBRO_ASSERT(lis->m_listening)
            AMBRO_ASSERT(lis->m_accept_pcb == nullptr)
            
            // In the listener, point m_accept_pcb to this PCB, so that
            // the connection can be accepted using TcpConnection::acceptConnection.
            lis->m_accept_pcb = pcb;
            
            // Remove the PCB from the list of unreferenced PCBs. This protects
            // the PCB from being aborted by allocate_pcb if a new connection is
            // created.
            tcp->m_unrefed_pcbs_list.remove({*pcb, *tcp}, *tcp);
            
            // Call the connectionEstablished callback of the listener to allow the
            // application to accept the connection.
            lis->m_callback->connectionEstablished(lis);
            
            // Handle abort of PCB.
            if (AMBRO_UNLIKELY(tcp->m_current_pcb == nullptr)) {
                return false;
            }
            
            // Possible transitions in callback (except to CLOSED):
            // - SYN_RCVD->ESTABLISHED
            // - ESTABLISHED->FIN_WAIT_1
            
            // If the connection has not been accepted or has been accepted but
            // already abandoned, abort with RST. We must not leave the PCB in
            // SYN_RCVD state here because many variables have been updated for
            // the transition to ESTABLISHED.
            // NOTE: It is important to check state==SYN_RCVD before pcb->con since
            // in SYN_RCVD, pcb->con is undefined because pcb->lis in the same union
            // is relevant.
            if (AMBRO_UNLIKELY(pcb->state == TcpState::SYN_RCVD || pcb->con == nullptr)) {
                TcpProto::pcb_abort(pcb, true);
                return false;
            }
            
            // Since acceptConnection was called successfully, the PCB transitioned
            // to ESTABLISHED, and possibly then to FIN_WAIT_1 if sending was already.
        }
        
        AMBRO_ASSERT(pcb->state == OneOf(TcpState::ESTABLISHED, TcpState::FIN_WAIT_1))
        
        return true;
    }
    
    static bool pcb_input_ack_wnd_processing (TcpPcb *pcb, TcpSegMeta const &tcp_meta,
                                              SeqType acked, size_t orig_data_len)
    {
        AMBRO_ASSERT(pcb->state != OneOf(TcpState::CLOSED,
                                         TcpState::SYN_SENT, TcpState::SYN_RCVD))
        
        // If this PCB is unreferenced, move it to the front of the unreferenced list.
        if (AMBRO_UNLIKELY(pcb->con == nullptr)) {
            pcb->tcp->move_unrefed_pcb_to_front(pcb);
        }
        
        // Handle new acknowledgments.
        if (acked > 0) {
            // We can only get here if there was anything pending acknowledgement
            // (snd_una!=snd_nxt), this is assured in pcb_input_basic_processing
            // when calculating acked. Further, it is assured that snd_una==snd_nxt
            // in FIN_WAIT_2 and TIME_WAIT (additional states we are asserting we are
            // not in).
            AMBRO_ASSERT(can_output_in_state(pcb->state))
            AMBRO_ASSERT(Output::pcb_has_snd_outstanding(pcb))
            // The amount of acknowledged sequence numbers was already calculated.
            AMBRO_ASSERT(acked == seq_diff(tcp_meta.ack_num, pcb->snd_una))
            
            // Inform Output that something was acked. This performs RTT
            // measurement and congestion control related processing.
            Output::pcb_output_handle_acked(pcb, tcp_meta.ack_num, acked);
            
            // Update snd_una due to sequences having been ACKed.
            pcb->snd_una = tcp_meta.ack_num;
            
            // Check if our FIN has just been acked.
            bool fin_acked = Output::pcb_fin_acked(pcb);
            
            // Calculate the amount of acknowledged data (without ACK of FIN).
            SeqType data_acked_seq = acked - (SeqType)fin_acked;
            AMBRO_ASSERT(data_acked_seq <= SIZE_MAX)
            size_t data_acked = data_acked_seq;
            
            if (AMBRO_LIKELY(data_acked > 0)) {
                // We necessarily still have a TcpConnection, if the connection was
                // abandoned with unsent/unacked data, it would have been aborted.
                TcpConnection *con = pcb->con;
                AMBRO_ASSERT(con != nullptr)
                AMBRO_ASSERT(data_acked <= con->m_v.snd_buf.tot_len)
                AMBRO_ASSERT(con->m_v.snd_buf_cur.tot_len <= con->m_v.snd_buf.tot_len)
                
                // The snd_wnd needs adjustment because it is relative to snd_una.
                // It is okay to adjust for acked data only not FIN since after
                // FIN is acked snd_wnd is no longer relevant.
                if (AMBRO_LIKELY(data_acked <= con->m_v.snd_wnd)) {
                    con->m_v.snd_wnd -= data_acked;
                } else {
                    con->m_v.snd_wnd = 0;
                }
                
                // Advance the send buffer.
                size_t cur_offset = con->m_v.snd_buf.tot_len - con->m_v.snd_buf_cur.tot_len;
                if (data_acked >= cur_offset) {
                    con->m_v.snd_buf_cur.skipBytes(data_acked - cur_offset);
                    con->m_v.snd_buf = con->m_v.snd_buf_cur;
                } else {
                    con->m_v.snd_buf.skipBytes(data_acked);
                }
                
                // Adjust the push index.
                if (data_acked <= con->m_v.snd_psh_index) {
                    con->m_v.snd_psh_index -= data_acked;
                } else {
                    con->m_v.snd_psh_index = 0;
                }
                
                // If sending was closed the push index must (still)
                // point to the end of the send buffer.
                AMBRO_ASSERT(snd_open_in_state(pcb->state) ||
                             con->m_v.snd_psh_index == con->m_v.snd_buf.tot_len)
                
                // Report data-sent event to the user.
                con->data_sent(data_acked);
                if (AMBRO_UNLIKELY(pcb_aborted_in_callback(pcb))) {
                    return false;
                }
                // Possible transitions in callback (except to CLOSED):
                // - ESTABLISHED->FIN_WAIT_1
                // - CLOSE_WAIT->LAST_ACK
            }
            
            if (AMBRO_UNLIKELY(fin_acked)) {
                // We must be in a state where we had queued a FIN for sending
                // but have not received its acknowledgement yet.
                AMBRO_ASSERT(pcb->state == OneOf(TcpState::FIN_WAIT_1, TcpState::CLOSING,
                                                 TcpState::LAST_ACK))
                
                // Tell TcpConnection and application (if any) about end sent.
                TcpConnection *con = pcb->con;
                if (AMBRO_LIKELY(con != nullptr)) {
                    con->end_sent();
                    if (AMBRO_UNLIKELY(pcb_aborted_in_callback(pcb))) {
                        return false;
                    }
                    // Possible transitions in callback (except to CLOSED): none.
                }
                
                if (pcb->state == TcpState::FIN_WAIT_1) {
                    // FIN is acked in FIN_WAIT_1, transition to FIN_WAIT_2.
                    TcpProto::pcb_go_to_fin_wait_2(pcb);
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
            } else {
                // Some data was ACKed but not FIN.
                AMBRO_ASSERT(can_output_in_state(pcb->state))
                
                // Is any data or FIN outstanding?
                if (AMBRO_LIKELY(Output::pcb_has_snd_outstanding(pcb))) {
                    // Stop the rtx_timer since any running timeout is no longer
                    // valid due to something having been acked.
                    pcb->tim(RtxTimer()).unset(Context());
                    
                    // Schedule pcb_output_active/pcb_output_abandoned, so that the
                    // rtx_timer will be restarted if needed (for retransmission or
                    // window probe).
                    pcb->setFlag(PcbFlags::OUT_PENDING);
                } else {
                    // Start the idle timeout.
                    pcb->tim(RtxTimer()).appendAfter(Context(), Output::pcb_rto_time(pcb));
                    pcb->setFlag(PcbFlags::IDLE_TIMER);
                    
                    // Clear the OUT_PENDING flag due to its preconditions.
                    pcb->clearFlag(PcbFlags::OUT_PENDING);
                    
                    // Stop the output timer due to assert in its handler.
                    pcb->tim(OutputTimer()).unset(Context());
                }
            }
        }
        // Handle duplicate ACKs (RFC 5681).
        else {
            // Check conditions in such an order that we are typically
            // fast for segments which are not duplicate ACKs.
            if (AMBRO_UNLIKELY(orig_data_len == 0) &&
                (tcp_meta.flags & Tcp4FlagFin) == 0 &&
                tcp_meta.ack_num == pcb->snd_una &&
                can_output_in_state(pcb->state) && Output::pcb_has_snd_unacked(pcb) &&
                pcb->con != nullptr &&
                pcb_decode_wnd_size(pcb, tcp_meta.window_size) == pcb->con->m_v.snd_wnd
            ) {
                if (pcb->num_dupack < Constants::FastRtxDupAcks +
                                      Constants::MaxAdditionaDupAcks)
                {
                    pcb->num_dupack++;
                    if (pcb->num_dupack == Constants::FastRtxDupAcks) {
                        Output::pcb_fast_rtx_dup_acks_received(pcb);
                    }
                    else if (pcb->num_dupack > Constants::FastRtxDupAcks) {
                        Output::pcb_extra_dup_ack_received(pcb);
                    }
                }
            }
        }
        
        // Handle window updates.
        // Our update logic is much simpler than recommented RFC 793:
        // update the window if the segment is not an old ACK. This is
        // more efficient both in terms of memory (no snd_wl1/snd_wl2)
        // and CPU (much simpler check). The standard approach may even
        // have issues with sequence numbers becoming uncomparable and
        // consequently valid window updates being ignored, but the
        // actual possibility will not be elaborated upon...
        // The only possibly problematic behavior of our way is that
        // it may use old window values when the ACK number is the same,
        // but this could only happen if segments are reordered, and is
        // far from being a serious problem even if it does happen.
        if (AMBRO_LIKELY(pcb->snd_una == tcp_meta.ack_num)) {
            SeqType new_snd_wnd = pcb_decode_wnd_size(pcb, tcp_meta.window_size);
            Output::pcb_update_snd_wnd(pcb, new_snd_wnd);
        }
        
        return true;
    }
    
    static bool pcb_input_rcv_processing (TcpPcb *pcb, SeqType eff_rel_seq, bool seg_fin,
                                          IpBufRef const &tcp_data)
    {
        AMBRO_ASSERT(accepting_data_in_state(pcb->state))
        
        // We only get here if the segment fits into the receive window, this is assured
        // by pcb_input_basic_processing. It is also ensured that pcb->rcv_ann_wnd fits
        // into size_t and we need this to avoid oveflows in buffer space checks below.
        if (SIZE_MAX < UINT32_MAX) {
            AMBRO_ASSERT(eff_rel_seq <= SIZE_MAX)
            AMBRO_ASSERT(tcp_data.tot_len <= SIZE_MAX - eff_rel_seq)
        }
        
        TcpConnection *con = pcb->con;
        
        // This will be the in-sequence data or FIN that we will process.
        size_t rcv_datalen;
        bool rcv_fin;
        
        // Handling for abandoned connection
        if (AMBRO_UNLIKELY(con == nullptr)) {
            // If the segment is out-of-sequence, or has any data, abort the
            // connection since we cannot accept any more data.
            if (AMBRO_UNLIKELY(eff_rel_seq != 0 || tcp_data.tot_len != 0)) {
                TcpProto::pcb_abort(pcb, true);
                return false;
            }
            
            // Processing possibly only a FIN.
            rcv_datalen = 0;
            rcv_fin = seg_fin;
        }
        // Fast path is that recevied segment is in sequence and there
        // is no out-of-sequence data or FIN buffered.
        else if (AMBRO_LIKELY(eff_rel_seq == 0 && con->m_v.ooseq.isNothingBuffered())) {
            // Processing the in-sequence segment.
            rcv_datalen = tcp_data.tot_len;
            rcv_fin = seg_fin;
            
            if (rcv_datalen > 0) {
                // Check that there is buffer space available for the received data.
                // If not then abort the connection. This can happen if the connection
                // failed to provide buffer for the initial receive window.
                if (AMBRO_UNLIKELY(con->m_v.rcv_buf.tot_len < rcv_datalen)) {
                    TcpProto::pcb_abort(pcb, true);
                    return false;
                }
                
                // Copy any received data into the receive buffer, shifting it.
                con->m_v.rcv_buf.giveBuf(tcp_data);
            }
        }
        // Slow path performs out-of-sequence buffering.
        else {
            // Update information about out-of-sequence data and FIN.
            SeqType eff_seq = seq_add(pcb->rcv_nxt, eff_rel_seq);
            bool need_ack;
            bool update_ok = con->m_v.ooseq.updateForSegmentReceived(
                pcb->rcv_nxt, eff_seq, tcp_data.tot_len, seg_fin, need_ack);
            
            // If there was an inconsistency, abort.
            if (AMBRO_UNLIKELY(!update_ok)) {
                TcpProto::pcb_abort(pcb, true);
                return false;
            }
            
            // If the ACK is needed, set the ACK-pending flag.
            if (need_ack) {
                pcb->setFlag(PcbFlags::ACK_PENDING);
            }
            
            if (tcp_data.tot_len > 0) {
                // Check that there is buffer space available for the received data.
                if (AMBRO_UNLIKELY(
                    con->m_v.rcv_buf.tot_len < eff_rel_seq + tcp_data.tot_len))
                {
                    TcpProto::pcb_abort(pcb, true);
                    return false;
                }
                
                // Copy any received data into the receive buffer.
                IpBufRef dst_buf = con->m_v.rcv_buf;
                dst_buf.skipBytes(eff_rel_seq);
                dst_buf.giveBuf(tcp_data);
            }
            
            // Get data or FIN from the out-of-sequence buffer.
            con->m_v.ooseq.shiftAvailable(pcb->rcv_nxt, rcv_datalen, rcv_fin);
            
            // If we got any data out of OOS buffering here then the receive buffer
            // can necessarily be shifted by that much. This is because the data was
            // written into the receive buffer when it was received.
            AMBRO_ASSERT(con->m_v.rcv_buf.tot_len >= rcv_datalen)
            
            // Shift any processed data out of the receive buffer.
            if (rcv_datalen > 0) {
                con->m_v.rcv_buf.skipBytes(rcv_datalen);
            }
        }
        
        // Compute the amount of processed sequence numbers. Note that rcv_fin
        // is not used from now on, instead rcv_seqlen and rcv_datalen are compared.
        SeqType rcv_seqlen = (SeqType)rcv_datalen + rcv_fin;
        
        // Process received data/FIN.
        return pcb_process_received(pcb, rcv_seqlen, rcv_datalen);
    }
    
    // Update state due to any received data (e.g. rcv_nxt), make state transitions
    // due to any received FIN, and call associated application callbacks.
    static bool pcb_process_received (TcpPcb *pcb, SeqType rcv_seqlen, size_t rcv_datalen)
    {
        // Accept anything newly received.
        if (rcv_seqlen > 0) {
            // Adjust rcv_nxt due to newly received data.
            pcb->rcv_nxt = seq_add(pcb->rcv_nxt, rcv_seqlen);
            
            // Adjust rcv_ann_wnd which is relative to rcv_nxt.
            // Note, it is possible that rcv_seqlen is greater than rcv_ann_wnd
            // in case the peer send data before receiving a window update
            // permitting that.
            if (AMBRO_LIKELY(rcv_seqlen <= pcb->rcv_ann_wnd)) {
                pcb->rcv_ann_wnd -= rcv_seqlen;
            } else {
                pcb->rcv_ann_wnd = 0;
            }
            
            // Due to window scaling the reduction of rcv_ann_wnd may
            // permit announcing more window, so set the flag which forces
            // pcb_ann_wnd to update the window when a segment is sent.
            pcb->setFlag(PcbFlags::RCV_WND_UPD);
            
            // Make sure an ACK is sent.
            pcb->setFlag(PcbFlags::ACK_PENDING);
            
            // Processing a FIN?
            if (AMBRO_UNLIKELY(rcv_seqlen > rcv_datalen)) {
                // Make the appropriate state transition.
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
            
            // Processing any data?
            if (AMBRO_LIKELY(rcv_datalen > 0)) {
                // Must have TcpConnection here (or we would have bailed out before).
                TcpConnection *con = pcb->con;
                AMBRO_ASSERT(con != nullptr)
                
                // Give any data to the user.
                con->data_received(rcv_datalen);
                if (AMBRO_UNLIKELY(pcb_aborted_in_callback(pcb))) {
                    return false;
                }
                // Possible transitions in callback (except to CLOSED):
                // - ESTABLISHED->FIN_WAIT_1
                // - CLOSE_WAIT->LAST_ACK
            }
            
            // Processing a FIN?
            if (AMBRO_UNLIKELY(rcv_seqlen > rcv_datalen)) {
                // Tell TcpConnection and application (if any) about end received.
                TcpConnection *con = pcb->con;
                if (AMBRO_LIKELY(con != nullptr)) {
                    con->end_received();
                    if (AMBRO_UNLIKELY(pcb_aborted_in_callback(pcb))) {
                        return false;
                    }
                    // Possible transitions in callback (except to CLOSED):
                    // - CLOSE_WAIT->LAST_ACK
                }
                
                // Complete transition from FIN_WAIT_2 to TIME_WAIT.
                if (pcb->state == TcpState::FIN_WAIT_2_TIME_WAIT) {
                    TcpProto::pcb_go_to_time_wait(pcb);
                }
            }
        }
        
        return true;
    }
    
    // Apply window scaling to a received window size value.
    inline static SeqType pcb_decode_wnd_size (TcpPcb *pcb, uint16_t rx_wnd_size)
    {
        return (SeqType)rx_wnd_size << pcb->snd_wnd_shift;
    }
    
    // Return the maximum receive window that can be announced
    // with respect to window scaling.
    static SeqType max_rcv_wnd_ann (TcpPcb *pcb)
    {
        return (SeqType)UINT16_MAX << pcb->rcv_wnd_shift;
    }
    
    // Calculate how much window would be announced if sent an ACK now.
    static SeqType pcb_calc_wnd_update (TcpPcb *pcb)
    {
        AMBRO_ASSERT(accepting_data_in_state(pcb->state))
        
        // Calculate the maximum window that can be announced with the the
        // current window scale factor.
        SeqType max_ann = max_rcv_wnd_ann(pcb);
        
        // Calculate the minimum of the available buffer space and the maximum
        // window that can be announced. There is no need to also clamp to
        // MaxWindow since max_ann will be less than MaxWindow.
        SeqType bounded_wnd = APrinter::MinValueU(pcb->rcvBufLen(), max_ann);
        
        // Clear the lowest order bits which cannot be sent with the current
        // window scale factor. The already calculated max_ann is suitable
        // as a mask for this (consider that bounded_wnd<=max_ann).
        SeqType ann_wnd = bounded_wnd & max_ann;
        
        // Result which may be assigned to pcb->rcv_ann_wnd is guaranteed
        // to fit into size_t as required since it is <=rcvBufLen which is
        // a size_t.
        return ann_wnd;
    }
    
    static void parse_received_opts (TcpProto *tcp)
    {
        // Only parse if the options were not parsed already.
        if (tcp->m_received_opts_buf.node != nullptr) {
            TcpUtils::parse_options(tcp->m_received_opts_buf, &tcp->m_received_opts);
            tcp->m_received_opts_buf.node = nullptr;
        }
    }
};

#include <aipstack/EndNamespace.h>

#endif
