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

#ifndef APRINTER_HTTP_STRING_TOOLS_H
#define APRINTER_HTTP_STRING_TOOLS_H

#include <stddef.h>
#include <string.h>
#include <stdint.h>

#include <aprinter/base/Hints.h>
#include <aprinter/misc/StringTools.h>

#include <aipstack/misc/MemRef.h>

namespace APrinter {

static bool HttpAsciiCaseInsensEndsWith (AIpStack::MemRef str1, char const *str2_low)
{
    size_t str2_len = strlen(str2_low);
    while (str1.len > 0 && str2_len > 0) {
        str1.len--;
        str2_len--;
        if (AsciiToLower(str1.ptr[str1.len]) != str2_low[str2_len]) {
            return false;
        }
    }
    return (str2_len == 0);
}

static bool HttpStringRemovePrefix (char **data, char const *prefix)
{
    size_t pos = 0;
    while (prefix[pos] != '\0') {
        if ((*data)[pos] != prefix[pos]) {
            return false;
        }
        pos++;
    }
    *data += pos;
    return true;
}

static bool HttpMemEqualsCaseIns (AIpStack::MemRef data, char const *low_str)
{
    size_t pos = 0;
    while (pos < data.len && low_str[pos] != '\0' && AsciiToLower(data.ptr[pos]) == low_str[pos]) {
        pos++;
    }
    return (pos == data.len && low_str[pos] == '\0');
}

static bool HttpStringRemoveHeader (char const **data, char const *low_header_name)
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

namespace HttpStringToolsPrivate {
    inline static bool is_http_token_sep (char ch)
    {
        return (ch == ' ' || ch == '\t' || ch == ',');
    }
}

template <typename TokenCallback>
void HttpStringIterTokens (AIpStack::MemRef data, TokenCallback token_cb)
{
    while (true) {
        while (data.len > 0 && HttpStringToolsPrivate::is_http_token_sep(data.ptr[0])) {
            data = data.subFrom(1);
        }
        
        if (data.len == 0) {
            break;
        }
        
        size_t token_len = 1;
        while (token_len < data.len && !HttpStringToolsPrivate::is_http_token_sep(data.ptr[token_len])) {
            token_len++;
        }
        
        AIpStack::MemRef token = data.subTo(token_len);
        token_cb(token);
        
        data = data.subFrom(token_len);
    }
}

static bool HttpStringParseHexadecimal (AIpStack::MemRef data, uint64_t *out)
{
    while (data.len > 0 && *data.ptr == '0') {
        data.ptr++;
        data.len--;
    }
    
#define APRINTER_PARSE_HEX_CODE(Type) \
        Type res = 0; \
        while (data.len > 0) { \
            char ch = *data.ptr++; \
            data.len--; \
            int digit; \
            if (!StringDecodeHexDigit(ch, &digit)) { \
                return false; \
            } \
            res = (res << 4) | digit; \
        } \
        *out = res;
    
    if (AMBRO_LIKELY(data.len <= 8)) {
        APRINTER_PARSE_HEX_CODE(uint32_t)
    }
    else if (data.len <= 16) {
        APRINTER_PARSE_HEX_CODE(uint64_t)
    }
    else {
        return false;
    }
    
    return true;
}

}

#endif
