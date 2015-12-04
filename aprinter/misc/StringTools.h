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

static bool AsciiCaseInsensStringEqual (char const *str1, char const *str2)
{
    while (1) {
        char c1 = AsciiToLower(*str1);
        char c2 = AsciiToLower(*str2);
        if (c1 != c2) {
            return false;
        }
        if (c1 == '\0') {
            return true;
        }
        ++str1;
        ++str2;
    }
}

static bool StringRemovePrefix (char const **data, size_t *length, char const *prefix)
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

static bool StringEqualsCaseIns (char const *data, size_t length, char const *low_str)
{
    while (length > 0 && *low_str != '\0' && AsciiToLower(*data) == *low_str) {
        data++;
        length--;
        low_str++;
    }
    return (length == 0 && *low_str == '\0');
}

#include <aprinter/EndNamespace.h>

#endif
