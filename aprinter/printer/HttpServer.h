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
#include <aprinter/base/Object.h>
#include <aprinter/base/Callback.h>
#include <aprinter/base/ProgramMemory.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Likely.h>
#include <aprinter/printer/HttpServerCommon.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, typename ThePrinterMain, typename RequestHandler, typename Params>
class HttpServer {
public:
    struct Object;
    
private:
    using TheTcpListener = typename Context::Network::TcpListener;
    using TheTcpConnection = typename Context::Network::TcpConnection;
    
    static_assert(Params::MaxClients > 0, "");
    static_assert(Params::MaxRequestLineLength >= 10, "");
    static_assert(Params::MaxHeaderLineLength >= 60, "");
    static_assert(Params::ExpectedResponseLength >= 250, "");
    static_assert(Params::ExpectedResponseLength <= TheTcpConnection::ProvidedTxBufSize, "");
    
    static size_t const RxBufferSize = TheTcpConnection::RequiredRxBufSize;
    
public:
    struct HttpRequest {
        char const *method;
        char const *path;
    };
    
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
                cl->accept_connection(c);
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
    
    static char make_lower_char (char ch)
    {
        if (ch >= 'A' && ch <= 'Z') {
            ch += 32;
        }
        return ch;
    }
    
    static bool remove_prefix (char const **data, size_t *length, char const *prefix)
    {
        size_t pos = 0;
        
        while (prefix[pos] != '\0') {
            if (pos == *length || (*data)[pos] != prefix[pos]) {
                return false;
            }
            pos++;
        }
        
        *data += pos;
        *length -= pos;
        
        return true;
    }
    
    static bool remove_header_name (char const **data, size_t *length, char const *low_header_name)
    {
        size_t pos = 0;
        
        while (low_header_name[pos] != '\0') {
            if (pos == *length || make_lower_char((*data)[pos]) != low_header_name[pos]) {
                return false;
            }
            pos++;
        }
        
        if (pos == *length || (*data)[pos] != ':') {
            return false;
        }
        pos++;
        
        while (pos != *length && (*data)[pos] == ' ') {
            pos++;
        }
        
        *data += pos;
        *length -= pos;
        
        return true;
    }
    
    static bool data_equals_caseins (char const *data, size_t length, char const *low_str)
    {
        while (length > 0 && *low_str != '\0' && make_lower_char(*data) == *low_str) {
            data++;
            length--;
            low_str++;
        }
        return (length == 0 && *low_str == '\0');
    }
    
    using TheRequestInterface = HttpRequestInterface<Context>;
    
    struct Client : public TheRequestInterface {
        enum class State : uint8_t {
            NOT_CONNECTED, WAIT_SEND_BUF, RECV_REQUEST_LINE, RECV_HEADER_LINE, HEAD_RECEIVED,
            RECV_KNOWN_LENGTH_BODY, RECV_CHUNK_HEADER, RECV_CHUNK_DATA,
            SEND_BODY
        };
        
        bool have_request (Context c)
        {
            switch (m_state) {
                case State::HEAD_RECEIVED:
                case State::RECV_KNOWN_LENGTH_BODY:
                case State::RECV_CHUNK_HEADER:
                case State::RECV_CHUNK_DATA:
                case State::SEND_BODY:
                    return true;
                default:
                    return false;
            }
        }
        
        bool have_request_before_headers_sent (Context c)
        {
            switch (m_state) {
                case State::HEAD_RECEIVED:
                case State::RECV_KNOWN_LENGTH_BODY:
                case State::RECV_CHUNK_HEADER:
                case State::RECV_CHUNK_DATA:
                    return true;
                default:
                    return false;
            }
        }
        
        void send_string (Context c, char const *str)
        {
            m_connection.copySendData(c, str, strlen(str));
        }
        
        void init (Context c)
        {
            m_parse_event.init(c, APRINTER_CB_OBJFUNC_T(&Client::parse_event_handler, this));
            m_connection.init(c, APRINTER_CB_OBJFUNC_T(&Client::connection_error_handler, this),
                                 APRINTER_CB_OBJFUNC_T(&Client::connection_recv_handler, this),
                                 APRINTER_CB_OBJFUNC_T(&Client::connection_send_handler, this));
            m_state = State::NOT_CONNECTED;
        }
        
        void deinit (Context c)
        {
            m_connection.deinit(c);
            m_parse_event.deinit(c);
        }
        
        void accept_connection (Context c)
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(m_state == State::NOT_CONNECTED)
            
