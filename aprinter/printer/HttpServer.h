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
#include <aprinter/base/Object.h>
#include <aprinter/base/Callback.h>
#include <aprinter/base/ProgramMemory.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/WrapBuffer.h>
#include <aprinter/misc/StringTools.h>
#include <aprinter/printer/HttpServerCommon.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, typename ThePrinterMain, typename RequestHandler, typename Params>
class HttpServer {
public:
    struct Object;
    
public:
    using TheRequestInterface = HttpRequestInterface<Context>;
    
private:
    using TheTcpListener   = typename Context::Network::TcpListener;
    using TheTcpConnection = typename Context::Network::TcpConnection;
    using RequestUserCallback     = typename TheRequestInterface::RequestUserCallback; 
    using RequestBodyBufferState  = typename TheRequestInterface::RequestBodyBufferState;
    using ResponseBodyBufferState = typename TheRequestInterface::ResponseBodyBufferState;
    
    static size_t const RxBufferSize = TheTcpConnection::RequiredRxBufSize;
    static size_t const TxBufferSize = TheTcpConnection::ProvidedTxBufSize;
    
    // Basic checks of parameters.
    static_assert(Params::MaxClients > 0, "");
    static_assert(Params::MaxRequestLineLength >= 32, "");
    static_assert(Params::MaxHeaderLineLength >= 40, "");
    static_assert(Params::ExpectedResponseLength >= 250, "");
    static_assert(Params::ExpectedResponseLength <= TxBufferSize, "");
    static_assert(Params::MaxRequestHeadLength >= 500, "");
    static_assert(Params::TxChunkHeaderDigits >= 3, "");
    static_assert(Params::TxChunkHeaderDigits <= 8, "");
    
    // Check TxChunkHeaderDigits (needs to be large enough to encode the largest possible chunk size).
    static size_t const TxChunkHeaderSize = Params::TxChunkHeaderDigits + 2;
    static_assert(TxChunkHeaderSize < TxBufferSize, "");
    static size_t const TxBufferSizeForChunkData = TxBufferSize - TxChunkHeaderSize;
    static_assert(Params::TxChunkHeaderDigits >= HexDigitsInInt<TxBufferSizeForChunkData>::Value, "");
    
public:
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        o->listener.init(c, APRINTER_CB_STATFUNC_T(&HttpServer::listener_accept_handler));
        
