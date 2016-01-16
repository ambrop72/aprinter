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

static bool StringEqualsCaseIns (char const *data, char const *low_str)
{
    while (*data != '\0' && *low_str != '\0' && AsciiToLower(*data) == *low_str) {
        data++;
        low_str++;
    }
    return (*data == '\0' && *low_str == '\0');
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

#include <aprinter/EndNamespace.h>

#endif