            ThePrinterMain::print_pgm_string(c, AMBRO_PSTR("//HttpClientConnected\n"));
            
            m_connection.acceptConnection(c, &o->listener);
            m_rx_buf_start = 0;
            m_rx_buf_length = 0;
            prepare_for_request(c);
        }
        
        void prepare_for_request (Context c)
        {
            m_line_length = 0;
            m_line_overflow = false;
            m_bad_content_length = false;
            m_have_content_length = false;
            m_have_chunked = false;
            m_bad_transfer_encoding = false;
            m_100_continue = false;
            m_expectation_failed = false;
            
            if (m_connection.getSendBufferSpace(c) >= Params::ExpectedResponseLength) {
                m_state = State::RECV_REQUEST_LINE;
                m_parse_event.prependNowNotAlready(c);
            } else {
                m_state = State::WAIT_SEND_BUF;
            }
        }
        
        void disconnect (Context c)
        {
            AMBRO_ASSERT(m_state != State::NOT_CONNECTED)
            
            m_connection.reset(c);
            m_parse_event.unset(c);
            m_state = State::NOT_CONNECTED;
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
            
            size_t write_offset = buf_add(m_rx_buf_start, m_rx_buf_length);
            size_t first_chunk_len = MinValue(bytes_read, (size_t)(RxBufferSize - write_offset));
            
            m_connection.copyReceivedData(c, m_rx_buf + write_offset, first_chunk_len);
            if (first_chunk_len < bytes_read) {
                m_connection.copyReceivedData(c, m_rx_buf, bytes_read - first_chunk_len);
            }
            
            m_rx_buf_length += bytes_read;
            
            if (m_state == State::RECV_REQUEST_LINE || m_state == State::RECV_HEADER_LINE) {
                if (!m_parse_event.isSet(c)) {
                    m_parse_event.prependNowNotAlready(c);
                }
            }
        }
        
        void connection_send_handler (Context c)
        {
            if (m_state == State::WAIT_SEND_BUF) {
                if (m_connection.getSendBufferSpace(c) >= Params::ExpectedResponseLength) {
                    m_state = State::RECV_REQUEST_LINE;
                    m_parse_event.prependNowNotAlready(c);
                }
            }
        }
        
        void parse_event_handler (Context c)
        {
            char *line_buf;
            size_t line_max_length;
            switch (m_state) {
                case State::RECV_REQUEST_LINE: {
                    line_buf = m_request_line;
                    line_max_length = Params::MaxRequestLineLength;
                } break;
                case State::RECV_HEADER_LINE: {
                    line_buf = m_header_line;
                    line_max_length = Params::MaxHeaderLineLength;
                } break;
                default: AMBRO_ASSERT(false);
            }
            
            size_t pos;
            bool end_of_line = false;
            for (pos = 0; pos < m_rx_buf_length; pos++) {
                if (!m_line_overflow && m_line_length >= line_max_length) {
                    m_line_overflow = true;
                }
                
                char ch = m_rx_buf[buf_add(m_rx_buf_start, pos)];
                
                if (AMBRO_UNLIKELY(ch == '\0')) {
                    ThePrinterMain::print_pgm_string(c, AMBRO_PSTR("//HttpClientNullInLine\n"));
                    return disconnect(c);
                }
                
                if (!m_line_overflow) {
                    line_buf[m_line_length] = ch;
                    m_line_length++;
                }
                
                if (ch == '\n') {
                    end_of_line = true;
                    break;
                }
            }
            
            m_rx_buf_start = buf_add(m_rx_buf_start, pos);
            m_rx_buf_length -= pos;
            m_connection.acceptReceivedData(c, pos);
            
            if (!end_of_line) {
                return;
            }
            
            size_t length = m_line_length - 1;
            if (length > 0 && m_request_line[length - 1] == '\r') {
                length--;
            }
            line_buf[length] = '\0';
            bool overflow = m_line_overflow;
            
            m_line_length = 0;
            m_line_overflow = false;
            
            if (m_state == State::RECV_REQUEST_LINE) {
                m_request_line_overflow = overflow;
                m_request_line_length = length;
                m_state = State::RECV_HEADER_LINE;
                m_parse_event.prependNowNotAlready(c);
            } else {
                if (length > 0) {
                    if (!overflow) {
                        handle_header(c, m_header_line, length);
                    }
                    m_parse_event.prependNowNotAlready(c);
                } else {
                    return request_head_received(c);
                }
            }
        }
        
