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

#ifndef AIPSTACK_TCP_OOS_BUFFER_H
#define AIPSTACK_TCP_OOS_BUFFER_H

#include <stdint.h>
#include <stddef.h>

#include <algorithm>

#include <aipstack/meta/Instance.h>
#include <aipstack/meta/ChooseInt.h>
#include <aipstack/misc/Preprocessor.h>
#include <aipstack/misc/Assert.h>

#include <aipstack/common/Options.h>
#include <aipstack/tcp/TcpUtils.h>

namespace AIpStack {

/**
 * Implements maintaining information about received
 * out-of-sequence TCP data or FIN.
 * 
 * It keeps information about up to a statically
 * configured number of received contiguous ranges
 * of data or FIN.
 */
template <typename Arg>
class TcpOosBuffer
{
    AIPSTACK_USE_TYPES1(TcpUtils, (SeqType))
    AIPSTACK_USE_VALS(TcpUtils, (seq_diff, seq_add, seq_lte, seq_lt))
    
    static_assert(Arg::NumOosSegs > 0, "");
    using IndexType = ChooseIntForMax<Arg::NumOosSegs, false>;
    static IndexType const NumOosSegs = Arg::NumOosSegs;
    
    // Represents one contiguous region of buffered data or
    // a FIN or an end marker.
    struct OosSeg {
        // First sequence number (for data segments).
        SeqType start;
        
        // One-past-last sequence number (for data segments).
        SeqType end;
        
        // Entry with start==end+1 marks the end of segments.
        // In reality only {1, 0} will be used but checking this
        // more general property is probably more efficient.
        inline bool isEnd () const
        {
            return start == seq_add(end, 1);
        }
        
        // Make an end segment.
        inline static OosSeg MakeEnd ()
        {
            return OosSeg{1, 0};
        }
        
        // Entry with start==end represents a FIN.
        // For a FIN, start and end will be the FIN sequence number
        // plus 1. This simplifies algorithms because a FIN is not
        // considerded to touch any preceding segment, just as data
        // segments never touch each other.
        inline bool isFin () const
        {
            return start == end;
        }
        
        // Get the FIN sequence number of a FIN segment.
        // It is only valid to call this on an isFin segment.
        inline SeqType getFinSeq () const
        {
            return seq_diff(start, 1);
        }
        
        // Make a FIN segment, with the given FIN sequence number.
        inline static OosSeg MakeFin (SeqType fin_seq)
        {
            SeqType seg_seq = seq_add(fin_seq, 1);
            return OosSeg{seg_seq, seg_seq};
        }
        
        // Check if segment is an end of FIN.
        // This is supposed to be more efficient than isEnd||isFin.
        inline bool isEndOrFin () const
        {
            return seq_diff(start, end) <= 1;
        }
    };
    
private:
    // List of buffered segments.
    // If there are less than NumOosSegs used, then the first
    // end segment (isEnd()) marks the end. The end segment is
    // not logically a valid segment, and segments following
    // the end segment are undefined.
    OosSeg m_ooseq[NumOosSegs];
    
public:
    /**
     * Initialize (clear) the out-of-sequence information.
     */
    inline void init ()
    {
        // Set the first element to an end segment.
        m_ooseq[0] = OosSeg::MakeEnd();
    }
    
    /**
     * Check if there is no out-of-sequence data or FIN buffered.
     * 
     * @return Whether neither data nor FIN is buffered.
     */
    inline bool isNothingBuffered ()
    {
        // Check if the first element is an end segment.
        return m_ooseq[0].isEnd();
    }
    
