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

#ifndef APRINTER_HTTP_SERVER_COMMON_H
#define APRINTER_HTTP_SERVER_COMMON_H

#include <aprinter/BeginNamespace.h>

struct HttpStatusCodes {
    static constexpr char const * Okay() { return "200 OK"; }
    static constexpr char const * BadRequest() { return "400 Bad Request"; }
    static constexpr char const * NotFound() { return "404 Not Found"; }
    static constexpr char const * MethodNotAllowed() { return "405 Method Not Allowed"; }
    static constexpr char const * RequestTimeout() { return "408 Request Timeout"; }
    static constexpr char const * UriTooLong() { return "414 URI Too Long"; }
    static constexpr char const * ExpectationFailed() { return "417 Expectation Failed"; }
    static constexpr char const * RequestHeaderFieldsTooLarge() { return "431 Request Header Fields Too Large"; }
    static constexpr char const * InternalServerError() { return "500 Internal Server Error"; }
    static constexpr char const * HttpVersionNotSupported() { return "505 HTTP Version Not Supported"; }
};

struct HttpContentTypes {
    static constexpr char const * TextPlainUtf8() { return "text/plain; charset=utf-8"; }
};

#include <aprinter/EndNamespace.h>

#endif