        void handle_header (Context c, char const *header, size_t length)
        {
            if (remove_header_name(&header, &length, "content-length")) {
                char *endptr;
                unsigned long long int value = strtoull(header, &endptr, 10);
                if (endptr != header + length || length == 0) {
                    m_bad_content_length = true;
                } else {
                    m_req_content_length = value;
                    m_have_content_length = true;
                }
            }
            else if (remove_header_name(&header, &length, "transfer-encoding")) {
                if (!data_equals_caseins(header, length, "identity")) {
                    if (data_equals_caseins(header, length, "chunked")) {
                        m_have_chunked = true;
                    } else {
                        m_bad_transfer_encoding = true;
                    }
                }
            }
            else if (remove_header_name(&header, &length, "expect")) {
                if (data_equals_caseins(header, length, "100-continue")) {
                    m_100_continue = true;
                } else {
                    m_expectation_failed = true;
                }
            }
        }
        
        void request_head_received (Context c)
        {
            // Initialize some states related to request handling and response,
            // possibly changed later on.
            m_have_body = false;
            m_resp_status = HttpStatusCodes::InternalServerError();
            m_resp_have_user_body = false;
            m_resp_content_type = "text/plain; charset=utf-8";
            m_request_body_callback = TheRequestInterface::RequestBodyCallback::MakeNull();
            m_response_body_callback = TheRequestInterface::ResponseBodyCallback::MakeNull();
            
            do {
                char *buf = m_request_line;
                size_t buf_length = m_request_line_length;
                
                // Extract the request method.
                char *first_space = (char *)memchr(buf, ' ', buf_length);
                if (!first_space) {
                    m_resp_status = HttpStatusCodes::BadRequest();
                    goto respond;
                }
                *first_space = '\0';
                m_request_method = buf;
                size_t method_length = first_space - buf;
                buf        += method_length + 1;
                buf_length -= method_length + 1;
                
                // Extract the request path.
                char *second_space = (char *)memchr(buf, ' ', buf_length);
                if (!second_space) {
                    m_resp_status = HttpStatusCodes::BadRequest();
                    goto respond;
                }
                *second_space = '\0';
                m_request_path = buf;
                size_t path_length = second_space - buf;
                buf        += path_length + 1;
                buf_length -= path_length + 1;
                
                // Extract the HTTP version string.
                if (memchr(buf, ' ', buf_length)) {
                    m_resp_status = HttpStatusCodes::BadRequest();
                    goto respond;
                }
                char const *http = buf;
                size_t http_length = buf_length;
                
                // Remove HTTP/ prefix from the version.
                if (!remove_prefix(&http, &http_length, "HTTP/")) {
                    m_resp_status = HttpStatusCodes::HttpVersionNotSupported();
                    goto respond;
                }
                
                // Parse the major and minor version.
                char *endptr1;
                unsigned long int http_major = strtoul(http, &endptr1, 10);
                if (*endptr1 != '/' || endptr1 == http) {
                    m_resp_status = HttpStatusCodes::HttpVersionNotSupported();
                    goto respond;
                }
                char *endptr2;
                unsigned long int http_minor = strtoul(endptr1 + 1, &endptr2, 10);
                if (*endptr2 != '\0' || endptr2 == endptr1 + 1) {
                    m_resp_status = HttpStatusCodes::HttpVersionNotSupported();
                    goto respond;
                }
                
                // Check the version.
                if (http_major != 1 || http_minor == 0) {
                    m_resp_status = HttpStatusCodes::HttpVersionNotSupported();
                    goto respond;
                }
                
                // Check for request line overflow.
                if (m_request_line_overflow) {
                    m_resp_status = HttpStatusCodes::UriTooLong();
                    goto respond;
                }
                
                // Check request-body-related stuff.
                if (m_bad_content_length || m_bad_transfer_encoding || (m_have_content_length && m_have_chunked)) {
                    m_resp_status = HttpStatusCodes::BadRequest();
                    goto respond;
                }
                
                // Determine if we are receiving a request body.
                if (m_have_content_length || m_have_chunked) {
                    m_have_body = true;
                }
                
                // Set this state temporarily so that we can have assertions
                // in HttpRequestInterface calls.
                m_state = State::HEAD_RECEIVED;
                
                RequestHandler::call(c, this);
            } while (false);
            
        respond:
            if (m_have_body) {
                // TBD receive body!
            } else {
                // We do not have a request body, so start sending the response.
                request_completely_received(c);
            }
        }
        
