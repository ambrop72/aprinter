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
#include <inttypes.h>

#include <aprinter/meta/MinMax.h>
#include <aprinter/meta/BitsInInt.h>
#include <aprinter/meta/AliasStruct.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/Callback.h>
#include <aprinter/base/ProgramMemory.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/WrapBuffer.h>
#include <aprinter/misc/StringTools.h>
#include <aprinter/printer/HttpServerCommon.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, typename ThePrinterMain, typename RequestHandler, typename UserClientState, typename Params>
class HttpServer {
public:
    struct Object;
    
public:
    using TheRequestInterface = HttpRequestInterface<Context, UserClientState>;
    
private:
    using TimeType = typename Context::Clock::TimeType;
    using TheNetwork = typename Context::Network;
    using TheTcpListener            = typename TheNetwork::TcpListener;
    using TheTcpListenerQueueParams = typename TheNetwork::TcpListenerQueueParams;
    using TheTcpListenerQueueEntry  = typename TheNetwork::TcpListenerQueueEntry;
    using TheTcpConnection          = typename TheNetwork::TcpConnection;
    using RequestUserCallback     = typename TheRequestInterface::RequestUserCallback; 
    using RequestBodyBufferState  = typename TheRequestInterface::RequestBodyBufferState;
    using ResponseBodyBufferState = typename TheRequestInterface::ResponseBodyBufferState;
    
    static size_t const RxBufferSize = TheTcpConnection::RequiredRxBufSize;
    static size_t const TxBufferSize = TheTcpConnection::ProvidedTxBufSize;
    
    // Note, via the ExpectedResponseLength we ensure that we do not overflow the send buffer.
    // This has to be set to a sufficiently large value that accomodates the worst case
    // (100-continue + largest possible response head). The limit really needs to be sufficient,
    // crashes will happen if we do overflow the send buffer.
    
    // Basic checks of parameters.
    static_assert(Params::MaxClients > 0, "");
    static_assert(Params::QueueSize >= 0, "");
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
    static size_t const TxChunkHeaderSize = Params::TxChunkHeaderDigits + 2;
    static size_t const TxChunkFooterSize = 2;
    static size_t const TxChunkOverhead = TxChunkHeaderSize + TxChunkFooterSize;
    static_assert(TxBufferSize > TxChunkOverhead, "");
    static size_t const TxBufferSizeForChunkData = TxBufferSize - TxChunkOverhead;
    static_assert(Params::TxChunkHeaderDigits >= HexDigitsInInt<TxBufferSizeForChunkData>::Value, "");
    
    static TimeType const QueueTimeoutTicks = Params::QueueTimeout::value() * Context::Clock::time_freq;
    
public:
    static size_t const MaxTxChunkSize = TxBufferSizeForChunkData;
    
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        o->listener.init(c, APRINTER_CB_STATFUNC_T(&HttpServer::listener_accept_handler));
        if (!o->listener.startListening(c, Params::Port, Params::MaxClients, TheTcpListenerQueueParams{Params::QueueSize, QueueTimeoutTicks, o->queue})) {
            ThePrinterMain::print_pgm_string(c, AMBRO_PSTR("//HttpServerListenError\n"));
        }
        for (int i = 0; i < Params::MaxClients; i++) {
            o->clients[i].init(c);
        }
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        
        for (int i = 0; i < Params::MaxClients; i++) {
            o->clients[i].deinit(c);
        }
        o->listener.deinit(c);
    }
    
