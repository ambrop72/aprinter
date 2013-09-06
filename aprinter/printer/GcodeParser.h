/*
 * Copyright (c) 2013 Ambroz Bizjak
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

#ifndef AMBROLIB_GCODE_PARSER_H
#define AMBROLIB_GCODE_PARSER_H

#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>

#include <aprinter/meta/BitsInInt.h>
#include <aprinter/meta/ChooseInt.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>

#include <aprinter/BeginNamespace.h>

template <int TMaxParts>
struct GcodeParserParams {
    static const int MaxParts = TMaxParts;
};

template <typename Context, typename Params, typename TBufferSizeType>
class GcodeParser
: private DebugObject<Context, void>
{
    static_assert(Params::MaxParts > 0, "");
    
public:
    using BufferSizeType = TBufferSizeType;
    using PartsSizeType = typename ChooseInt<BitsInInt<Params::MaxParts>::value, true>::Type;
    
    enum {
        ERROR_TOO_MANY_PARTS = -1,
        ERROR_INVALID_PART = -2,
        ERROR_CHECKSUM = -3,
        ERROR_RECV_OVERRUN = -4
    };
    
    struct CommandPart {
        char code;
        char *data;
        BufferSizeType length;
    };
    
    struct Command {
        BufferSizeType length;
        bool have_line_number;
        uint32_t line_number;
        PartsSizeType num_parts;
        CommandPart parts[Params::MaxParts];
    };
    
    void init (Context c)
    {
        m_state = STATE_NOCMD;
        
        this->debugInit(c);
    }
    
    void deinit (Context c)
    {
        this->debugDeinit(c);
    }
    
    bool haveCommand (Context c)
    {
        this->debugAccess(c);
        
        return (m_state != STATE_NOCMD);
    }
    
    void startCommand (Context c, char *buffer, int8_t assume_error)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_state == STATE_NOCMD)
        AMBRO_ASSERT(buffer)
        AMBRO_ASSERT(assume_error <= 0)
        
        m_state = STATE_OUTSIDE;
        m_buffer = buffer;
        m_checksum = 0;
        m_command.length = 0;
        m_command.have_line_number = false;
        m_command.num_parts = assume_error;
    }
    
    Command * extendCommand (Context c, BufferSizeType avail)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_state != STATE_NOCMD)
        AMBRO_ASSERT(avail >= m_command.length)
        
        for (; m_command.length < avail; m_command.length++) {
            char ch = m_buffer[m_command.length];
            
            if (ch == '\n') {
                if (m_command.num_parts >= 0) {
                    if (m_state == STATE_INSIDE) {
                        finish_part();
                    } else if (m_state == STATE_CHECKSUM) {
                        check_checksum();
                    }
                }
                m_command.length++;
                m_state = STATE_NOCMD;
                return &m_command;
            }
            
            if (m_command.num_parts < 0 || m_state == STATE_CHECKSUM) {
                continue;
            }
            
            if (ch == '*') {
                if (m_state == STATE_INSIDE) {
                    finish_part();
                }
                m_temp = m_command.length;
                m_state = STATE_CHECKSUM;
                continue;
            }
            
            m_checksum ^= (unsigned char)ch;
            
            if (m_state == STATE_OUTSIDE) {
                if (!is_space(ch)) {
                    if (!is_code(ch)) {
                        m_command.num_parts = ERROR_INVALID_PART;
                    }
                    m_temp = m_command.length;
                    m_state = STATE_INSIDE;
                }
            } else {
                if (is_space(ch)) {
                    finish_part();
                    m_state = STATE_OUTSIDE;
                }
            }
        }
        
        return NULL;
    }
    
    void resetCommand (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_state != STATE_NOCMD)
        
        m_state = STATE_NOCMD;
    }
    
private:
    enum {STATE_NOCMD, STATE_OUTSIDE, STATE_INSIDE, STATE_CHECKSUM};
    
    static bool is_code (char ch)
    {
        return ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z'));
    }
    
    static bool is_space (char ch)
    {
        return (ch == ' ' || ch == '\t' || ch == '\r');
    }
    
    static bool compare_checksum (uint8_t expected, char const *received, BufferSizeType received_len)
    {
        while (received_len > 0 && is_space(received[received_len - 1])) {
            received_len--;
        }
        
        do {
            char ch = '0' + (expected % 10);
            if (received_len == 0 || received[received_len - 1] != ch) {
                return false;
            }
            expected /= 10;
            received_len--;
        } while (expected > 0);
        
        return (received_len == 0);
    }
    
    void check_checksum ()
    {
        AMBRO_ASSERT(m_command.num_parts >= 0)
        AMBRO_ASSERT(m_state == STATE_CHECKSUM)
        
        char *received = m_buffer + (m_temp + 1);
        BufferSizeType received_len = m_command.length - (m_temp + 1);
        
        if (!compare_checksum(m_checksum, received, received_len)) {
            m_command.num_parts = ERROR_CHECKSUM;
        }
    }
    
    void finish_part ()
    {
        AMBRO_ASSERT(m_command.num_parts >= 0)
        AMBRO_ASSERT(m_state == STATE_INSIDE)
        AMBRO_ASSERT(is_code(m_buffer[m_temp]))
        
        if (m_command.num_parts == Params::MaxParts) {
            m_command.num_parts = ERROR_TOO_MANY_PARTS;
            return;
        }
        
        char code = m_buffer[m_temp];
        if (code >= 'a') {
            code -= 32;
        }
        
        m_buffer[m_command.length] = '\0';
        
        if (!m_command.have_line_number && m_command.num_parts == 0 && code == 'N') {
            m_command.have_line_number = true;
            m_command.line_number = strtoul(m_buffer + (m_temp + 1), NULL, 10);
            return;
        }
        
        m_command.parts[m_command.num_parts].code = code;
        m_command.parts[m_command.num_parts].data = m_buffer + (m_temp + 1);
        m_command.parts[m_command.num_parts].length = m_command.length - (m_temp + 1);
        m_command.num_parts++;
    }
    
    uint8_t m_state;
    char *m_buffer;
    uint8_t m_checksum;
    BufferSizeType m_temp;
    Command m_command;
};

#include <aprinter/EndNamespace.h>

#endif
