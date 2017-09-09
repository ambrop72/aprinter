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

#ifndef AIPSTACK_TCP_RING_BUFFER_UTILS_H
#define AIPSTACK_TCP_RING_BUFFER_UTILS_H

#include <stddef.h>
#include <string.h>

#include <aipstack/misc/Preprocessor.h>
#include <aipstack/misc/Assert.h>

#include <aipstack/common/Buf.h>
#include <aipstack/misc/MinMax.h>
#include <aipstack/common/MemRef.h>
#include <aipstack/utils/WrapBuffer.h>

namespace AIpStack {

template <typename TcpProto>
class TcpRingBufferUtils {
    AIPSTACK_USE_TYPES2(AIpStack, (IpBufNode, IpBufRef))
    AIPSTACK_USE_TYPES1(TcpProto, (SeqType, TcpConnection))
    
public:
    class SendRingBuffer {
    public:
        void setup (TcpConnection &con, char *buf, size_t buf_size)
        {
            AIPSTACK_ASSERT(buf != nullptr)
            AIPSTACK_ASSERT(buf_size > 0)
            
            m_buf_node = IpBufNode{buf, buf_size, &m_buf_node};
            
            con.setSendBuf(IpBufRef{&m_buf_node, (size_t)0, (size_t)0});
        }
        
        size_t getFreeLen (TcpConnection &con)
        {
            return m_buf_node.len - get_send_buf(con).tot_len;
        }
        
        WrapBuffer getWritePtr (TcpConnection &con)
        {
            IpBufRef snd_buf = get_send_buf(con);
            size_t write_offset = add_modulo(snd_buf.offset, snd_buf.tot_len, m_buf_node.len);
            return WrapBuffer(m_buf_node.len - write_offset, m_buf_node.ptr + write_offset, m_buf_node.ptr);
        }
        
        void provideData (TcpConnection &con, size_t amount)
        {
            AIPSTACK_ASSERT(amount <= getFreeLen(con))
            
            con.extendSendBuf(amount);
        }
        
        void writeData (TcpConnection &con, MemRef data)
        {
            AIPSTACK_ASSERT(data.len <= getFreeLen(con))
            
            getWritePtr(con).copyIn(data);
            con.extendSendBuf(data.len);
        }
        
    private:
        inline IpBufRef get_send_buf (TcpConnection &con)
        {
            IpBufRef snd_buf = con.getSendBuf();
            AIPSTACK_ASSERT(snd_buf.tot_len <= m_buf_node.len)
            AIPSTACK_ASSERT(snd_buf.offset < m_buf_node.len) // < due to eager buffer consumption
            return snd_buf;
        }
        
    private:
        IpBufNode m_buf_node;
    };
    
    class RecvRingBuffer {
    public:
        // NOTE: If using mirror region and initial_rx_data is not empty, it is
        // you may need to call updateMirrorAfterDataReceived to make sure initial
        // data is mirrored as applicable.
        void setup (TcpConnection &con, char *buf, size_t buf_size, int wnd_upd_div,
                    IpBufRef initial_rx_data)
        {
            AIPSTACK_ASSERT(buf != nullptr)
            AIPSTACK_ASSERT(buf_size > 0)
            AIPSTACK_ASSERT(wnd_upd_div >= 2)
            AIPSTACK_ASSERT(initial_rx_data.tot_len <= buf_size)
            AIPSTACK_ASSERT(con.getRecvBuf().tot_len <= buf_size - initial_rx_data.tot_len)
            
            m_buf_node = IpBufNode{buf, buf_size, &m_buf_node};
            
            SeqType max_rx_window = MinValueU(m_buf_node.len, TcpProto::MaxRcvWnd);
            SeqType thres = MaxValue((SeqType)1, max_rx_window / wnd_upd_div);
            con.setWindowUpdateThreshold(thres);
            
            IpBufRef recv_buf = IpBufRef{&m_buf_node, (size_t)0, m_buf_node.len};
            
            if (initial_rx_data.tot_len > 0) {
                recv_buf.giveBuf(initial_rx_data);
            }
            
            IpBufRef old_recv_buf = con.getRecvBuf();
            if (old_recv_buf.tot_len > 0) {
                IpBufRef tmp_buf = recv_buf;
                tmp_buf.giveBuf(old_recv_buf);
            }
            
            con.setRecvBuf(recv_buf);
        }
        
        size_t getUsedLen (TcpConnection &con)
        {
            return m_buf_node.len - get_recv_buf(con).tot_len;
        }
        
        WrapBuffer getReadPtr (TcpConnection &con)
        {
            IpBufRef rcv_buf = get_recv_buf(con);
            size_t read_offset = add_modulo(rcv_buf.offset, rcv_buf.tot_len, m_buf_node.len);
            return WrapBuffer(m_buf_node.len - read_offset, m_buf_node.ptr + read_offset, m_buf_node.ptr);
        }
        
        void consumeData (TcpConnection &con, size_t amount)
        {
            AIPSTACK_ASSERT(amount <= getUsedLen(con))
            
            con.extendRecvBuf(amount);
        }
        
        void readData (TcpConnection &con, MemRef data)
        {
            AIPSTACK_ASSERT(data.len <= getUsedLen(con))
            
            getReadPtr(con).copyOut(data);
            con.extendRecvBuf(data.len);
        }
        
        void updateMirrorAfterDataReceived (TcpConnection &con, size_t mirror_size, size_t amount)
        {
            AIPSTACK_ASSERT(mirror_size > 0)
            AIPSTACK_ASSERT(mirror_size < m_buf_node.len)
            
            if (amount > 0) {
                // Calculate the offset in the buffer to which new data was written.
                IpBufRef rcv_buf = con.getRecvBuf();
                AIPSTACK_ASSERT(rcv_buf.tot_len + amount <= m_buf_node.len)
                AIPSTACK_ASSERT(rcv_buf.offset < m_buf_node.len)
                size_t data_offset = add_modulo(rcv_buf.offset, m_buf_node.len - amount, m_buf_node.len);
                
                // Copy data to the mirror region as needed.
                if (data_offset < mirror_size) {
                    ::memcpy(
                        m_buf_node.ptr + m_buf_node.len + data_offset,
                        m_buf_node.ptr + data_offset,
                        MinValue(amount, mirror_size - data_offset));
                }
                if (amount > m_buf_node.len - data_offset) {
                    ::memcpy(
                        m_buf_node.ptr + m_buf_node.len,
                        m_buf_node.ptr,
                        MinValue(amount - (m_buf_node.len - data_offset), mirror_size));
                }
            }
        }
        
    private:
        inline IpBufRef get_recv_buf (TcpConnection &con)
        {
            IpBufRef rcv_buf = con.getRecvBuf();
            AIPSTACK_ASSERT(rcv_buf.tot_len <= m_buf_node.len)
            AIPSTACK_ASSERT(rcv_buf.offset < m_buf_node.len) // < due to eager buffer consumption
            return rcv_buf;
        }
        
    private:
        IpBufNode m_buf_node;
    };
    
private:
    static size_t add_modulo (size_t start, size_t count, size_t modulo)
    {
        size_t x = start + count;
        if (x >= modulo) {
            x -= modulo;
        }
        return x;
    }
};

}

#endif