        if (!o->listener.startListening(c, Params::Port)) {
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
    
    static bool remove_header_name (char const **data, char const *low_header_name)
    {
        // The header name.
        size_t pos = 0;
        while (low_header_name[pos] != '\0') {
            if (AsciiToLower((*data)[pos]) != low_header_name[pos]) {
                return false;
            }
            pos++;
        }
        
        // A colon.
        if ((*data)[pos] != ':') {
            return false;
        }
        pos++;
        
        // Any spaces after the colon.
        while ((*data)[pos] == ' ') {
            pos++;
        }
        
        *data += pos;
        return true;
    }
    
    struct Client : public TheRequestInterface {
        enum class State : uint8_t {
            NOT_CONNECTED, WAIT_SEND_BUF_FOR_REQUEST,
            RECV_REQUEST_LINE, RECV_HEADER_LINE, HEAD_RECEIVED,
            RECV_KNOWN_LENGTH_BODY, RECV_CHUNK_HEADER, RECV_CHUNK_DATA,
            SEND_RESPONSE_BODY, SEND_LAST_CHUNK,
            SEND_ERROR_AND_DISCONNECT, DISCONNECT_AFTER_SENDING,
            CALLING_REQUEST_TERMINATED
        };
        
        bool response_is_pending (Context c)
        {
            switch (m_state) {
                case State::HEAD_RECEIVED:
                case State::RECV_KNOWN_LENGTH_BODY:
                case State::RECV_CHUNK_HEADER:
                case State::RECV_CHUNK_DATA:
                    return true;
            }
            return false;
        }
        
        bool receiving_request_body (Context c)
        {
            switch (m_state) {
                case State::RECV_KNOWN_LENGTH_BODY:
                case State::RECV_CHUNK_HEADER:
                case State::RECV_CHUNK_DATA:
                    return true;
            }
            return false;
        }
        
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
            m_event.init(c, APRINTER_CB_OBJFUNC_T(&Client::event_handler, this));
            m_connection.init(c, APRINTER_CB_OBJFUNC_T(&Client::connection_error_handler, this),
                                 APRINTER_CB_OBJFUNC_T(&Client::connection_recv_handler, this),
                                 APRINTER_CB_OBJFUNC_T(&Client::connection_send_handler, this));
            m_state = State::NOT_CONNECTED;
            m_user = nullptr;
        }
        
        void deinit (Context c)
        {
            m_connection.deinit(c);
            m_event.deinit(c);
        }
        
        void accept_connection (Context c, TheTcpListener *listener)
        {
            AMBRO_ASSERT(m_state == State::NOT_CONNECTED)
            AMBRO_ASSERT(!m_user)
            
            ThePrinterMain::print_pgm_string(c, AMBRO_PSTR("//HttpClientConnected\n"));
            
            // Accept the connection.
            m_connection.acceptConnection(c, listener);
            
            // Initialzie the RX buffer.
            m_rx_buf_start = 0;
            m_rx_buf_length = 0;
            
            // Prepare for receiving the first request.
            prepare_for_request(c);
        }
        
        void disconnect (Context c)
        {
            AMBRO_ASSERT(m_state != State::NOT_CONNECTED)
            
            // Inform the user that the request is over, if necessary.
            terminate_user(c);
            
            // Clean up in preparation to accept another client.
            m_connection.reset(c);
            m_event.unset(c);
            m_state = State::NOT_CONNECTED;
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
            m_resp_status = HttpStatusCodes::InternalServerError();
            m_resp_content_type = nullptr;
            m_user_accepting_request_body = false;
            m_user_providing_response_body = false;
            
            // Prepare for parsing the request as a sequence of lines.
            prepare_line_parsing(c);
            
            // Waiting for space in the send buffer.
            m_state = State::WAIT_SEND_BUF_FOR_REQUEST;
            m_event.prependNow(c);
        }
        
        void connection_error_handler (Context c, bool remote_closed)
        {
            AMBRO_ASSERT(m_state != State::NOT_CONNECTED)
            
            ThePrinterMain::print_pgm_string(c, AMBRO_PSTR("//HttpClientDisconnected\n"));
            disconnect(c);
        }
        
        void connection_recv_handler (Context c, size_t bytes_read)
        {
            AMBRO_ASSERT(m_state != State::NOT_CONNECTED)
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
            m_event.prependNow(c);
        }
        
        void connection_send_handler (Context c)
        {
            AMBRO_ASSERT(m_state != State::NOT_CONNECTED)
            
            // Check for things to be done now that we have more space.
            m_event.prependNow(c);
        }
        
        void event_handler (Context c)
        {
            // Check for stuff to be done and do it.
            // A call to this is scheduled by m_event.prependNow(c).
            
            switch (m_state) {
                case State::WAIT_SEND_BUF_FOR_REQUEST: {
                    // When we have sufficient space in the send buffer, start receiving a request.
                    if (m_connection.getSendBufferSpace(c) >= Params::ExpectedResponseLength) {
                        m_state = State::RECV_REQUEST_LINE;
                        m_event.prependNow(c);
                    }
                } break;
                
                case State::RECV_REQUEST_LINE: {
                    // Receiving the request line.
                    recv_line(c, m_request_line, Params::MaxRequestLineLength);
                } break;
                
                case State::RECV_HEADER_LINE:
                case State::RECV_CHUNK_HEADER: {
                    // Receiving a line (request header or chunk header).
                    recv_line(c, m_header_line, Params::MaxHeaderLineLength);
                } break;
                
                case State::RECV_KNOWN_LENGTH_BODY:
                case State::RECV_CHUNK_DATA: {
                    // When the user is accepting the request body,
                    // call the callback whenever we may have new data.
                    if (m_user_accepting_request_body) {
                        return m_user->requestBufferEvent(c);
                    }
                    
                    // Accept request body data ourselves and discard it.
                    if (m_req_body_recevied) {
                        return send_response(c, false);
                    }
                    size_t amount = get_request_body_avail(c);
                    if (amount > 0) {
                        consume_request_body(c, amount);
                    }
                } break;
                
                case State::SEND_RESPONSE_BODY: {
                    // When the user is providing the response body,
                    // call the callback whenever we may have new space in the buffer.
                    if (m_user_providing_response_body) {
                        return m_user->responseBufferEvent(c);
                    }
                    
                    // There won't be any more data, now we have to send a zero-chunk.
                    AMBRO_ASSERT(!m_user)
                    m_state = State::SEND_LAST_CHUNK;
                    m_event.prependNow(c);
                } break;
                
                case State::SEND_LAST_CHUNK: {
                    AMBRO_ASSERT(!m_user)
                    
                    // Waiting for enough space to send the zero-chunk.
                    if (m_connection.getSendBufferSpace(c) >= 3) {
                        // Send the zero-chunk.
                        send_string(c, "0\r\n");
                        m_connection.pokeSending(c);
                        
                        // Prepare for receiving the next request.
                        prepare_for_request(c);
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
        
        size_t get_request_body_avail (Context c)
        {
            AMBRO_ASSERT(receiving_request_body(c))
            
            if (m_state == State::RECV_CHUNK_HEADER) {
                return 0;
            }
            return (size_t)MinValue((uint64_t)m_rx_buf_length, m_rem_req_body_length);
        }
        
        void consume_request_body (Context c, size_t amount)
        {
            AMBRO_ASSERT(m_state == State::RECV_KNOWN_LENGTH_BODY || m_state == State::RECV_CHUNK_DATA)
            AMBRO_ASSERT(!m_req_body_recevied)
            AMBRO_ASSERT(amount > 0)
            AMBRO_ASSERT(amount <= m_rx_buf_length)
            AMBRO_ASSERT(amount <= m_rem_req_body_length)
            
            // Adjust RX buffer and remaining-data length.
            accept_rx_data(c, amount);
            m_rem_req_body_length -= amount;
            
            // End of known-length body or chunk?
            if (m_rem_req_body_length == 0) {
                if (m_state == State::RECV_KNOWN_LENGTH_BODY) {
                    m_req_body_recevied = true;
                } else {
                    m_state = State::RECV_CHUNK_HEADER;
                    m_rem_allowed_length = Params::MaxHeaderLineLength;
                }
                // Continue in event_handler.
                m_event.prependNow(c);
            }
        }
        
        void recv_line (Context c, char *line_buf, size_t line_max_length)
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
                
                if (m_line_length >= line_max_length) {
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
            
            // Detect too long request bodies.
            if (pos > m_rem_allowed_length) {
                ThePrinterMain::print_pgm_string(c, AMBRO_PSTR("//HttpClientRequestTooLong\n"));
                return send_error_and_disconnect(c, HttpStatusCodes::RequestHeaderFieldsTooLarge());
            }
            m_rem_allowed_length -= pos;
            
            // If there was no newline yet, wait for more data.
            if (!end_of_line) {
                return;
            }
            
            // Null-terminate the line, striping the \n and possibly a \r.
            size_t length = m_line_length - 1;
            if (length > 0 && line_buf[length - 1] == '\r') {
                length--;
            }
            line_buf[length] = '\0';
            
            // Adjust line-parsing state for parsing another line.
            bool overflow = m_line_overflow;
            prepare_line_parsing(c);
            
            // Delegate the interpretation of the line to another function.
            line_received(c, length, overflow);
        }
        
        void line_received (Context c, size_t length, bool overflow)
        {
            switch (m_state) {
                case State::RECV_REQUEST_LINE: {
                    // Remember the request line and continue parsing the header lines.
                    m_request_line_length = length;
                    m_request_line_overflow = overflow;
                    m_state = State::RECV_HEADER_LINE;
                    m_event.prependNow(c);
                } break;
                
                case State::RECV_HEADER_LINE: {
                    if (length == 0) {
                        // This is the empty line which terminates the request head.
                        return request_head_received(c);
                    }
                    
                    // Extract and remember any useful information the header and continue parsing.
                    // We do not remember the headers themselves, this would use too much RAM.
                    if (!overflow && !m_null_in_line) {
                        handle_header(c, m_header_line);
                    }
                    m_event.prependNow(c);
                } break;
                
                case State::RECV_CHUNK_HEADER: {
                    AMBRO_ASSERT(!m_req_body_recevied)
                    
                    // Check for errors in line parsing.
                    if (overflow || m_null_in_line) {
                        ThePrinterMain::print_pgm_string(c, AMBRO_PSTR("//HttpClientBadChunkLine\n"));
                        return send_error_and_disconnect(c, HttpStatusCodes::BadRequest());
                    }
                    
                    // Parse the chunk length.
                    char *endptr;
                    unsigned long long int value = strtoull(m_header_line, &endptr, 16);
                    if (endptr == m_header_line || (*endptr != '\0' && *endptr != ';')) {
                        ThePrinterMain::print_pgm_string(c, AMBRO_PSTR("//HttpClientBadChunkLine\n"));
                        return send_error_and_disconnect(c, HttpStatusCodes::BadRequest());
                    }
                    
                    // Remember the chunk length and continue in event_handler.
                    m_rem_req_body_length = value;
                    m_req_body_recevied = (value == 0);
                    m_state = State::RECV_CHUNK_DATA;
                    m_event.prependNow(c);
                } break;
                
                default: AMBRO_ASSERT(false);
            }
        }
        
        void handle_header (Context c, char const *header)
        {
            if (remove_header_name(&header, "content-length")) {
                char *endptr;
                unsigned long long int value = strtoull(header, &endptr, 10);
                if (endptr == header || *endptr != '\0' || m_have_content_length) {
                    m_bad_content_length = true;
                } else {
                    m_have_content_length = true;
                    m_rem_req_body_length = value;
                }
            }
            else if (remove_header_name(&header, "transfer-encoding")) {
                if (!StringEqualsCaseIns(header, "identity")) {
                    if (!StringEqualsCaseIns(header, "chunked") || m_have_chunked) {
                        m_bad_transfer_encoding = true;
                    } else {
                        m_have_chunked = true;
                    }
                }
            }
            else if (remove_header_name(&header, "expect")) {
                if (!StringEqualsCaseIns(header, "100-continue")) {
                    m_expectation_failed = true;
                } else {
                    m_expect_100_continue = true;
                }
            }
        }
        
        void request_head_received (Context c)
        {
            AMBRO_ASSERT(!m_user)
            
            char const *status = HttpStatusCodes::InternalServerError();
            do {
                // Check for nulls in the request.
                if (m_null_in_line) {
                    status = HttpStatusCodes::BadRequest();
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
                
                // Check that there are no spaces after the HTTP version string.
                if (strchr(buf, ' ')) {
                    status = HttpStatusCodes::BadRequest();
                    goto error;
                }
                
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
                
                // Check for request line overflow.
                if (m_request_line_overflow) {
                    status = HttpStatusCodes::UriTooLong();
                    goto error;
                }
                
                // Check request-body-related stuff.
                if (m_bad_content_length || m_bad_transfer_encoding || (m_have_content_length && m_have_chunked)) {
                    status = HttpStatusCodes::BadRequest();
                    goto error;
                }
                
                // Check for failed expectations.
                if (m_expectation_failed) {
                    status = HttpStatusCodes::ExpectationFailed();
                    goto error;
                }
                
                // Determine if we are receiving a request body.
                if (m_have_content_length || m_have_chunked) {
                    m_have_request_body = true;
                }
                
                // Change state before passing the request to the user.
                m_state = State::HEAD_RECEIVED;
                
                // Call the user's request handler.
                return RequestHandler::call(c, this);
            } while (false);
            
        error:
            // Respond with an error and close the connection.
            send_error_and_disconnect(c, status);
        }
        
        void continue_after_head_accepted (Context c)
        {
            AMBRO_ASSERT(!m_user_accepting_request_body || m_user)
            
            // If there is no request body, treat it like there is a zero-length
            // one, for simpler code.
            if (!m_have_request_body) {
                m_rem_req_body_length = 0;
            }
            
            // Handle 100-continue stuff.
            if (m_have_request_body && m_expect_100_continue) {
                if (m_user_accepting_request_body) {
                    // The user is prepared to accept the request body.
                    // Send 100-continue and receive the reqeust body.
                    send_string(c, "HTTP/1.1 100 Continue\r\n\r\n");
                    m_connection.pokeSending(c);
                } else {
                    // The user will not be accepting the request body, so
                    // skip receiving the request body.
                    m_have_chunked = false;
                    m_rem_req_body_length = 0;
                }
            }
            
            // Check if the entire body is already received.
            m_req_body_recevied = (!m_have_chunked && m_rem_req_body_length == 0);
            
            // Go on receiving the request body.
            if (m_have_chunked) {
                m_state = State::RECV_CHUNK_HEADER;
                m_rem_allowed_length = Params::MaxHeaderLineLength;
            } else {
                m_state = State::RECV_KNOWN_LENGTH_BODY;
            }
            m_event.prependNow(c);
        }
        
        void send_error_and_disconnect (Context c, char const *status)
        {
            // Terminate the request with the user, if any.
            terminate_user(c);
            m_user_accepting_request_body = false;
            m_user_providing_response_body = false;
            
            // Send a response and go to State::DISCONNECT_AFTER_SENDING.
            m_resp_status = status;
            send_response(c, true);
        }
        
        void send_response (Context c, bool close_connection)
        {
            AMBRO_ASSERT(!m_user_providing_response_body || m_user)
            AMBRO_ASSERT(!close_connection || !m_user_providing_response_body)
            
            // Use text/plain content-type, unless the user is providing the
            // response payload and has specified the content type.
            if (!m_user_providing_response_body || !m_resp_content_type) {
                m_resp_content_type = "text/plain; charset=utf-8";
            }
            
            // Send the response head.
            send_string(c, "HTTP/1.1 ");
            send_string(c, m_resp_status);
            send_string(c, "\r\nServer: Aprinter\r\nContent-Type: ");
            send_string(c, m_resp_content_type);
            send_string(c, "\r\n");
            if (!m_user_providing_response_body) {
                send_string(c, "Content-Length: ");
                char length_buf[12];
                sprintf(length_buf, "%d", (int)(strlen(m_resp_status) + 1));
                send_string(c, length_buf);
            } else {
                send_string(c, "Transfer-Encoding: chunked");
            }
            send_string(c, "\r\n\r\n");
            
            if (!m_user_providing_response_body) {
                // We send the status as the response.
                send_string(c, m_resp_status);
                send_string(c, "\n");
                m_connection.pokeSending(c);
                
                // Either we prepare for another request or close the connection
                // after sending the response, as desired.
                if (close_connection) {
                    m_state = State::DISCONNECT_AFTER_SENDING;
                    m_event.prependNow(c);
                } else {
                    // TBD: m_user
                    prepare_for_request(c);
                }
            } else {
                // The user will now be producing the response body.
                m_connection.pokeSending(c);
                m_state = State::SEND_RESPONSE_BODY;
                m_event.prependNow(c);
            }
        }
        
    public: // HttpRequestInterface functions
        char const * getMethod (Context c)
        {
            AMBRO_ASSERT(m_state == State::HEAD_RECEIVED || m_user)
            
            return m_request_method;
        }
        
        char const * getPath (Context c)
        {
            AMBRO_ASSERT(m_state == State::HEAD_RECEIVED || m_user)
            
            return m_request_path;
        }
        
        bool hasRequestBody (Context c)
        {
            AMBRO_ASSERT(m_state == State::HEAD_RECEIVED || m_user)
            
            return m_have_request_body;
        }
        
        void adoptRequest (Context c, RequestUserCallback *callback)
        {
            AMBRO_ASSERT(m_state == State::HEAD_RECEIVED)
            AMBRO_ASSERT(!m_user)
            AMBRO_ASSERT(callback)
            
            m_user = callback;
        }
        
        void abandonRequest (Context c)
        {
            AMBRO_ASSERT(m_user)
            
            // Remember that the user is gone. We will not call any
            // more HttpRequestInterface callbacks from this point or
            // expect/allow calls from the user.
            m_user = nullptr;
            
            // The user is not able to accept a request body or
            // provide a response body from now on.
            m_user_accepting_request_body = false;
            m_user_providing_response_body = false;
            
            if (m_state == State::HEAD_RECEIVED) {
                // Continue handling the request, since we will not get an
                // acceptRequestHead call from the user.
                continue_after_head_accepted(c);
            } else {
                // We may need to continue dealing with a pending request or
                // response body ourselves.
                m_event.prependNow(c);
            }
        }
        
        void willAcceptRequestBody (Context c)
        {
            AMBRO_ASSERT(m_state == State::HEAD_RECEIVED)
            AMBRO_ASSERT(m_user)
            
            m_user_accepting_request_body = true;
        }
        
        void willProvideResponseBody (Context c)
        {
            AMBRO_ASSERT(response_is_pending(c))
            AMBRO_ASSERT(m_user)
            
            m_user_providing_response_body = true;
        }
        
        void acceptRequestHead (Context c)
        {
            AMBRO_ASSERT(m_state == State::HEAD_RECEIVED)
            
            continue_after_head_accepted(c);
        }
        
        void setResponseStatus (Context c, char const *status)
        {
            AMBRO_ASSERT(response_is_pending(c))
            AMBRO_ASSERT(status)
            
            m_resp_status = status;
        }
        
        void setResponseContentType (Context c, char const *content_type)
        {
            AMBRO_ASSERT(response_is_pending(c))
            AMBRO_ASSERT(content_type)
            
            m_resp_content_type = content_type;
        }
        
        void acceptRequestBody (Context c)
        {
            AMBRO_ASSERT(receiving_request_body(c))
            AMBRO_ASSERT(m_user_accepting_request_body)
            AMBRO_ASSERT(m_req_body_recevied)
            
            send_response(c, false);
        }
        
        RequestBodyBufferState getRequestBodyBufferState (Context c)
        {
            AMBRO_ASSERT(receiving_request_body(c))
            AMBRO_ASSERT(m_user_accepting_request_body)
            
            size_t data_len = get_request_body_avail(c);
            size_t first_chunk_len = MinValue(data_len, (size_t)(RxBufferSize - m_rx_buf_start));
            
            return RequestBodyBufferState{
                WrapBuffer::Make(first_chunk_len, m_rx_buf + m_rx_buf_start, m_rx_buf),
                data_len,
                m_req_body_recevied
            };
        }
        
        void acceptRequestBodyData (Context c, size_t length)
        {
            AMBRO_ASSERT(receiving_request_body(c))
            AMBRO_ASSERT(m_user_accepting_request_body)
            AMBRO_ASSERT(length <= get_request_body_avail(c))
            
            if (length > 0) {
                consume_request_body(c, length);
            }
        }
        
        ResponseBodyBufferState getResponseBodyBufferState (Context c)
        {
            AMBRO_ASSERT(m_state == State::SEND_RESPONSE_BODY)
            AMBRO_ASSERT(m_user_providing_response_body)
            
            WrapBuffer con_space_buffer;
            size_t con_space_avail = m_connection.getSendBufferSpace(c, &con_space_buffer);
            
            if (con_space_avail < TxChunkHeaderSize) {
                return ResponseBodyBufferState{con_space_buffer, 0};
            } else {
                return ResponseBodyBufferState{con_space_buffer.subFrom(TxChunkHeaderSize), con_space_avail - TxChunkHeaderSize};
            }
        }
        
        void provideResponseBodyData (Context c, size_t length)
        {
            AMBRO_ASSERT(m_state == State::SEND_RESPONSE_BODY)
            AMBRO_ASSERT(m_user_providing_response_body)
            
            if (length > 0) {
                WrapBuffer con_space_buffer;
                size_t con_space_avail = m_connection.getSendBufferSpace(c, &con_space_buffer);
                AMBRO_ASSERT(con_space_avail >= TxChunkHeaderSize)
                AMBRO_ASSERT(length <= con_space_avail - TxChunkHeaderSize)
                
                char chunk_header[TxChunkHeaderSize + 1];
                sprintf(chunk_header, "%.*" PRIu32 "\r\n", (int)Params::TxChunkHeaderDigits, (uint32_t)length);
                con_space_buffer.copyIn(0, TxChunkHeaderSize, chunk_header);
                
                m_connection.copySendData(c, nullptr, TxChunkHeaderSize + length);
                m_connection.pokeSending(c);
            }
        }
        
    public:
        typename Context::EventLoop::QueuedEvent m_event;
        TheTcpConnection m_connection;
        RequestUserCallback *m_user;
        size_t m_rx_buf_start;
        size_t m_rx_buf_length;
        size_t m_line_length;
        size_t m_request_line_length;
        size_t m_rem_allowed_length;
        uint64_t m_rem_req_body_length;
        char const *m_request_method;
        char const *m_request_path;
        char const *m_resp_status;
        char const *m_resp_content_type;
        State m_state;
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
        bool m_user_accepting_request_body;
        bool m_user_providing_response_body;
        bool m_req_body_recevied;
        char m_rx_buf[RxBufferSize];
        char m_request_line[Params::MaxRequestLineLength];
        char m_header_line[Params::MaxHeaderLineLength];
    };
    
public:
    struct Object : public ObjBase<HttpServer, ParentObject, EmptyTypeList> {
        TheTcpListener listener;
        Client clients[Params::MaxClients];
    };
};

template <
    uint16_t TPort,
    int TMaxClients,
    size_t TMaxRequestLineLength,
    size_t TMaxHeaderLineLength,
    size_t TExpectedResponseLength,
    size_t TMaxRequestHeadLength,
    size_t TTxChunkHeaderDigits
>
struct HttpServerService {
    static uint16_t const Port = TPort;
    static int const MaxClients = TMaxClients;
    static size_t const MaxRequestLineLength = TMaxRequestLineLength;
    static size_t const MaxHeaderLineLength = TMaxHeaderLineLength;
    static size_t const ExpectedResponseLength = TExpectedResponseLength;
    static size_t const MaxRequestHeadLength = TMaxRequestHeadLength;
    static size_t const TxChunkHeaderDigits = TTxChunkHeaderDigits;
    
    template <typename Context, typename ParentObject, typename ThePrinterMain, typename RequestHandler>
    using Server = HttpServer<Context, ParentObject, ThePrinterMain, RequestHandler, HttpServerService>;
};

#include <aprinter/EndNamespace.h>

#endif
