/*
 * Copyright (c) 2016 Ambroz Bizjak
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

#ifndef AMBROLIB_JSON_BUILDER_H
#define AMBROLIB_JSON_BUILDER_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <math.h>

#include <aprinter/base/Hints.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/MemRef.h>
#include <aprinter/math/FloatTools.h>

#include <aprinter/BeginNamespace.h>

struct JsonUint32 {
    uint32_t val;
};

struct JsonDouble {
    double val;
};

struct JsonBool {
    bool val;
};

struct JsonNull {};

struct JsonString {
    MemRef val;
};

struct JsonSafeString {
    char const *val;
};

struct JsonSafeChar {
    char val;
};

class JsonBuilder {
public:
    void loadBuffer (char *buffer, size_t buffer_total_size)
    {
        AMBRO_ASSERT(buffer_total_size > 0)
        
        m_buffer = buffer;
        m_buffer_size = buffer_total_size - 1;
        m_length = 0;
    }
    
    size_t getLength ()
    {
        AMBRO_ASSERT(m_length <= m_buffer_size)
        return m_length;
    }
    
    void start ()
    {
        m_inhibit_comma = true;
    }
    
    void add (JsonUint32 val)
    {
        adding_element();
        
        char *end = get_end();
        snprintf(end, get_rem()+1, "%" PRIu32, val.val);
        m_length += strlen(end);
    }
    
    void add (JsonDouble val)
    {
        adding_element();
        
        if (AMBRO_UNLIKELY(val.val == INFINITY)) {
            add_token("1e1024");
        }
        else if (AMBRO_UNLIKELY(val.val == -INFINITY || FloatIsNan(val.val))) {
            add_token("-1e1024");
        }
        else {
            char *end = get_end();
            snprintf(end, get_rem()+1, "%.6g", val.val);
            m_length += strlen(end);
        }
    }
    
    void add (JsonBool val)
    {
        adding_element();
        
        add_token(val.val ? "true" : "false");
    }
    
    void add (JsonNull)
    {
        adding_element();
        
        add_token("null");
    }
    
    void beginString ()
    {
        adding_element();
        
        add_char('"');
    }
    
    void addStringChar (char ch)
    {
        switch (ch) {
            case '\\':
            case '"': {
                add_char('\\');
                add_char(ch);
            } break;
            
            case '\t': {
                add_char('\\');
                add_char('t');
            } break;
            
            case '\n': {
                add_char('\\');
                add_char('n');
            } break;
            
            case '\r': {
                add_char('\\');
                add_char('r');
            } break;
            
            default: {
                if (AMBRO_UNLIKELY(ch < 0x20)) {
                    add_char('\\');
                    add_char('u');
                    add_char('0');
                    add_char('0');
                    int ich = ch;
                    add_char(encode_hex_digit(ich>>4));
                    add_char(encode_hex_digit(ich&0xF));
                } else {
                    add_char(ch);
                }
            } break;
        }
    }
    
    void addStringMem (MemRef mem)
    {
        for (size_t pos = 0; pos < mem.len; pos++) {
            addStringChar(mem.ptr[pos]);
        }
    }
    
    void endString ()
    {
        add_char('"');
    }
    
    void add (JsonString val)
    {
        beginString();
        addStringMem(val.val);
        endString();
    }
    
    void add (JsonSafeString val)
    {
        beginString();
        add_token(val.val);
        endString();
    }
    
    void add (JsonSafeChar val)
    {
        beginString();
        add_char(val.val);
        endString();
    }
    
    void startArray ()
    {
        start_list('[');
    }
    
    void endArray ()
    {
        end_list(']');
    }
    
    void startObject ()
    {
        start_list('{');
    }
    
    void endObject ()
    {
        end_list('}');
    }
    
    void entryValue ()
    {
        add_char(':');
        m_inhibit_comma = true;
    }
    
    template <typename TKey, typename TVal>
    void addKeyVal (TKey key, TVal val)
    {
        add(key);
        entryValue();
        add(val);
    }
    
    template <typename TVal>
    void addSafeKeyVal (char const *key, TVal val)
    {
        addKeyVal(JsonSafeString{key}, val);
    }
    
    template <typename TKey>
    void addKeyObject (TKey key)
    {
        add(key);
        entryValue();
        startObject();
    }
    
    template <typename TKey>
    void addKeyArray (TKey key)
    {
        add(key);
        entryValue();
        startArray();
    }
    
private:
    static char encode_hex_digit (int value)
    {
        return (value < 10) ? ('0' + value) : ('A' + (value - 10));
    }
    
    char * get_end ()
    {
        return m_buffer + m_length;
    }
    
    size_t get_rem ()
    {
        return m_buffer_size - m_length;
    }
    
    void add_char (char ch)
    {
        if (AMBRO_LIKELY(m_length < m_buffer_size)) {
            m_buffer[m_length++] = ch;
        }
    }
    
    void add_token (char const *token)
    {
        while (*token != '\0') {
            add_char(*token++);
        }
    }
    
    void start_list (char paren)
    {
        adding_element();
        
        add_char(paren);
        m_inhibit_comma = true;
    }
    
    void end_list (char paren)
    {
        add_char(paren);
        m_inhibit_comma = false;
    }
    
    void adding_element ()
    {
        if (m_inhibit_comma) {
            m_inhibit_comma = false;
        } else {
            add_char(',');
        }
    }
    
    char *m_buffer;
    size_t m_buffer_size;
    size_t m_length;
    bool m_inhibit_comma;
};

#include <aprinter/EndNamespace.h>

#endif