    /**
     * Update out-of-sequence information due to arrival of a new segment.
     * 
     * The segment must already have been checked to fit within
     * the available receive window. After this is called,
     * shiftAvailable will typicall be called to remove any available
     * data (but this is not strictly required).
     * 
     * @param rcv_nxt The first sequence number that has not been received,
     *                i.e. the rcv_nxt of the PCB before it was updated due
     *                to arrival of this segment.
     * @param seg_start The sequence number of this segment.
     * @param seg_datalen The data length of this segment (possibly zero).
     * @param seg_fin Whether this segment is a FIN.
     * @param need_ack This will be set to whether an ACK is needed because
     *                 the segment is out of sequence or filled a gap.
     * @return True on success, false in case of FIN inconsistency
     *         (no updates done).
     */
    bool updateForSegmentReceived (SeqType rcv_nxt, SeqType seg_start, size_t seg_datalen,
                                   bool seg_fin, bool &need_ack)
    {
        // Initialize need_ack to whether the segment is out of sequence.
        // If the segment fills in a gap this will be set to true below.
        need_ack = seg_start != rcv_nxt;
        
        // Calculate sequence number for end of data.
        SeqType seg_end = seq_add(seg_start, seg_datalen);
        
        // Count the number of valid segments (this may include a FIN segment).
        IndexType num_ooseq = count_ooseq();
        
        // Check for FIN-related inconsistencies.
        if (num_ooseq > 0 && m_ooseq[num_ooseq - 1].isFin()) {
            // Have a buffered FIN, get its sequence number.
            SeqType fin_seq = m_ooseq[num_ooseq - 1].getFinSeq();
            
            // Check if we just received data beyond the buffered FIN. (A)
            if (seg_datalen > 0 && !seq_lte(seg_end, fin_seq, rcv_nxt)) {
                return false;
            }
            
            // Check if we just received a FIN at a different position.
            if (seg_fin && seg_end != fin_seq) {
                return false;
            }
        } else {
            // Check if we just received a FIN that is before already received data.
            if (seg_fin && num_ooseq > 0 &&
                !seq_lte(m_ooseq[num_ooseq - 1].end, seg_end, rcv_nxt)) {
                return false;
            }
        }
        
        // If the new segment has any data, update the segments.
        if (seg_datalen > 0) {
            // Skip over segments strictly before this one.
            // Note: we would never skip over a FIN segment due to check (A) above.
            IndexType pos = 0;
            while (pos < num_ooseq && seq_lt(m_ooseq[pos].end, seg_start, rcv_nxt)) {
                pos++;
            }
            
            // If there are no more segments or the segment [pos] is strictly
            // after the new segment, we insert a new segment here. Otherwise
            // the new segment intersects or touches [pos] and we merge the
            // new segment with [pos] and possibly subsequent segments.
            // No special accomodation of FIN segments is needed because a FIN
            // appears with start==end equal to the FIN sequence number plus one.
            if (pos == num_ooseq || seq_lt(seg_end, m_ooseq[pos].start, rcv_nxt)) {
                // If all segment slots are used and we are not inserting to the end,
                // release the last slot. This ensures that we can always accept
                // in-sequence data, and not stall after all slots are exhausted.
                // More generally, it means that if all slots are used we will
                // discard existing data in favor of newly received data that
                // precedes the existing data in terms of sequence numbers.
                // Note that we may discard a FIN here, this is not a problem
                // other than missing a chance to detect a FIN inconsistency.
                if (num_ooseq == NumOosSegs && pos < NumOosSegs) {
                    num_ooseq--;
                }
                
                // Insert a segment to this spot only if there is space.
                if (num_ooseq < NumOosSegs) {
                    if (pos < num_ooseq) {
                        need_ack = true;
                        std::move_backward(&m_ooseq[pos],
                                           &m_ooseq[num_ooseq], &m_ooseq[num_ooseq + 1]);
                    }
                    m_ooseq[pos] = OosSeg{seg_start, seg_end};
                    num_ooseq++;
                }
            } else {
                // The segment at [pos] cannot be a FIN.
                // Proof: else branch implies seg_end >= [pos].start, therefore
                // seg_end > [pos].start - 1 which is equivalent to the consistency
                // check (A) assuming [pos] is the FIN, therefore the check would
                // have failed and we wouldn't be here.
                AIPSTACK_ASSERT(!m_ooseq[pos].isFin())
                
                // Extend the existing segment to the left if needed.
                if (seq_lt(seg_start, m_ooseq[pos].start, rcv_nxt)) {
                    need_ack = true;
                    m_ooseq[pos].start = seg_start;
                }
                
                // Extend the existing segment to the right if needed.
                if (!seq_lte(seg_end, m_ooseq[pos].end, rcv_nxt)) {
                    need_ack = true;
                    m_ooseq[pos].end = seg_end;
                    
                    // Merge the extended segment [pos] with any subsequent segments
                    // that it now intersects or touches.
                    IndexType merge_pos = pos + 1;
                    while (merge_pos < num_ooseq &&
                           !seq_lt(seg_end, m_ooseq[merge_pos].start, rcv_nxt))
                    {
                        // Segment at [merge_pos] cannot be a FIN, for similar reasons that
                        // [pos] could not be above.
                        AIPSTACK_ASSERT(!m_ooseq[merge_pos].isFin())
                        
                        // If the extended segment [pos] extends no more than to the end of
                        // [merge_pos], then [merge_pos] is the last segment to be merged.
                        if (seq_lte(seg_end, m_ooseq[merge_pos].end, rcv_nxt)) {
                            // Make sure [pos] includes the entire [merge_pos].
                            m_ooseq[pos].end = m_ooseq[merge_pos].end;
                            merge_pos++;
                            break;
                        }
                        
                        // The segment [merge_pos] is extends strictly over the end of
                        // [merge_pos], continue looking for segments to merge.
                        merge_pos++;
                    }
                    
                    // If we merged [pos] with any subsequent segments, move any
                    // remaining segments left over those merged segments and adjust
                    // num_ooseq.
                    IndexType num_merged = merge_pos - (pos + 1);
                    if (num_merged > 0) {
                        if (merge_pos < num_ooseq) {
                            std::move(&m_ooseq[merge_pos],
                                      &m_ooseq[num_ooseq], &m_ooseq[pos + 1]);
                        }
                        num_ooseq -= num_merged;
                    }
                }
            }
        }
        
        // If we got a FIN, remember it if not already and there is space.
        if (seg_fin && (num_ooseq == 0 || !m_ooseq[num_ooseq - 1].isFin()) &&
            num_ooseq < NumOosSegs)
        {
            m_ooseq[num_ooseq] = OosSeg::MakeFin(seg_end);
            num_ooseq++;
        }
        
        // If not all segments are used, terminate the list.
        if (num_ooseq < NumOosSegs) {
            m_ooseq[num_ooseq] = OosSeg::MakeEnd();
        }
        AIPSTACK_ASSERT(num_ooseq == count_ooseq())
        
        return true;
    }
    