private:
    static bool listener_accept_handler (Context c)
    {
        auto *o = Object::self(c);
        
        for (int i = 0; i < Params::MaxClients; i++) {
            Client *cl = &o->clients[i];
            if (cl->m_state == Client::State::NOT_CONNECTED) {
                cl->accept_connection(c, &o->listener);
                return true;
            }
        }
        ThePrinterMain::print_pgm_string(c, AMBRO_PSTR("//HttpServerAcceptNoSlot\n"));
        return false;
    }
    
    static size_t buf_add (size_t start, size_t count)
    {
        size_t x = start + count;
        if (x >= RxBufferSize) {
            x -= RxBufferSize;
        }
        return x;
    }
    
    struct Client : public TheRequestInterface {
        enum class State : uint8_t {
            NOT_CONNECTED, WAIT_SEND_BUF_FOR_REQUEST,
            RECV_REQUEST_LINE, RECV_HEADER_LINE, HEAD_RECEIVED, USER_GONE,
            DISCONNECT_AFTER_SENDING, CALLING_REQUEST_TERMINATED
        };
        
        enum class RecvState : uint8_t {INVALID, NOT_STARTED, RECV_KNOWN_LENGTH, RECV_CHUNK_HEADER, RECV_CHUNK_DATA, COMPLETED};
        
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
            m_send_event.init(c, APRINTER_CB_OBJFUNC_T(&Client::send_event_handler, this));
            m_recv_event.init(c, APRINTER_CB_OBJFUNC_T(&Client::recv_event_handler, this));
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
            m_recv_event.deinit(c);
            m_send_event.deinit(c);
        }
        
        void accept_connection (Context c, TheTcpListener *listener)
        {
            AMBRO_ASSERT(m_state == State::NOT_CONNECTED)
            
#if APRINTER_DEBUG_HTTP_SERVER
            ThePrinterMain::print_pgm_string(c, AMBRO_PSTR("//HttpClientConnected\n"));
#endif
            
            // Accept the connection.
            m_connection.acceptConnection(c, listener);
            
            // Initialzie the RX buffer.
            m_rx_buf_start = 0;
            m_rx_buf_length = 0;
            m_rx_buf_eof = false;
            
            // Waiting for space in the send buffer.
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
            // Note, m_null_in_line is not cleared!
            m_line_length = 0;
            m_line_overflow = false;
        }
        
        void prepare_for_request (Context c)
        {
            AMBRO_ASSERT(!m_user)
            AMBRO_ASSERT(m_recv_state == RecvState::INVALID)
            AMBRO_ASSERT(m_send_state == SendState::INVALID)
            
            // Set initial values to the various request-parsing states.
            m_null_in_line = false;
            m_bad_content_length = false;
            m_have_content_length = false;
            m_have_chunked = false;
            m_bad_transfer_encoding = false;
            m_expect_100_continue = false;
            m_expectation_failed = false;
            m_rem_allowed_length = Params::MaxRequestHeadLength;
            
            // And set some values related to higher-level processing of the request.
            m_have_request_body = false;
            m_resp_status = nullptr;
            m_resp_content_type = nullptr;
            m_user_accepting_request_body = false;
            
            // Prepare for parsing the request as a sequence of lines.
            prepare_line_parsing(c);
            
            // Waiting for space in the send buffer.
            m_state = State::RECV_REQUEST_LINE;
            m_recv_event.prependNow(c);
        }
        
        void check_for_next_request (Context c)
        {
            // When a request is completed, move on to the next one.
            if (m_state == State::USER_GONE &&
                m_recv_state == RecvState::COMPLETED &&
                m_send_state == SendState::COMPLETED)
            {
                AMBRO_ASSERT(!m_user)
                m_state = State::WAIT_SEND_BUF_FOR_REQUEST;
                m_recv_state = RecvState::INVALID;
                m_send_state = SendState::INVALID;
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
            ThePrinterMain::print_pgm_string(c, AMBRO_PSTR("//HttpClientError\n"));
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
                        prepare_for_request(c);
                    }
                } break;
                
                case State::HEAD_RECEIVED:
                case State::USER_GONE: {
                    switch (m_send_state) {
                        case SendState::SEND_BODY: {
                            // The user is providing the response body, so call
                            // the callback whenever we may have new space in the buffer.
                            AMBRO_ASSERT(m_user)
                            return m_user->responseBufferEvent(c);
                        } break;
                        
                        case SendState::SEND_LAST_CHUNK: {
                            // When we have enough space in the send buffer, send the zero-chunk.
                            if (m_connection.getSendBufferSpace(c) >= 5) {
                                send_string(c, "0\r\n\r\n");
                                m_connection.pokeSending(c);
                                m_send_state = SendState::COMPLETED;
                                check_for_next_request(c);
                            }
                        } break;
                    }
                } break;
                
                case State::DISCONNECT_AFTER_SENDING: {
                    // Disconnect the client once all data has been sent.
                    if (m_connection.getSendBufferSpace(c) >= TxBufferSize) {
                        disconnect(c);
                    }
                } break;
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
                        case RecvState::RECV_CHUNK_HEADER: {
                            // Receiving the chunk-header line.
                            recv_line(c, m_header_line, Params::MaxHeaderLineLength);
                        } break;
                        
                        case RecvState::RECV_KNOWN_LENGTH:
                        case RecvState::RECV_CHUNK_DATA: {
                            // Detect premature EOF from the client.
                            // We don't bother passing any remaining data to the user, this is easier.
                            if (m_rx_buf_eof && m_rx_buf_length < m_rem_req_body_length) {
#if APRINTER_DEBUG_HTTP_SERVER
                                ThePrinterMain::print_pgm_string(c, AMBRO_PSTR("//HttpClientEofInData\n"));
#endif
                                return close_gracefully(c, HttpStatusCodes::BadRequest());
                            }
                            
                            // When the user is accepting the request body, call the callback whenever
                            // we may have new data. Note that even after the user has received the
                            // entire body, we wait for abandonResponseBody() or completeHandling()
                            // before moving on to RecvState::COMPLETED.
                            if (m_user_accepting_request_body) {
                                AMBRO_ASSERT(m_user)
                                return m_user->requestBufferEvent(c);
                            }
                            
                            // If the entire body is received, move on to RecvState::COMPLETED.
                            if (m_req_body_recevied) {
                                m_recv_state = RecvState::COMPLETED;
                                check_for_next_request(c);
                                return;
                            }
                            
                            // Accept and discard any available request-body data.
                            size_t amount = get_request_body_avail(c);
                            if (amount > 0) {
                                consume_request_body(c, amount);
                            }
                        } break;
                    }
                } break;
            }
        }
        
        void recv_line (Context c, char *line_buf, size_t line_buf_size)
        {
            // Examine the received data, looking for a newline and copying
            // characters into line_buf.
            size_t pos = 0;
            bool end_of_line = false;
            
            while (pos < m_rx_buf_length) {
                char ch = m_rx_buf[buf_add(m_rx_buf_start, pos)];
                pos++;
                
                if (ch == '\0') {
                    m_null_in_line = true;
                }
                
                if (m_line_length >= line_buf_size) {
                    m_line_overflow = true;
                } else {
                    line_buf[m_line_length] = ch;
                    m_line_length++;
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
                ThePrinterMain::print_pgm_string(c, AMBRO_PSTR("//HttpClientRequestTooLong\n"));
                return close_gracefully(c, HttpStatusCodes::RequestHeaderFieldsTooLarge());
            }
            m_rem_allowed_length -= pos;
            
            // No newline yet?
            if (!end_of_line) {
                // If no EOF either, wait for more data.
                if (!m_rx_buf_eof) {
                    return;
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
        
        void line_received (Context c, size_t length, bool overflow, bool eof)
        {
            if (eof) {
#if APRINTER_DEBUG_HTTP_SERVER
                ThePrinterMain::print_pgm_string(c, AMBRO_PSTR("//HttpClientEofInLine\n"));
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
                        return request_head_received(c);
                    }
                    
                    // Extract and remember any useful information the header and continue parsing.
                    if (!overflow && !m_null_in_line) {
                        handle_header(c, m_header_line);
                    }
                    m_recv_event.prependNow(c);
                } break;
                
                case State::HEAD_RECEIVED:
                case State::USER_GONE: {
                    AMBRO_ASSERT(m_recv_state == RecvState::RECV_CHUNK_HEADER)
                    AMBRO_ASSERT(!m_req_body_recevied)
                    
                    // Check for errors in line parsing.
                    if (overflow || m_null_in_line) {
                        ThePrinterMain::print_pgm_string(c, AMBRO_PSTR("//HttpClientBadChunkLine\n"));
                        return close_gracefully(c, HttpStatusCodes::BadRequest());
                    }
                    
                    // Parse the chunk length.
                    char *endptr;
                    unsigned long long int value = strtoull(m_header_line, &endptr, 16);
                    if (endptr == m_header_line || (*endptr != '\0' && *endptr != ';')) {
                        ThePrinterMain::print_pgm_string(c, AMBRO_PSTR("//HttpClientBadChunkLine\n"));
                        return close_gracefully(c, HttpStatusCodes::BadRequest());
                    }
                    
                    // Remember the chunk length and continue in event_handler.
                    m_rem_req_body_length = value;
                    m_req_body_recevied = (value == 0);
                    m_recv_state = RecvState::RECV_CHUNK_DATA;
                    m_recv_event.prependNow(c);
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
                if (!StringEqualsCaseIns(header, "identity")) {
                    if (!StringEqualsCaseIns(header, "chunked") || m_have_chunked) {
                        m_bad_transfer_encoding = true;
                    } else {
                        m_have_chunked = true;
                    }
                }
            }
            else if (StringRemoveHttpHeader(&header, "expect")) {
                if (!StringEqualsCaseIns(header, "100-continue")) {
                    m_expectation_failed = true;
                } else {
                    m_expect_100_continue = true;
                }
            }
        }
        
        void request_head_received (Context c)
        {
            AMBRO_ASSERT(m_recv_state == RecvState::INVALID)
            AMBRO_ASSERT(m_send_state == SendState::INVALID)
            AMBRO_ASSERT(!m_user)
            
            char const *status = HttpStatusCodes::InternalServerError();
            do {
                // Check for nulls in the request.
                if (m_null_in_line) {
                    status = HttpStatusCodes::BadRequest();
                    goto error;
                }
                
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
                m_recv_state = m_have_request_body ? RecvState::NOT_STARTED : RecvState::COMPLETED;
                m_send_state = SendState::HEAD_NOT_SENT;
                
                // Call the user's request handler.
                return RequestHandler::call(c, this);
            } while (false);
            
        error:
            // Respond with an error and close the connection.
            close_gracefully(c, status);
        }
        
        bool receiving_request_body (Context c)
        {
            return (m_recv_state == RecvState::RECV_KNOWN_LENGTH ||
                    m_recv_state == RecvState::RECV_CHUNK_HEADER ||
                    m_recv_state == RecvState::RECV_CHUNK_DATA);
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
            if (user_accepting) {
                m_user_accepting_request_body = true;
            }
            
            // Start receiving the request body, chunked or known-length.
            if (m_have_chunked) {
                m_req_body_recevied = false;
                prepare_for_receiving_chunk_header(c);
            } else {
                m_req_body_recevied = (m_rem_req_body_length == 0);
                m_recv_state = RecvState::RECV_KNOWN_LENGTH;
            }
            m_recv_event.prependNow(c);
        }
        
        size_t get_request_body_avail (Context c)
        {
            AMBRO_ASSERT(receiving_request_body(c))
            
            if (m_recv_state == RecvState::RECV_CHUNK_HEADER) {
                return 0;
            }
            return (size_t)MinValue((uint64_t)m_rx_buf_length, m_rem_req_body_length);
        }
        
        void consume_request_body (Context c, size_t amount)
        {
            AMBRO_ASSERT(m_recv_state == RecvState::RECV_KNOWN_LENGTH || m_recv_state == RecvState::RECV_CHUNK_DATA)
            AMBRO_ASSERT(amount > 0)
            AMBRO_ASSERT(amount <= m_rx_buf_length)
            AMBRO_ASSERT(amount <= m_rem_req_body_length)
            AMBRO_ASSERT(!m_req_body_recevied)
            
            // Adjust RX buffer and remaining-data length.
            accept_rx_data(c, amount);
            m_rem_req_body_length -= amount;
            
            // End of known-length body or chunk?
            if (m_rem_req_body_length == 0) {
                if (m_recv_state == RecvState::RECV_KNOWN_LENGTH) {
                    m_req_body_recevied = true;
                } else {
                    prepare_for_receiving_chunk_header(c);
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
        
        void prepare_for_receiving_chunk_header (Context c)
        {
            // We need to set m_rem_allowed_length each time we start to receive a chunk header.
            m_recv_state = RecvState::RECV_CHUNK_HEADER;
            m_rem_allowed_length = Params::MaxHeaderLineLength;
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
        
        void abandon_response_body (Context c)
        {
            AMBRO_ASSERT(m_send_state != SendState::INVALID)
            
            if (m_send_state == SendState::HEAD_NOT_SENT) {
                // The response head has not been sent.
                // Send the response now, with the status as the body.
                send_response(c, m_resp_status, true);
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
        // HttpRequestInterface functions
        
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
            send_response(c, m_resp_status, false, m_resp_content_type);
            
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
            
            if (length == 0) {
                return;
            }
            
            // Get the send buffer reference and sanity check the length / space.
            WrapBuffer con_space_buffer;
            size_t con_space_avail = m_connection.getSendBufferSpace(c, &con_space_buffer);
            AMBRO_ASSERT(con_space_avail >= TxChunkOverhead)
            AMBRO_ASSERT(length <= con_space_avail - TxChunkOverhead)
            
            // Write the chunk header and footer.
            char chunk_header[TxChunkHeaderSize + 1];
            sprintf(chunk_header, "%.*" PRIx32 "\r\n", (int)Params::TxChunkHeaderDigits, (uint32_t)length);
            con_space_buffer.copyIn(0, TxChunkHeaderSize, chunk_header);
            con_space_buffer.copyIn(TxChunkHeaderSize + length, 2, "\r\n");
            
            // Submit data to the connection and poke sending.
            m_connection.copySendData(c, nullptr, TxChunkOverhead + length);
            m_connection.pokeSending(c);
        }
        
    public:
        typename Context::EventLoop::QueuedEvent m_send_event;
        typename Context::EventLoop::QueuedEvent m_recv_event;
        TheTcpConnection m_connection;
        RequestUserCallback *m_user;
        UserClientState m_user_client_state;
        size_t m_rx_buf_start;
        size_t m_rx_buf_length;
        size_t m_line_length;
        size_t m_rem_allowed_length;
        uint64_t m_rem_req_body_length;
        char const *m_request_method;
        char const *m_request_path;
        char const *m_resp_status;
        char const *m_resp_content_type;
        State m_state;
        RecvState m_recv_state;
        SendState m_send_state;
        bool m_line_overflow;
        bool m_null_in_line;
        bool m_request_line_overflow;
        bool m_bad_content_length;
        bool m_have_content_length;
        bool m_have_chunked;
        bool m_bad_transfer_encoding;
        bool m_expect_100_continue;
        bool m_expectation_failed;
        bool m_have_request_body;
        bool m_req_body_recevied;
        bool m_user_accepting_request_body;
        bool m_rx_buf_eof;
        char m_rx_buf[RxBufferSize];
        char m_request_line[Params::MaxRequestLineLength];
        char m_header_line[Params::MaxHeaderLineLength];
    };
    
public:
    struct Object : public ObjBase<HttpServer, ParentObject, EmptyTypeList> {
        TheTcpListener listener;
        TheTcpListenerQueueEntry queue[Params::QueueSize];
        Client clients[Params::MaxClients];
    };
};

APRINTER_ALIAS_STRUCT_EXT(HttpServerService, (
    APRINTER_AS_VALUE(uint16_t, Port),
    APRINTER_AS_VALUE(int, MaxClients),
    APRINTER_AS_VALUE(int, QueueSize),
    APRINTER_AS_TYPE(QueueTimeout),
    APRINTER_AS_VALUE(size_t, MaxRequestLineLength),
    APRINTER_AS_VALUE(size_t, MaxHeaderLineLength),
    APRINTER_AS_VALUE(size_t, ExpectedResponseLength),
    APRINTER_AS_VALUE(size_t, MaxRequestHeadLength),
    APRINTER_AS_VALUE(size_t, TxChunkHeaderDigits)
), (
    template <typename Context, typename ParentObject, typename ThePrinterMain, typename RequestHandler, typename UserClientState>
    using Server = HttpServer<Context, ParentObject, ThePrinterMain, RequestHandler, UserClientState, HttpServerService>;
))

#include <aprinter/EndNamespace.h>

#endif