        void request_completely_received (Context c)
        {
            // If the user will not provide a body, we will send the status string as the body.
            if (!m_resp_have_user_body) {
                m_resp_length_known = true;
                m_resp_length = strlen(m_resp_status);
            }
            
            // Send headers.
            send_string(c, "HTTP/1.1 ");
            send_string(c, m_resp_status);
            send_string(c, "\r\nServer: Aprinter\r\nContent-Type: ");
            send_string(c, m_resp_content_type);
            send_string(c, "\r\n");
            if (m_resp_length_known) {
                send_string(c, "Content-Length: ");
                char length_buf[12];
                sprintf(length_buf, "%" PRIu32, m_resp_length);
                send_string(c, length_buf);
            } else {
                send_string(c, "Transfer-Encoding: chunked");
            }
            send_string(c, "\r\n\r\n");
            
            if (m_resp_have_user_body) {
                // TBD: user sends body
                
            } else {
                // Send the status as the body.
                send_string(c, m_resp_status);
                
                // Request is handled, we're ready for another request.
                prepare_for_request(c);
            }
        }
        
        char const * getMethod (Context c)
        {
            AMBRO_ASSERT(have_request(c))
            return m_request_method;
        }
        
        char const * getPath (Context c)
        {
            AMBRO_ASSERT(have_request(c))
            return m_request_path;
        }
        
        bool hasBody (Context c)
        {
            AMBRO_ASSERT(have_request(c))
            return m_have_body;
        }
        
        void setRequestBodyCallback (Context c, typename TheRequestInterface::RequestBodyCallback callback)
        {
            AMBRO_ASSERT(have_request(c))
            m_request_body_callback = callback;
        }
        
        void setResponseBodyCallback (Context c, typename TheRequestInterface::ResponseBodyCallback callback)
        {
            AMBRO_ASSERT(have_request(c))
            m_response_body_callback = callback;
        }
        
        void setResponseStatus (Context c, char const *status)
        {
            AMBRO_ASSERT(have_request_before_headers_sent(c))
            AMBRO_ASSERT(status)
            return m_resp_status = status;
        }
        
        void provideResponseBody (Context c, char const *content_type, bool length_is_known, uint32_t length)
        {
            AMBRO_ASSERT(have_request_before_headers_sent(c))
            AMBRO_ASSERT(content_type)
            
            m_resp_have_user_body = true;
            m_resp_content_type = content_type;
            m_resp_length_known = length_is_known;
            m_resp_length = length;
        }
        
        void getRequestBodyChunk (Context c, char const **data, size_t *length)
        {
            AMBRO_ASSERT(have_request_before_headers_sent(c))
            // TBD
        }
        
        void acceptRequestBodyData (Context c, size_t length)
        {
            AMBRO_ASSERT(have_request_before_headers_sent(c))
            // TBD
        }
        
        void getResponseBodyChunk (Context c, char **data, size_t *length)
        {
            // TBD
        }
        
        void provideResponseBodyData (Context c, size_t length)
        {
            // TBD
        }
        
        typename Context::EventLoop::QueuedEvent m_parse_event;
        TheTcpConnection m_connection;
        typename TheRequestInterface::RequestBodyCallback m_request_body_callback;
        typename TheRequestInterface::ResponseBodyCallback m_response_body_callback;
        State m_state;
        size_t m_rx_buf_start;
        size_t m_rx_buf_length;
        size_t m_line_length;
        size_t m_request_line_length;
        uint64_t m_req_content_length;
        bool m_line_overflow;
        bool m_request_line_overflow;
        bool m_bad_content_length;
        bool m_have_content_length;
        bool m_have_chunked;
        bool m_bad_transfer_encoding;
        bool m_100_continue;
        bool m_expectation_failed;
        bool m_have_body;
        char const *m_request_method;
        char const *m_request_path;
        char const *m_resp_status;
        bool m_resp_have_user_body;
        bool m_resp_length_known;
        uint32_t m_resp_length;
        char const *m_resp_content_type;
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
    size_t TExpectedResponseLength
>
struct HttpServerService {
    static uint16_t Port = TPort;
    static int const MaxClients = TMaxClients;
    static size_t const MaxRequestLineLength = TMaxRequestLineLength;
    static size_t const MaxHeaderLineLength = TMaxHeaderLineLength;
    static size_t const ExpectedResponseLength = TExpectedResponseLength;
    
    template <typename Context, typename ParentObject, typename ThePrinterMain, typename RequestHandler>
    using Server = HttpServer<Context, ParentObject, ThePrinterMain, RequestHandler, HttpServerService>;
};

#include <aprinter/EndNamespace.h>

#endif
