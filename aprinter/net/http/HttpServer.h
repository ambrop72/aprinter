/*
 * Copyright (c) 2015 Ambroz Bizjak
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

#ifndef APRINTER_HTTP_SERVER_H
#define APRINTER_HTTP_SERVER_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <aprinter/meta/MinMax.h>
#include <aprinter/meta/BitsInInt.h>
#include <aprinter/meta/AliasStruct.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/Callback.h>
#include <aprinter/base/ProgramMemory.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/WrapBuffer.h>
#include <aprinter/base/Likely.h>
#include <aprinter/misc/StringTools.h>
#include <aprinter/net/http/HttpServerCommon.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, typename TheMain, typename RequestHandler, typename UserClientState, typename Params>
class HttpServer {
public:
    struct Object;
    
private:
    class Client;
    
    using TimeType = typename Context::Clock::TimeType;
    using TheNetwork = typename Context::Network;
    using TheTcpListener            = typename TheNetwork::TcpListener;
    using TheTcpListenerQueueParams = typename TheNetwork::TcpListenerQueueParams;
    using TheTcpListenerQueueEntry  = typename TheNetwork::TcpListenerQueueEntry;
    using TheTcpConnection          = typename TheNetwork::TcpConnection;
    
    static size_t const RxBufferSize = TheTcpConnection::RequiredRxBufSize;
    static size_t const TxBufferSize = TheTcpConnection::ProvidedTxBufSize;
    
    // Note, via the ExpectedResponseLength we ensure that we do not overflow the send buffer.
    // This has to be set to a sufficiently large value that accomodates the worst case
    // (100-continue + largest possible response head). The limit really needs to be sufficient,
    // crashes will happen if we do overflow the send buffer.
    
    // Basic checks of parameters.
    static_assert(Params::Net::MaxClients > 0, "");
    static_assert(Params::Net::QueueSize >= 0, "");
    static_assert(Params::MaxRequestLineLength >= 32, "");
    static_assert(Params::MaxHeaderLineLength >= 40, "");
    static_assert(Params::ExpectedResponseLength >= 250, "");
    static_assert(Params::ExpectedResponseLength <= TxBufferSize, "");
    static_assert(Params::MaxRequestHeadLength >= 500, "");
    static_assert(Params::TxChunkHeaderDigits >= 3, "");
    static_assert(Params::TxChunkHeaderDigits <= 8, "");
    
    // Check sizes related to sending by chunked-encoding.
    // - The send buffer needs to be large enough that we can build a chunk with at least 1 byte of payload.
    // - The number of digits for the chunk length needs to be enough for the largest possible chunk.
    static size_t const TxChunkHeaderDigits = Params::TxChunkHeaderDigits;
    static size_t const TxChunkHeaderSize = TxChunkHeaderDigits + 2;
    static size_t const TxChunkFooterSize = 2;
    static size_t const TxChunkOverhead = TxChunkHeaderSize + TxChunkFooterSize;
    static_assert(TxBufferSize > TxChunkOverhead, "");
    static size_t const TxBufferSizeForChunkData = TxBufferSize - TxChunkOverhead;
    static_assert(TxChunkHeaderDigits >= HexDigitsInInt<TxBufferSizeForChunkData>::Value, "");
    
    static TimeType const QueueTimeoutTicks      = Params::Net::QueueTimeout::value()      * Context::Clock::time_freq;
    static TimeType const InactivityTimeoutTicks = Params::Net::InactivityTimeout::value() * Context::Clock::time_freq;
    
public:
    using TheRequestInterface = Client;
    
    static size_t const MaxTxChunkSize = TxBufferSizeForChunkData;
    
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        o->listener.init(c, APRINTER_CB_STATFUNC_T(&HttpServer::listener_accept_handler));
        if (!o->listener.startListening(c, Params::Net::Port, Params::Net::MaxClients, TheTcpListenerQueueParams{Params::Net::QueueSize, QueueTimeoutTicks, o->queue})) {
            TheMain::print_pgm_string(c, AMBRO_PSTR("//HttpServerListenError\n"));
        }
        for (int i = 0; i < Params::Net::MaxClients; i++) {
            o->clients[i].init(c);
        }
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        
        for (int i = 0; i < Params::Net::MaxClients; i++) {
            o->clients[i].deinit(c);
        }
        o->listener.deinit(c);
    }
    
private:
    static void listener_accept_handler (Context c)
    {
        auto *o = Object::self(c);
        
        for (int i = 0; i < Params::Net::MaxClients; i++) {
            Client *cl = &o->clients[i];
            if (cl->m_state == Client::State::NOT_CONNECTED) {
                return cl->accept_connection(c, &o->listener);
            }
        }
    }
    
    static size_t buf_add (size_t start, size_t count)
    {
        size_t x = start + count;
        if (x >= RxBufferSize) {
            x -= RxBufferSize;
        }
        return x;
    }
    
    class Client {
        friend HttpServer;
        
    private:
        enum class State : uint8_t {
            NOT_CONNECTED, WAIT_SEND_BUF_FOR_REQUEST,
            RECV_REQUEST_LINE, RECV_HEADER_LINE,
            HEAD_RECEIVED, USER_GONE,
            DISCONNECT_AFTER_SENDING, CALLING_REQUEST_TERMINATED
        };
        
        enum class RecvState : uint8_t {
            INVALID, NOT_STARTED, RECV_KNOWN_LENGTH,
            RECV_CHUNK_HEADER, RECV_CHUNK_DATA, RECV_CHUNK_TRAILER,
            RECV_TRAILER, COMPLETED
        };
        
        enum class SendState : uint8_t {INVALID, HEAD_NOT_SENT, SEND_BODY, SEND_LAST_CHUNK, COMPLETED};
        
        void accept_rx_data (Context c, size_t amount)
        {
            AMBRO_ASSERT(m_state != State::NOT_CONNECTED)
            AMBRO_ASSERT(amount <= m_rx_buf_length)
            
            m_rx_buf_start = buf_add(m_rx_buf_start, amount);
            m_rx_buf_length -= amount;
            m_connection.acceptReceivedData(c, amount);
        }
        
        void send_string (Context c, char const *str)
        {
            AMBRO_ASSERT(m_state != State::NOT_CONNECTED)
            
            m_connection.copySendData(c, str, strlen(str));
        }
        
        void init (Context c)
        {
            m_last_chunk_length = -1;
            m_chunk_header[TxChunkHeaderDigits+0] = '\r';
            m_chunk_header[TxChunkHeaderDigits+1] = '\n';
            
            m_send_event.init(c, APRINTER_CB_OBJFUNC_T(&Client::send_event_handler, this));
            m_recv_event.init(c, APRINTER_CB_OBJFUNC_T(&Client::recv_event_handler, this));
            m_send_timeout_event.init(c, APRINTER_CB_OBJFUNC_T(&Client::send_timeout_event_handler, this));
            m_recv_timeout_event.init(c, APRINTER_CB_OBJFUNC_T(&Client::recv_timeout_event_handler, this));
            m_combined_timeout_event.init(c, APRINTER_CB_OBJFUNC_T(&Client::combined_timeout_event_handler, this));
            m_connection.init(c, APRINTER_CB_OBJFUNC_T(&Client::connection_error_handler, this),
                                 APRINTER_CB_OBJFUNC_T(&Client::connection_recv_handler, this),
                                 APRINTER_CB_OBJFUNC_T(&Client::connection_send_handler, this));
            m_user = nullptr;
            m_state = State::NOT_CONNECTED;
            m_recv_state = RecvState::INVALID;
            m_send_state = SendState::INVALID;
            m_user_client_state.init(c);
        }
        
        void deinit (Context c)
        {
            m_user_client_state.deinit(c);
            m_connection.deinit(c);
            m_combined_timeout_event.deinit(c);
            m_recv_timeout_event.deinit(c);
            m_send_timeout_event.deinit(c);
            m_recv_event.deinit(c);
            m_send_event.deinit(c);
        }
        
        void accept_connection (Context c, TheTcpListener *listener)
        {
            AMBRO_ASSERT(m_state == State::NOT_CONNECTED)
            
#if APRINTER_DEBUG_HTTP_SERVER
            TheMain::print_pgm_string(c, AMBRO_PSTR("//HttpClientConnected\n"));
#endif
            
            // Accept the connection.
            m_connection.acceptConnection(c, listener);
            
            // Initialzie the RX buffer.
            m_rx_buf_start = 0;
            m_rx_buf_length = 0;
            m_rx_buf_eof = false;
            
            // Go prepare_for_request() very soon through this state for simplicity.
            // Really there will be no waiting.
            m_state = State::WAIT_SEND_BUF_FOR_REQUEST;
            m_send_event.prependNow(c);
        }
        
        void disconnect (Context c)
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(m_state != State::NOT_CONNECTED)
            
            // Inform the user that the request is over, if necessary.
            terminate_user(c);
            
            // Clean up in preparation to accept another client.
            m_connection.reset(c);
            m_combined_timeout_event.unset(c);
            m_recv_timeout_event.unset(c);
            m_send_timeout_event.unset(c);
            m_recv_event.unset(c);
            m_send_event.unset(c);
            m_state = State::NOT_CONNECTED;
            m_recv_state = RecvState::INVALID;
            m_send_state = SendState::INVALID;
            
            // Remind the listener to give any queued connection.
            o->listener.scheduleDequeue(c);
        }
        
        void terminate_user (Context c)
        {
            if (m_user) {
                // Change the state temporarily so that assetions can
                // catch unexpected calls from the user.
                auto state = m_state;
                auto user = m_user;
                m_state = State::CALLING_REQUEST_TERMINATED;
                m_user = nullptr;
                user->requestTerminated(c);
                m_state = state;
            }
        }
        
        void prepare_line_parsing (Context c)
        {
            m_line_length = 0;
            m_line_overflow = false;
        }
        
        void prepare_for_request (Context c)
        {
            AMBRO_ASSERT(!m_user)
            AMBRO_ASSERT(m_recv_state == RecvState::INVALID)
            AMBRO_ASSERT(m_send_state == SendState::INVALID)
            
            // Set initial values to the various request-parsing states.
            m_bad_content_length = false;
            m_have_content_length = false;
            m_have_chunked = false;
            m_bad_transfer_encoding = false;
            m_expect_100_continue = false;
            m_expectation_failed = false;
            m_rem_allowed_length = Params::MaxRequestHeadLength;
            
            // And set some values related to higher-level processing of the request.
            m_have_request_body = false;
            m_close_connection = !Params::Net::AllowPersistent;
            m_resp_status = nullptr;
            m_resp_content_type = nullptr;
            m_user_accepting_request_body = false;
            
            // Prepare for parsing the request as a sequence of lines.
            prepare_line_parsing(c);
            
            // Waiting for space in the send buffer.
            m_state = State::RECV_REQUEST_LINE;
            m_recv_event.prependNow(c);
        }
        
        bool have_request (Context c)
        {
            return (m_state == State::HEAD_RECEIVED || m_state == State::USER_GONE);
        }
        
        void check_for_next_request (Context c)
        {
            if (m_state == State::USER_GONE &&
                m_recv_state == RecvState::COMPLETED &&
                m_send_state == SendState::COMPLETED)
            {
                // The request is processed.
                // If closing is desired, we want to wait until the send buffer is emptied,
                // otherwise just until we have enough space in the send buffer for the next request.
                AMBRO_ASSERT(!m_user)
                m_state = m_close_connection ? State::DISCONNECT_AFTER_SENDING : State::WAIT_SEND_BUF_FOR_REQUEST;
                m_recv_state = RecvState::INVALID;
                m_send_state = SendState::INVALID;
                m_combined_timeout_event.unset(c);
                m_send_event.prependNow(c);
            }
        }
        
        void connection_error_handler (Context c, bool remote_closed)
        {
            AMBRO_ASSERT(m_state != State::NOT_CONNECTED)
            AMBRO_ASSERT(!remote_closed || !m_rx_buf_eof)
            
            // If this is an EOF from the client, just note it and handle it later.
            if (remote_closed) {
                m_rx_buf_eof = true;
                m_recv_event.prependNow(c);
                return;
            }
            
#if APRINTER_DEBUG_HTTP_SERVER
            TheMain::print_pgm_string(c, AMBRO_PSTR("//HttpClientError\n"));
#endif
            disconnect(c);
        }
        
        void connection_recv_handler (Context c, size_t bytes_read)
        {
            AMBRO_ASSERT(m_state != State::NOT_CONNECTED)
            AMBRO_ASSERT(!m_rx_buf_eof)
            AMBRO_ASSERT(bytes_read <= RxBufferSize - m_rx_buf_length)
            
            // Write the received data to the RX buffer.
            size_t write_offset = buf_add(m_rx_buf_start, m_rx_buf_length);
            size_t first_chunk_len = MinValue(bytes_read, (size_t)(RxBufferSize - write_offset));
            m_connection.copyReceivedData(c, m_rx_buf + write_offset, first_chunk_len);
            if (first_chunk_len < bytes_read) {
                m_connection.copyReceivedData(c, m_rx_buf, bytes_read - first_chunk_len);
            }
            m_rx_buf_length += bytes_read;
            
            // Check for things to be done now that we have new data.
            m_recv_event.prependNow(c);
        }
        
        void connection_send_handler (Context c)
        {
            AMBRO_ASSERT(m_state != State::NOT_CONNECTED)
            
            // Check for things to be done now that we have more space.
            m_send_event.prependNow(c);
        }
        
        void send_event_handler (Context c)
        {
            switch (m_state) {
                case State::WAIT_SEND_BUF_FOR_REQUEST: {
                    // When we have sufficient space in the send buffer, start receiving a request.
                    if (m_connection.getSendBufferSpace(c) >= Params::ExpectedResponseLength) {
                        m_send_timeout_event.unset(c);
                        prepare_for_request(c);
                    } else {
                        m_send_timeout_event.appendAfter(c, InactivityTimeoutTicks);
                    }
                } break;
                
                case State::HEAD_RECEIVED:
                case State::USER_GONE: {
                    switch (m_send_state) {
                        case SendState::SEND_BODY: {
                            // If there is any data in the send buffer, start the send timeout, else clear it.
                            if (m_connection.getSendBufferSpace(c) < TxBufferSize) {
                                start_combined_timeout(c, CombinedTimeout::SEND);
                            } else {
                                clear_combined_timeout(c, CombinedTimeout::SEND);
                            }
                            
                            // The user is providing the response body, so call
                            // the callback whenever we may have new space in the buffer.
                            AMBRO_ASSERT(m_user)
                            return m_user->responseBufferEvent(c);
                        } break;
                        
                        case SendState::SEND_LAST_CHUNK: {
                            // Not enough space in the send buffer yet?
                            if (m_connection.getSendBufferSpace(c) < 5) {
                                start_combined_timeout(c, CombinedTimeout::SEND);
                                return;
                            }
                            
                            // Send the terminating chunk with no payload.
                            send_string(c, "0\r\n\r\n");
                            m_connection.pokeSending(c);
                            
                            // Close sending on the connection if needed.
                            if (m_close_connection) {
                                m_connection.closeSending(c);
                            }
                            
                            // Sending the response is now completed.
                            clear_combined_timeout(c, CombinedTimeout::SEND);
                            m_send_state = SendState::COMPLETED;
                            
                            // Maybe we can move on to the next request.
                            check_for_next_request(c);
                        } break;
                        
                        default: break;
                    }
                } break;
                
                case State::DISCONNECT_AFTER_SENDING: {
                    // Disconnect the client once all data has been sent.
                    if (m_connection.getSendBufferSpace(c) >= TxBufferSize) {
                        disconnect(c);
                    } else {
                        m_send_timeout_event.appendAfter(c, InactivityTimeoutTicks);
                    }
                } break;
                
                default: break;
            }
        }
        
        void recv_event_handler (Context c)
        {
            switch (m_state) {
                case State::RECV_REQUEST_LINE: {
                    // Receiving the request line.
                    recv_line(c, m_request_line, Params::MaxRequestLineLength);
                } break;
                
                case State::RECV_HEADER_LINE: {
                    // Receiving a header line.
                    recv_line(c, m_header_line, Params::MaxHeaderLineLength);
                } break;
                
                case State::HEAD_RECEIVED:
                case State::USER_GONE: {
                    switch (m_recv_state) {
                        case RecvState::RECV_CHUNK_HEADER:
                        case RecvState::RECV_CHUNK_TRAILER:
                        case RecvState::RECV_TRAILER: {
                            // Receiving a line.
                            recv_line(c, m_header_line, Params::MaxHeaderLineLength);
                        } break;
                        
                        case RecvState::RECV_KNOWN_LENGTH:
                        case RecvState::RECV_CHUNK_DATA: {
                            // Detect premature EOF from the client.
                            // We don't bother passing any remaining data to the user, this is easier.
                            if (m_rx_buf_eof && m_rx_buf_length < m_rem_req_body_length) {
#if APRINTER_DEBUG_HTTP_SERVER
                                TheMain::print_pgm_string(c, AMBRO_PSTR("//HttpClientEofInData\n"));
#endif
                                return close_gracefully(c, HttpStatusCodes::BadRequest());
                            }
                            
                            // Start the inactivity timeout if more data can be received, else clear it.
                            if (m_rx_buf_length < m_rem_req_body_length && m_rx_buf_length < RxBufferSize) {
                                start_combined_timeout(c, CombinedTimeout::RECV);
                            } else {
                                clear_combined_timeout(c, CombinedTimeout::RECV);
                            }
                            
                            // When the user is accepting the request body, call the callback whenever
                            // we may have new data. Note that even after the user has received the
                            // entire body, we wait for abandonResponseBody() or completeHandling()
                            // before moving on to RecvState::COMPLETED.
                            if (m_user_accepting_request_body) {
                                AMBRO_ASSERT(m_user)
                                return m_user->requestBufferEvent(c);
                            }
                            
                            // Consume/discard data until the entire body is received.
                            if (!m_req_body_recevied) {
                                size_t amount = get_request_body_avail(c);
                                if (amount > 0) {
                                    consume_request_body(c, amount);
                                }
                                return;
                            }
                            
                            // Receiving the request body is now completed.
                            AMBRO_ASSERT(is_combined_timeout_cleared(c, CombinedTimeout::RECV)) // was cleared above because m_rem_req_body_length==0
                            m_recv_state = RecvState::COMPLETED;
                            
                            // Maybe we can move on to the next request.
                            check_for_next_request(c);
                        } break;
                        
                        default: break;
                    }
                } break;
                
                default: break;
            }
        }
        
        void send_timeout_event_handler (Context c)
        {
#if APRINTER_DEBUG_HTTP_SERVER
            TheMain::print_pgm_string(c, AMBRO_PSTR("//HttpClientSendTimeout\n"));
#endif
            
            switch (m_state) {
                case State::WAIT_SEND_BUF_FOR_REQUEST:
                case State::DISCONNECT_AFTER_SENDING: {
                    disconnect(c);
                } break;
                
                case State::HEAD_RECEIVED:
                case State::USER_GONE: {
                    m_send_timeout_expired = true;
                    m_combined_timeout_event.prependNow(c);
                } break;
                
                default: AMBRO_ASSERT(false);
            }
        }
        
        void recv_timeout_event_handler (Context c)
        {
#if APRINTER_DEBUG_HTTP_SERVER
            TheMain::print_pgm_string(c, AMBRO_PSTR("//HttpClientRecvTimeout\n"));
#endif
            
            switch (m_state) {
                case State::RECV_REQUEST_LINE:
                case State::RECV_HEADER_LINE: {
                    // Only send an error response if we received any part of a request.
                    char const *err_resp = HttpStatusCodes::RequestTimeout();
                    if (m_state == State::RECV_REQUEST_LINE && m_line_length == 0) {
                        err_resp = nullptr;
                    }
                    close_gracefully(c, err_resp);
                } break;
                
                case State::HEAD_RECEIVED:
                case State::USER_GONE: {
                    m_recv_timeout_expired = true;
                    m_combined_timeout_event.prependNow(c);
                } break;
                
                default: AMBRO_ASSERT(false);
            }
        }
        
        enum class CombinedTimeout {SEND, RECV};
        
        void start_combined_timeout (Context c, CombinedTimeout which)
        {
            AMBRO_ASSERT(have_request(c))
            
            // Set the right timer. Don't bother clearing the _expired flag, it's not needed.
            if (which == CombinedTimeout::SEND) {
                m_send_timeout_event.appendAfter(c, InactivityTimeoutTicks);
            } else {
                m_recv_timeout_event.appendAfter(c, InactivityTimeoutTicks);
            }
        }
        
        void clear_combined_timeout (Context c, CombinedTimeout which)
        {
            AMBRO_ASSERT(have_request(c))
            
            // Stop the right timer. Clear the _expired flag, so we know the timeout is inactive not expired.
            if (which == CombinedTimeout::SEND) {
                m_send_timeout_event.unset(c);
                m_send_timeout_expired = false;
            } else {
                m_recv_timeout_event.unset(c);
                m_recv_timeout_expired = false;
            }
            
            // Schedule an expiration check. Because we may need to timeout right now,
            // if one timeout was cleared but the other one is already expired.
            m_combined_timeout_event.prependNow(c);
        }
        
        bool is_combined_timeout_cleared (Context c, CombinedTimeout which)
        {
            AMBRO_ASSERT(have_request(c))
            
            if (which == CombinedTimeout::SEND) {
                return (!m_send_timeout_event.isSet(c) && !m_send_timeout_expired);
            } else {
                return (!m_recv_timeout_event.isSet(c) && !m_recv_timeout_expired);
            }
        }
        
        void combined_timeout_event_handler (Context c)
        {
            AMBRO_ASSERT(have_request(c))
            
            // In these states we separately track inactivity in sending and receiving.
            // We disconnect the client only when the inactivity timer is expired for
            // all I/O directions that are currently active, and at least one direction
            // is active.
            
            bool send_active = sending_response(c);
            bool recv_active = receiving_request_body(c);
            
            if (send_active || recv_active) {
                bool send_expired = !send_active || (!m_send_timeout_event.isSet(c) && m_send_timeout_expired);
                bool recv_expired = !recv_active || (!m_recv_timeout_event.isSet(c) && m_recv_timeout_expired);
                
                if (send_expired && recv_expired) {
                    close_gracefully(c, HttpStatusCodes::RequestTimeout());
                }
            }
        }
        
        void recv_line (Context c, char *line_buf, size_t line_buf_size)
        {
            // Examine the received data, looking for a newline and copying
            // characters into line_buf.
            size_t pos = 0;
            bool end_of_line = false;
            
            char *data = m_rx_buf + m_rx_buf_start;
            while (pos < m_rx_buf_length) {
                char ch = *data++;
                pos++;
                
                if (AMBRO_UNLIKELY(data == m_rx_buf + RxBufferSize)) {
                    data = m_rx_buf;
                }
                
                if (AMBRO_UNLIKELY(m_line_length >= line_buf_size)) {
                    m_line_overflow = true;
                } else {
                    line_buf[m_line_length++] = ch;
                }
                
                if (ch == '\n') {
                    end_of_line = true;
                    break;
                }
            }
            
            // Adjust the RX buffer, accepting all the data we have looked at.
            accept_rx_data(c, pos);
            
            // Detect too long request bodies / lines.
            if (pos > m_rem_allowed_length) {
                return line_allowed_length_exceeded(c);
            }
            m_rem_allowed_length -= pos;
            
            // No newline yet?
            if (!end_of_line) {
                // If no EOF either, wait for more data.
                if (!m_rx_buf_eof) {
                    return line_not_received_yet(c);
                }
                
                // No newline but got EOF. Pass the remainign data.
                // Be careful that the line is null-terminated properly
                // and raise overflow if there's no space for a null.
                if (m_line_length < line_buf_size) {
                    m_line_length++;
                } else {
                    m_line_overflow = true;
                }
            }
            
            // Null-terminate the line, striping the \n and possibly an \r.
            size_t length = m_line_length - 1;
            if (length > 0 && line_buf[length - 1] == '\r') {
                length--;
            }
            line_buf[length] = '\0';
            
            // Adjust line-parsing state for parsing another line.
            bool overflow = m_line_overflow;
            prepare_line_parsing(c);
            
            // Delegate the interpretation of the line to another function.
            line_received(c, length, overflow, !end_of_line);
        }
        
        void line_allowed_length_exceeded (Context c)
        {
            TheMain::print_pgm_string(c, AMBRO_PSTR("//HttpClientRequestTooLong\n"));
            return close_gracefully(c, HttpStatusCodes::RequestHeaderFieldsTooLarge());
        }
        
        void line_not_received_yet (Context c)
        {
            if (have_request(c)) {
                start_combined_timeout(c, CombinedTimeout::RECV);
            } else {
                m_recv_timeout_event.appendAfter(c, InactivityTimeoutTicks);
            }
        }
        
        void line_received (Context c, size_t length, bool overflow, bool eof)
        {
            if (eof) {
#if APRINTER_DEBUG_HTTP_SERVER
                TheMain::print_pgm_string(c, AMBRO_PSTR("//HttpClientEofInLine\n"));
#endif
                // Respond with BadRequest, except when EOF was seen where a request would start.
                char const *err_resp = HttpStatusCodes::BadRequest();
                if (m_state == State::RECV_REQUEST_LINE && length == 0) {
                    err_resp = nullptr;
                }
                return close_gracefully(c, err_resp);
            }
            
            switch (m_state) {
                case State::RECV_REQUEST_LINE: {
                    // Remember the request line and move on to parsing the header lines.
                    m_request_line_overflow = overflow;
                    m_state = State::RECV_HEADER_LINE;
                    m_recv_event.prependNow(c);
                } break;
                
                case State::RECV_HEADER_LINE: {
                    // An empty line terminates the request head.
                    if (length == 0) {
                        m_recv_timeout_event.unset(c);
                        return request_head_received(c);
                    }
                    
                    // Extract and remember any useful information the header and continue parsing.
                    if (!overflow) {
                        handle_header(c, m_header_line);
                    }
                    m_recv_event.prependNow(c);
                } break;
                
                case State::HEAD_RECEIVED:
                case State::USER_GONE: {
                    AMBRO_ASSERT(!m_req_body_recevied)
                    
                    switch (m_recv_state) {
                        case RecvState::RECV_CHUNK_HEADER: {
                            // Check for errors in line parsing.
                            if (overflow) {
                                TheMain::print_pgm_string(c, AMBRO_PSTR("//HttpClientBadChunkLine\n"));
                                return close_gracefully(c, HttpStatusCodes::BadRequest());
                            }
                            
                            // Parse the chunk length.
                            uint64_t value;
                            if (!StringParseHexadecimal(MemRef(m_header_line, length), &value)) {
                                TheMain::print_pgm_string(c, AMBRO_PSTR("//HttpClientBadChunkLine\n"));
                                return close_gracefully(c, HttpStatusCodes::BadRequest());
                            }
                            
                            if (value == 0) {
                                // This is the final zero chunk, move on to receiving the trailing headers.
                                m_recv_state = RecvState::RECV_TRAILER;
                                m_rem_allowed_length = Params::MaxRequestHeadLength;
                            } else {
                                // Remember the chunk length and continue in event_handler.
                                m_recv_state = RecvState::RECV_CHUNK_DATA;
                                m_rem_req_body_length = value;
                            }
                            m_recv_event.prependNow(c);
                        } break;
                        
                        case RecvState::RECV_CHUNK_TRAILER: {
                            // This is supposed to be just an empty line following the chunk payload.
                            if (length > 0) {
                                TheMain::print_pgm_string(c, AMBRO_PSTR("//HttpClientBadChunkTrailer\n"));
                                return close_gracefully(c, HttpStatusCodes::BadRequest());
                            }
                            
                            // Now read the other trailer lines.
                            m_recv_state = RecvState::RECV_CHUNK_HEADER;
                            m_rem_allowed_length = Params::MaxHeaderLineLength;
                            m_recv_event.prependNow(c);
                        } break;
                        
                        case RecvState::RECV_TRAILER: {
                            if (length == 0) {
                                // End of trailer, go via this state for simplicity.
                                m_recv_state = RecvState::RECV_CHUNK_DATA;
                                m_rem_req_body_length = 0;
                                m_req_body_recevied = true;
                            }
                            m_recv_event.prependNow(c);
                        } break;
                        
                        default: AMBRO_ASSERT(false);
                    }
                } break;
                
                default: AMBRO_ASSERT(false);
            }
        }
        
        void handle_header (Context c, char const *header)
        {
            if (StringRemoveHttpHeader(&header, "content-length")) {
                char *endptr;
                unsigned long long int value = strtoull(header, &endptr, 10);
                if (endptr == header || *endptr != '\0' || m_have_content_length) {
                    m_bad_content_length = true;
                } else {
                    m_have_content_length = true;
                    m_rem_req_body_length = value;
                }
            }
            else if (StringRemoveHttpHeader(&header, "transfer-encoding")) {
                if (!MemEqualsCaseIns(header, "identity")) {
                    if (!MemEqualsCaseIns(header, "chunked") || m_have_chunked) {
                        m_bad_transfer_encoding = true;
                    } else {
                        m_have_chunked = true;
                    }
                }
            }
            else if (StringRemoveHttpHeader(&header, "expect")) {
                if (!MemEqualsCaseIns(header, "100-continue")) {
                    m_expectation_failed = true;
                } else {
                    m_expect_100_continue = true;
                }
            }
            else if (StringRemoveHttpHeader(&header, "connection")) {
                StringIterHttpTokens(header, [this](MemRef token) {
                    if (MemEqualsCaseIns(token, "close")) {
                        m_close_connection = true;
                    }
                });
            }
        }
        
        void request_head_received (Context c)
        {
            AMBRO_ASSERT(m_recv_state == RecvState::INVALID)
            AMBRO_ASSERT(m_send_state == SendState::INVALID)
            AMBRO_ASSERT(!m_user)
            AMBRO_ASSERT(!m_send_timeout_event.isSet(c))
            AMBRO_ASSERT(!m_recv_timeout_event.isSet(c))
            AMBRO_ASSERT(!m_combined_timeout_event.isSet(c))
            
            char const *status = HttpStatusCodes::InternalServerError();
            do {
                // Check for request line overflow.
                if (m_request_line_overflow) {
                    status = HttpStatusCodes::UriTooLong();
                    goto error;
                }
                
                // Start parsing the request line.
                char *buf = m_request_line;
                
                // Extract the request method.
                char *first_space = strchr(buf, ' ');
                if (!first_space) {
                    status = HttpStatusCodes::BadRequest();
                    goto error;
                }
                *first_space = '\0';
                m_request_method = buf;
                buf = first_space + 1;
                
                // Extract the request path.
                char *second_space = strchr(buf, ' ');
                if (!second_space) {
                    status = HttpStatusCodes::BadRequest();
                    goto error;
                }
                *second_space = '\0';
                m_request_path = buf;
                buf = second_space + 1;
                
                // Remove HTTP/ prefix from the version.
                if (!StringRemovePrefix(&buf, "HTTP/")) {
                    status = HttpStatusCodes::HttpVersionNotSupported();
                    goto error;
                }
                
                // Parse the major version.
                char *endptr1;
                unsigned long int http_major = strtoul(buf, &endptr1, 10);
                if (endptr1 == buf || *endptr1 != '.') {
                    status = HttpStatusCodes::HttpVersionNotSupported();
                    goto error;
                }
                buf = endptr1 + 1;
                
                // Parse the minor version.
                char *endptr2;
                unsigned long int http_minor = strtoul(buf, &endptr2, 10);
                if (endptr2 == buf || *endptr2 != '\0') {
                    status = HttpStatusCodes::HttpVersionNotSupported();
                    goto error;
                }
                
                // Check the version. We want 1.X where X >= 1.
                if (http_major != 1 || http_minor == 0) {
                    status = HttpStatusCodes::HttpVersionNotSupported();
                    goto error;
                }
                
                // Check for errors concerning headers describing the request-body.
                if (m_bad_content_length || m_bad_transfer_encoding) {
                    status = HttpStatusCodes::BadRequest();
                    goto error;
                }
                
                // Check for failed expectations.
                if (m_expectation_failed) {
                    status = HttpStatusCodes::ExpectationFailed();
                    goto error;
                }
                
                // Determine if we are receiving a request body.
                // Note: we allow both "content-length" and "transfer-encoding: chunked"
                // at the same time, where chunked takes precedence.
                if (m_have_content_length || m_have_chunked) {
                    m_have_request_body = true;
                }
                
                // Change state before passing the request to the user.
                m_state = State::HEAD_RECEIVED;
                m_send_state = SendState::HEAD_NOT_SENT;
                m_recv_state = m_have_request_body ? RecvState::NOT_STARTED : RecvState::COMPLETED;
                m_send_timeout_expired = false;
                m_recv_timeout_expired = false;
                
                print_debug_request_info(c);
                
                // Call the user's request handler.
                return RequestHandler::call(c, this);
            } while (false);
            
        error:
            // Respond with an error and close the connection.
            close_gracefully(c, status);
        }
        
        void print_debug_request_info (Context c)
        {
#if APRINTER_DEBUG_HTTP_SERVER
            auto *output = TheMain::get_msg_output(c);
            output->reply_append_pstr(c, AMBRO_PSTR("//HttpRequest "));
            output->reply_append_str(c, getMethod(c));
            output->reply_append_ch(c, ' ');
            output->reply_append_str(c, getPath(c));
            output->reply_append_pstr(c, AMBRO_PSTR(" body="));
            output->reply_append_ch(c, hasRequestBody(c) ? '1' : '0');
            output->reply_append_ch(c, '\n');
            output->reply_poke(c);
#endif
        }
        
        bool receiving_request_body (Context c)
        {
            return (m_recv_state == RecvState::RECV_KNOWN_LENGTH   ||
                    m_recv_state == RecvState::RECV_CHUNK_HEADER   ||
                    m_recv_state == RecvState::RECV_CHUNK_DATA     ||
                    m_recv_state == RecvState::RECV_CHUNK_TRAILER  ||
                    m_recv_state == RecvState::RECV_TRAILER);
        }
        
        void start_receiving_request_body (Context c, bool user_accepting)
        {
            AMBRO_ASSERT(m_recv_state == RecvState::NOT_STARTED)
            AMBRO_ASSERT(!user_accepting || m_user)
            AMBRO_ASSERT(!m_user_accepting_request_body)
            AMBRO_ASSERT(m_have_request_body)
            
            // Send 100-continue if needed.
            if (m_expect_100_continue && m_send_state == SendState::HEAD_NOT_SENT) {
                send_string(c, "HTTP/1.1 100 Continue\r\n\r\n");
                m_connection.pokeSending(c);
            }
            
            // Remember if the user is accepting the body (else we're discarding it).
            m_user_accepting_request_body = user_accepting;
            
            // Start receiving the request body, chunked or known-length.
            if (m_have_chunked) {
                m_recv_state = RecvState::RECV_CHUNK_HEADER;
                m_rem_allowed_length = Params::MaxHeaderLineLength;
                m_req_body_recevied = false;
            } else {
                m_recv_state = RecvState::RECV_KNOWN_LENGTH;
                m_req_body_recevied = (m_rem_req_body_length == 0);
            }
            m_recv_event.prependNow(c);
        }
        
        size_t get_request_body_avail (Context c)
        {
            AMBRO_ASSERT(receiving_request_body(c))
            
            if (m_recv_state == RecvState::RECV_KNOWN_LENGTH || m_recv_state == RecvState::RECV_CHUNK_DATA) {
                return (size_t)MinValue((uint64_t)m_rx_buf_length, m_rem_req_body_length);
            } else {
                return 0;
            }
        }
        
        void consume_request_body (Context c, size_t amount)
        {
            AMBRO_ASSERT(m_recv_state == RecvState::RECV_KNOWN_LENGTH || m_recv_state == RecvState::RECV_CHUNK_DATA)
            AMBRO_ASSERT(amount > 0)
            AMBRO_ASSERT(amount <= m_rx_buf_length)
            AMBRO_ASSERT(amount <= m_rem_req_body_length)
            AMBRO_ASSERT(!m_req_body_recevied)
            
            // If the RX buffer becomes no longer full, start the inactivity timeout.
            if (m_rx_buf_length == RxBufferSize) {
                start_combined_timeout(c, CombinedTimeout::RECV);
            }
            
            // Adjust RX buffer and remaining-data length.
            accept_rx_data(c, amount);
            m_rem_req_body_length -= amount;
            
            // End of known-length body or chunk?
            if (m_rem_req_body_length == 0) {
                if (m_recv_state == RecvState::RECV_KNOWN_LENGTH) {
                    m_req_body_recevied = true;
                } else {
                    m_recv_state = RecvState::RECV_CHUNK_TRAILER;
                    m_rem_allowed_length = Params::MaxHeaderLineLength;
                }
                m_recv_event.prependNow(c);
            }
        }
        
        void abandon_request_body (Context c)
        {
            AMBRO_ASSERT(m_recv_state != RecvState::INVALID)
            
            if (m_recv_state == RecvState::NOT_STARTED) {
                // Start receiving the request body now, discarding all data.
                start_receiving_request_body(c, false);
            }
            else if (m_recv_state != RecvState::COMPLETED) {
                // Discard any remaining request-body data.
                m_user_accepting_request_body = false;
                m_recv_event.prependNow(c);
            }
        }
        
        void close_gracefully (Context c, char const *resp_status)
        {
            AMBRO_ASSERT(m_state != State::NOT_CONNECTED)
            AMBRO_ASSERT(m_state != State::DISCONNECT_AFTER_SENDING)
            AMBRO_ASSERT(!resp_status || m_state != State::WAIT_SEND_BUF_FOR_REQUEST)
            
            // Terminate the request with the user, if any.
            terminate_user(c);
            
            // Send an error response if desired and possible.
            if (resp_status && (m_send_state == SendState::INVALID || m_send_state == SendState::HEAD_NOT_SENT)) {
                send_response(c, resp_status, true, nullptr, true);
            }
            
            // Disconnect the client after all data has been sent.
            m_state = State::DISCONNECT_AFTER_SENDING;
            m_recv_state = RecvState::INVALID;
            m_send_state = SendState::INVALID;
            m_send_timeout_event.unset(c);
            m_recv_timeout_event.unset(c);
            m_combined_timeout_event.unset(c);
            m_send_event.prependNow(c);
        }
        
        void send_response (Context c, char const *resp_status, bool send_status_as_body, char const *content_type=nullptr, bool connection_close=false)
        {
            if (!resp_status) {
                resp_status = HttpStatusCodes::Okay();
            }
            if (!content_type) {
                content_type = HttpContentTypes::TextPlainUtf8();
            }
            
            // Send the response head.
            send_string(c, "HTTP/1.1 ");
            send_string(c, resp_status);
            if (connection_close) {
                send_string(c, "\r\nConnection: close");
            }
            send_string(c, "\r\nServer: Aprinter\r\nContent-Type: ");
            send_string(c, content_type);
            send_string(c, "\r\n");
            if (send_status_as_body) {
                send_string(c, "Content-Length: ");
                char length_buf[12];
                sprintf(length_buf, "%d", (int)(strlen(resp_status) + 1));
                send_string(c, length_buf);
            } else {
                send_string(c, "Transfer-Encoding: chunked");
            }
            send_string(c, "\r\n\r\n");
            
            // If desired send the status as the response body.
            if (send_status_as_body) {
                send_string(c, resp_status);
                send_string(c, "\n");
            }
            
            m_connection.pokeSending(c);
        }
        
        bool sending_response (Context c)
        {
            return (m_send_state == SendState::SEND_BODY ||
                    m_send_state == SendState::SEND_LAST_CHUNK);
        }
        
        void abandon_response_body (Context c)
        {
            AMBRO_ASSERT(m_send_state != SendState::INVALID)
            
            if (m_send_state == SendState::HEAD_NOT_SENT) {
                // The response head has not been sent.
                // Send the response now, with the status as the body.
                send_response(c, m_resp_status, true, nullptr, m_close_connection);
                
                // Close sending on the connection if needed.
                if (m_close_connection) {
                    m_connection.closeSending(c);
                }
                
                // Make the state transition.
                AMBRO_ASSERT(is_combined_timeout_cleared(c, CombinedTimeout::SEND)) // was never started
                m_send_state = SendState::COMPLETED;
            }
            else if (m_send_state == SendState::SEND_BODY) {
                // The response head has been sent and possibly some body,
                // but we need to terminate it with a zero-chunk.
                m_send_state = SendState::SEND_LAST_CHUNK;
                m_send_event.prependNow(c);
            }
        }
        
    public:
        struct RequestUserCallback {
            virtual void requestTerminated (Context c) = 0;
            virtual void requestBufferEvent (Context c) {};
            virtual void responseBufferEvent (Context c) {};
        };
        
        struct RequestBodyBufferState {
            WrapBuffer data;
            size_t length;
            bool eof;
        };
        
        struct ResponseBodyBufferState {
            WrapBuffer data;
            size_t length;
        };
        
        UserClientState * getUserClientState (Context c)
        {
            return &m_user_client_state;
        }
        
        char const * getMethod (Context c)
        {
            AMBRO_ASSERT(m_state == State::HEAD_RECEIVED)
            
            return m_request_method;
        }
        
        char const * getPath (Context c)
        {
            AMBRO_ASSERT(m_state == State::HEAD_RECEIVED)
            
            return m_request_path;
        }
        
        bool hasRequestBody (Context c)
        {
            AMBRO_ASSERT(m_state == State::HEAD_RECEIVED)
            
            return m_have_request_body;
        }
        
        void setCallback (Context c, RequestUserCallback *callback)
        {
            AMBRO_ASSERT(m_state == State::HEAD_RECEIVED)
            AMBRO_ASSERT(!m_user)
            AMBRO_ASSERT(callback)
            
            // Remember the user-callback object.
            m_user = callback;
        }
        
        void completeHandling (Context c)
        {
            AMBRO_ASSERT(m_state == State::HEAD_RECEIVED)
            AMBRO_ASSERT(m_recv_state != RecvState::INVALID)
            AMBRO_ASSERT(m_send_state != SendState::INVALID)
            
            // Remember that the user is gone.
            m_state = State::USER_GONE;
            m_user = nullptr;
            
            // Make sure that sending the response proceeds.
            abandon_response_body(c);
            
            // Make sure that receiving the request body proceeds.
            // Note: this is done after abandon_response_body so that we
            // don't unnecessarily send 100-continue.
            abandon_request_body(c);
            
            // Check if the next request can begin already; we may have to complete
            // receiving the request body and sending the response.
            check_for_next_request(c);
        }
        
        void adoptRequestBody (Context c)
        {
            AMBRO_ASSERT(m_state == State::HEAD_RECEIVED)
            AMBRO_ASSERT(m_recv_state == RecvState::NOT_STARTED)
            AMBRO_ASSERT(m_user)
            
            // Start receiving the request body, passing it to the user.
            start_receiving_request_body(c, true);
        }
        
        void abandonRequestBody (Context c)
        {
            AMBRO_ASSERT(m_state == State::HEAD_RECEIVED)
            AMBRO_ASSERT(m_recv_state == RecvState::NOT_STARTED || receiving_request_body(c))
            AMBRO_ASSERT(m_recv_state == RecvState::NOT_STARTED || m_user_accepting_request_body)
            
            abandon_request_body(c);
        }
        
        RequestBodyBufferState getRequestBodyBufferState (Context c)
        {
            AMBRO_ASSERT(m_state == State::HEAD_RECEIVED)
            AMBRO_ASSERT(receiving_request_body(c))
            AMBRO_ASSERT(m_user_accepting_request_body)
            
            WrapBuffer data = WrapBuffer::Make(RxBufferSize - m_rx_buf_start, m_rx_buf + m_rx_buf_start, m_rx_buf);
            size_t data_len = get_request_body_avail(c);
            return RequestBodyBufferState{data, data_len, m_req_body_recevied};
        }
        
        void acceptRequestBodyData (Context c, size_t length)
        {
            AMBRO_ASSERT(m_state == State::HEAD_RECEIVED)
            AMBRO_ASSERT(receiving_request_body(c))
            AMBRO_ASSERT(m_user_accepting_request_body)
            AMBRO_ASSERT(length <= get_request_body_avail(c))
            
            if (length > 0) {
                consume_request_body(c, length);
            }
        }
        
        void setResponseStatus (Context c, char const *status)
        {
            AMBRO_ASSERT(m_state == State::HEAD_RECEIVED)
            AMBRO_ASSERT(m_send_state == SendState::HEAD_NOT_SENT)
            AMBRO_ASSERT(status)
            
            m_resp_status = status;
        }
        
        void setResponseContentType (Context c, char const *content_type)
        {
            AMBRO_ASSERT(m_state == State::HEAD_RECEIVED)
            AMBRO_ASSERT(m_send_state == SendState::HEAD_NOT_SENT)
            AMBRO_ASSERT(content_type)
            
            m_resp_content_type = content_type;
        }
        
        void adoptResponseBody (Context c)
        {
            AMBRO_ASSERT(m_state == State::HEAD_RECEIVED)
            AMBRO_ASSERT(m_send_state == SendState::HEAD_NOT_SENT)
            AMBRO_ASSERT(m_user)
            
            // Send the response head.
            send_response(c, m_resp_status, false, m_resp_content_type, m_close_connection);
            
            // The user will now be producing the response body.
            m_send_state = SendState::SEND_BODY;
            m_send_event.prependNow(c);
        }
        
        void abandonResponseBody (Context c)
        {
            AMBRO_ASSERT(m_state == State::HEAD_RECEIVED)
            AMBRO_ASSERT(m_send_state == SendState::HEAD_NOT_SENT || m_send_state == SendState::SEND_BODY)
            AMBRO_ASSERT(m_send_state == SendState::HEAD_NOT_SENT || m_user)
            
            abandon_response_body(c);
        }
        
        ResponseBodyBufferState getResponseBodyBufferState (Context c)
        {
            AMBRO_ASSERT(m_state == State::HEAD_RECEIVED)
            AMBRO_ASSERT(m_send_state == SendState::SEND_BODY)
            AMBRO_ASSERT(m_user)
            
            WrapBuffer con_space_buffer;
            size_t con_space_avail = m_connection.getSendBufferSpace(c, &con_space_buffer);
            
            if (con_space_avail <= TxChunkOverhead) {
                return ResponseBodyBufferState{WrapBuffer::Make(nullptr), 0};
            }
            return ResponseBodyBufferState{con_space_buffer.subFrom(TxChunkHeaderSize), con_space_avail - TxChunkOverhead};
        }
        
        void provideResponseBodyData (Context c, size_t length)
        {
            AMBRO_ASSERT(m_state == State::HEAD_RECEIVED)
            AMBRO_ASSERT(m_send_state == SendState::SEND_BODY)
            AMBRO_ASSERT(m_user)
            AMBRO_ASSERT(length > 0)
            
            // Get the send buffer reference and sanity check the length / space.
            WrapBuffer con_space_buffer;
            size_t con_space_avail = m_connection.getSendBufferSpace(c, &con_space_buffer);
            AMBRO_ASSERT(con_space_avail >= TxChunkOverhead)
            AMBRO_ASSERT(length <= con_space_avail - TxChunkOverhead)
            
            // Prepare the chunk header, with speed.
            if (AMBRO_UNLIKELY(length != m_last_chunk_length)) {
                size_t rem_length = length;
                for (int i = TxChunkHeaderDigits-1; i >= 0; i--) {
                    char digit_num = rem_length & 0xF;
                    m_chunk_header[i] = (digit_num < 10) ? ('0' + digit_num) : ('A' + (digit_num - 10));
                    rem_length >>= 4;
                }
                m_last_chunk_length = length;
            }
            
            // Write the chunk header and footer.
            con_space_buffer.copyIn(0, TxChunkHeaderSize, m_chunk_header);
            con_space_buffer.copyIn(TxChunkHeaderSize + length, 2, m_chunk_header+TxChunkHeaderDigits);
            
            // Submit data to the connection and poke sending.
            m_connection.copySendData(c, nullptr, TxChunkOverhead + length);
            m_connection.pokeSending(c);
            
            // If we did not have any data in the send buffer, start the send timeout.
            if (con_space_avail == TxBufferSize) {
                start_combined_timeout(c, CombinedTimeout::SEND);
            }
        }
        
    private:
        typename Context::EventLoop::QueuedEvent m_send_event;
        typename Context::EventLoop::QueuedEvent m_recv_event;
        typename Context::EventLoop::TimedEvent m_send_timeout_event;
        typename Context::EventLoop::TimedEvent m_recv_timeout_event;
        typename Context::EventLoop::QueuedEvent m_combined_timeout_event;
        TheTcpConnection m_connection;
        RequestUserCallback *m_user;
        UserClientState m_user_client_state;
        size_t m_rx_buf_start;
        size_t m_rx_buf_length;
        size_t m_line_length;
        size_t m_rem_allowed_length;
        size_t m_last_chunk_length;
        uint64_t m_rem_req_body_length;
        char const *m_request_method;
        char const *m_request_path;
        char const *m_resp_status;
        char const *m_resp_content_type;
        State m_state;
        RecvState m_recv_state;
        SendState m_send_state;
        bool m_line_overflow;
        bool m_request_line_overflow;
        bool m_bad_content_length;
        bool m_have_content_length;
        bool m_have_chunked;
        bool m_bad_transfer_encoding;
        bool m_expect_100_continue;
        bool m_expectation_failed;
        bool m_have_request_body;
        bool m_close_connection;
        bool m_req_body_recevied;
        bool m_user_accepting_request_body;
        bool m_rx_buf_eof;
        bool m_send_timeout_expired;
        bool m_recv_timeout_expired;
        char m_rx_buf[RxBufferSize];
        char m_request_line[Params::MaxRequestLineLength];
        char m_header_line[Params::MaxHeaderLineLength];
        char m_chunk_header[TxChunkHeaderSize];
    };
    
public:
    struct Object : public ObjBase<HttpServer, ParentObject, EmptyTypeList> {
        TheTcpListener listener;
        TheTcpListenerQueueEntry queue[Params::Net::QueueSize];
        Client clients[Params::Net::MaxClients];
    };
};

APRINTER_ALIAS_STRUCT(HttpServerNetParams, (
    APRINTER_AS_VALUE(uint16_t, Port),
    APRINTER_AS_VALUE(int, MaxClients),
    APRINTER_AS_VALUE(int, QueueSize),
    APRINTER_AS_VALUE(bool, AllowPersistent),
    APRINTER_AS_TYPE(QueueTimeout),
    APRINTER_AS_TYPE(InactivityTimeout)
))

APRINTER_ALIAS_STRUCT_EXT(HttpServerService, (
    APRINTER_AS_TYPE(Net),
    APRINTER_AS_VALUE(size_t, MaxRequestLineLength),
    APRINTER_AS_VALUE(size_t, MaxHeaderLineLength),
    APRINTER_AS_VALUE(size_t, ExpectedResponseLength),
    APRINTER_AS_VALUE(size_t, MaxRequestHeadLength),
    APRINTER_AS_VALUE(size_t, TxChunkHeaderDigits)
), (
    template <typename Context, typename ParentObject, typename TheMain, typename RequestHandler, typename UserClientState>
    using Server = HttpServer<Context, ParentObject, TheMain, RequestHandler, UserClientState, HttpServerService>;
))

#include <aprinter/EndNamespace.h>

#endif
