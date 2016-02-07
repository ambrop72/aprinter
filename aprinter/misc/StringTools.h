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

#ifndef APRINTER_STRING_TOOLS_H
#define APRINTER_STRING_TOOLS_H

#include <stddef.h>
#include <string.h>
#include <stdint.h>

#include <aprinter/base/MemRef.h>
#include <aprinter/base/Likely.h>
#include <aprinter/base/Inline.h>

#include <aprinter/BeginNamespace.h>

static char AsciiToLower (char c)
{
    return (c >= 'A' && c <= 'Z') ? (c + 32) : c;
}

static bool AsciiCaseInsensStringEqualToMem (char const *str1, char const *str2, size_t str2_len)
{
    while (*str1 != '\0') {
        if (str2_len == 0 || AsciiToLower(*str1) != AsciiToLower(*str2)) {
            return false;
        }
        str1++;
        str2++;
        str2_len--;
    }
    return (str2_len == 0);
}

static bool AsciiCaseInsensEndsWith (char const *str1, size_t str1_len, char const *str2_low)
{
    size_t str2_len = strlen(str2_low);
    while (str1_len > 0 && str2_len > 0) {
        str1_len--;
        str2_len--;
        if (AsciiToLower(str1[str1_len]) != str2_low[str2_len]) {
            return false;
        }
    }
    return (str2_len == 0);
}

static bool StringRemovePrefix (char **data, char const *prefix)
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

static bool MemEqualsCaseIns (MemRef data, char const *low_str)
{
    size_t pos = 0;
    while (pos < data.len && low_str[pos] != '\0' && AsciiToLower(data.ptr[pos]) == low_str[pos]) {
        pos++;
    }
    return (pos == data.len && low_str[pos] == '\0');
}

static bool StringRemoveHttpHeader (char const **data, char const *low_header_name)
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

namespace StringToolsPrivate {
    inline static bool is_http_token_sep (char ch)
    {
        return (ch == ' ' || ch == '\t' || ch == ',');
    }
}

template <typename TokenCallback>
void StringIterHttpTokens (MemRef data, TokenCallback token_cb)
{
    while (true) {
        while (data.len > 0 && StringToolsPrivate::is_http_token_sep(data.ptr[0])) {
            data = data.subFrom(1);
        }
        
        if (data.len == 0) {
            break;
        }
        
        size_t token_len = 1;
        while (token_len < data.len && !StringToolsPrivate::is_http_token_sep(data.ptr[token_len])) {
            token_len++;
        }
        
        MemRef token = data.subTo(token_len);
        token_cb(token);
        
        data = data.subFrom(token_len);
    }
}

namespace StringToolsPrivate {
    AMBRO_ALWAYS_INLINE
    static bool DecodeHexDigit (char c, int *out)
    {
        if (c >= '0' && c <= '9') {
            *out = c - '0';
        }
        else if (c >= 'A' && c <= 'F') {
            *out = 10 + (c - 'A');
        }
        else if (c >= 'a' && c <= 'f') {
            *out = 10 + (c - 'a');
        }
        else {
            return false;
        }
        return true;
    }
}

static bool StringParseHexadecimal (MemRef data, uint64_t *out)
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
            if (!StringToolsPrivate::DecodeHexDigit(ch, &digit)) { \
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

#include <aprinter/EndNamespace.h>

#endif