    /**
     * Shifts any available data or FIN from the front of the
     * out-of-sequence information buffer.
     * 
     * @param rcv_nxt The first sequence number that has not been shifted
     *                out, i.e. the rcv_nxt of the PCB before it was updated
     *                due any shifted out data.
     * @param datalen This will be set to the length of shifted-out data,
     *                or zero if none. Any shifted out data begins with
     *                sequence number rcv_nxt.
     * @param fin This will be set to whether a FIN follows the shifted-out
     *            data (or follows rcv_nxt if there is no shifted-out data).
     */
    void shiftAvailable (SeqType rcv_nxt, size_t &datalen, bool &fin)
    {
        // Check if we have a data segment starting at rcv_nxt.
        if (!m_ooseq[0].isEndOrFin() && m_ooseq[0].start == rcv_nxt) {
            // Return the data length to the caller.
            SeqType seq_end = m_ooseq[0].end;
            datalen = seq_diff(seq_end, m_ooseq[0].start);
            
            // Shift the segment out of the buffer.
            IndexType num_ooseq = count_ooseq();
            if (num_ooseq > 1) {
                std::move(&m_ooseq[1], &m_ooseq[num_ooseq], &m_ooseq[0]);
            }
            num_ooseq--;
            m_ooseq[num_ooseq] = OosSeg::MakeEnd();
            
            // The next segment is not supposed to have any data that we
            // could immediately consume since there are always gaps
            // between segments.
            AIPSTACK_ASSERT(m_ooseq[0].isEndOrFin() ||
                         !seq_lte(m_ooseq[0].start, seq_end, rcv_nxt))
        } else {
            // Not returning any data.
            datalen = 0;
        }
        
        // Check if we have a FIN with sequence number rcv_nxt+datalen.
        // Note: no need to check !isEnd because isFin implies !isEnd.
        // There is no need to consume the FIN.
        fin = m_ooseq[0].isFin() && m_ooseq[0].getFinSeq() == seq_add(rcv_nxt, datalen);
    }
    
private:
    // Return the number of out-of-sequence segments by counting
    // until an end marker is found or the end of segments is reached.
    IndexType count_ooseq ()
    {
        IndexType num_ooseq = 0;
        while (num_ooseq < NumOosSegs && !m_ooseq[num_ooseq].isEnd()) {
            num_ooseq++;
        }
        return num_ooseq;
    }
};

struct TcpOosBufferServiceOptions {
    AIPSTACK_OPTION_DECL_VALUE(NumOosSegs, size_t, 4)
};

template <typename... Options>
class TcpOosBufferService {
    template <typename>
    friend class TcpOosBuffer;
    
    AIPSTACK_OPTION_CONFIG_VALUE(TcpOosBufferServiceOptions, NumOosSegs)

public:
    AIPSTACK_DEF_INSTANCE(TcpOosBufferService, TcpOosBuffer)
};

}

#endif
