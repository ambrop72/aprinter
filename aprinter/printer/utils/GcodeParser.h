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

#include <aprinter/meta/AliasStruct.h>
#include <aprinter/math/FloatTools.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Hints.h>
#include <aprinter/printer/utils/GcodeCommand.h>

#include <aprinter/BeginNamespace.h>

struct GcodeParserTypeSerial {};
struct GcodeParserTypeFile {};

template <typename TheParserType>
struct GcodeParserExtraMembers {
    bool m_continuing_comment_line;
};

template <>
struct GcodeParserExtraMembers<GcodeParserTypeSerial> {
    uint8_t m_checksum;
};

template <typename Context, typename TBufferSizeType, typename FpType, typename ParserType, typename Params>
class GcodeParser
: public GcodeCommand<Context, FpType>,
  private SimpleDebugObject<Context>,
  private GcodeParserExtraMembers<ParserType>
{
    static_assert(Params::MaxParts > 0, "");
    static_assert(Params::MaxParts <= 127, "");
    
public:
    using BufferSizeType = TBufferSizeType;
    using PartsSizeType = int8_t;
    using TheGcodeCommand = GcodeCommand<Context, FpType>;
    using PartRef = typename TheGcodeCommand::PartRef;
    
    template <typename TheParserType, typename Dummy = void>
    struct CommandExtra {};
    
    struct CommandPart {
        char code;
        char *data;
    };
    
    struct Command : public CommandExtra<ParserType> {
        BufferSizeType length;
        PartsSizeType num_parts;
        uint16_t cmd_number;
        CommandPart parts[Params::MaxParts];
    };
    
    template <typename Dummy>
    struct CommandExtra<GcodeParserTypeSerial, Dummy> {
        bool have_line_number;
        uint32_t line_number;
    };
    
    void init (Context c)
    {
        m_state = STATE_NOCMD;
        TheTypeHelper::init_hook(c, this);
        
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
        m_command.length = 0;
        m_command.num_parts = assume_error;
        TheTypeHelper::init_command_hook(c, this);
    }
    
    bool extendCommand (Context c, BufferSizeType avail, bool line_buffer_exhausted=false)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_state != STATE_NOCMD)
        AMBRO_ASSERT(avail >= m_command.length)
        
        for (; m_command.length < avail; m_command.length++) {
            char ch = m_buffer[m_command.length];
            
            if (AMBRO_UNLIKELY(ch == '\n')) {
                if (m_command.num_parts >= 0) {
                    if (m_state == STATE_INSIDE) {
                        finish_part(c);
                    } else if (TheTypeHelper::ChecksumEnabled && m_state == STATE_CHECKSUM) {
                        TheTypeHelper::checksum_check_hook(c, this);
                    }
                    if (m_command.num_parts >= 0) {
                        if (m_command.num_parts == 0) {
                            m_command.num_parts = GCODE_ERROR_NO_PARTS;
                        } else {
                            m_command.num_parts--;
                            m_command.cmd_number = atoi(m_command.parts[0].data);
                        }
                    }
                }
                m_command.length++;
                m_state = STATE_NOCMD;
                TheTypeHelper::newline_handled_hook(c, this);
                return true;
            }
            
            if (AMBRO_UNLIKELY(m_command.num_parts < 0 || (TheTypeHelper::ChecksumEnabled && m_state == STATE_CHECKSUM))) {
                continue;
            }
            
            if (AMBRO_UNLIKELY(TheTypeHelper::ChecksumEnabled && ch == '*')) {
                if (m_state == STATE_INSIDE) {
                    finish_part(c);
                }
                m_temp = m_command.length;
                m_state = STATE_CHECKSUM;
                continue;
            }
            
            TheTypeHelper::checksum_add_hook(c, this, ch);
            
            if (TheTypeHelper::CommentsEnabled) {
                if (AMBRO_UNLIKELY(m_state == STATE_COMMENT)) {
                    continue;
                }
                if (AMBRO_UNLIKELY(ch == ';')) {
                    if (m_state == STATE_INSIDE) {
                        finish_part(c);
                    }
                    m_state = STATE_COMMENT;
                    continue;
                }
            }
            
            if (AMBRO_UNLIKELY(m_state == STATE_OUTSIDE)) {
                if (AMBRO_LIKELY(!is_space(ch))) {
                    if (TheTypeHelper::EofEnabled) {
                        if (m_command.num_parts == 0 && ch == 'E') {
                            m_command.length++;
                            m_command.num_parts = GCODE_ERROR_EOF;
                            m_state = STATE_NOCMD;
                            return true;
                        }
                    }
                    if (!is_code(ch)) {
                        m_command.num_parts = GCODE_ERROR_INVALID_PART;
                    }
                    m_temp = m_command.length;
                    m_state = STATE_INSIDE;
                }
            } else {
                if (AMBRO_UNLIKELY(is_space(ch))) {
                    finish_part(c);
                    m_state = STATE_OUTSIDE;
                }
            }
        }
        
        if (line_buffer_exhausted) {
            if (TheTypeHelper::handle_exhausted_line(c, this)) {
                return true;
            }
        }
        
        return false;
    }
    
    void resetCommand (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_state != STATE_NOCMD)
        
        m_state = STATE_NOCMD;
    }
    
    BufferSizeType getLength (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_state == STATE_NOCMD)
        
        return m_command.length;
    }
    
    PartsSizeType getNumParts (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_state == STATE_NOCMD)
        
        return m_command.num_parts;
    }
    
    char getCmdCode (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_state == STATE_NOCMD)
        AMBRO_ASSERT(m_command.num_parts >= 0)
        
        return m_command.parts[0].code;
    }
    
    uint16_t getCmdNumber (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_state == STATE_NOCMD)
        AMBRO_ASSERT(m_command.num_parts >= 0)
        
        return m_command.cmd_number;
    }
    
    PartRef getPart (Context c, PartsSizeType i)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_state == STATE_NOCMD)
        AMBRO_ASSERT(m_command.num_parts >= 0)
        AMBRO_ASSERT(i >= 0)
        AMBRO_ASSERT(i < m_command.num_parts)
        
        return PartRef{&m_command.parts[1 + i]};
    }
    
    char getPartCode (Context c, PartRef part)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_state == STATE_NOCMD)
        AMBRO_ASSERT(m_command.num_parts >= 0)
        
        return cast_part_ref(part)->code;
    }
    
    FpType getPartFpValue (Context c, PartRef part)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_state == STATE_NOCMD)
        AMBRO_ASSERT(m_command.num_parts >= 0)
        
        return StrToFloat<FpType>(cast_part_ref(part)->data, NULL);
    }
    
    uint32_t getPartUint32Value (Context c, PartRef part)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_state == STATE_NOCMD)
        AMBRO_ASSERT(m_command.num_parts >= 0)
        
        return strtoul(cast_part_ref(part)->data, NULL, 10);
    }
    
    char const * getPartStringValue (Context c, PartRef part)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_state == STATE_NOCMD)
        AMBRO_ASSERT(m_command.num_parts >= 0)
        
        return cast_part_ref(part)->data;
    }
    
    char * getBuffer (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_state != STATE_NOCMD)
        
        return m_buffer;
    }
    
    Command * getCmd (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_state == STATE_NOCMD)
        
        return &m_command;
    }
    
private:
    enum {STATE_NOCMD, STATE_OUTSIDE, STATE_INSIDE, STATE_COMMENT, STATE_CHECKSUM};
    
    template <typename TheParserType, typename Dummy = void>
    struct TypeHelper;
    
    template <typename Dummy>
    struct TypeHelper<GcodeParserTypeSerial, Dummy> {
        static const bool ChecksumEnabled = true;
        static const bool CommentsEnabled = false;
        static const bool EofEnabled = false;
        
        static void init_hook (Context c, GcodeParser *o)
        {
        }
        
        static void init_command_hook (Context c, GcodeParser *o)
        {
            o->m_checksum = 0;
            o->m_command.have_line_number = false;
        }
        
        static bool finish_part_hook (Context c, GcodeParser *o, char code)
        {
            if (AMBRO_UNLIKELY(!o->m_command.have_line_number && o->m_command.num_parts == 0 && code == 'N')) {
                o->m_command.have_line_number = true;
                o->m_command.line_number = strtoul(o->m_buffer + (o->m_temp + 1), NULL, 10);
                return true;
            }
            return false;
        }
        
        static void checksum_add_hook (Context c, GcodeParser *o, char ch)
        {
            o->m_checksum ^= (unsigned char)ch;
        }
        
        static void checksum_check_hook (Context c, GcodeParser *o)
        {
            AMBRO_ASSERT(o->m_command.num_parts >= 0)
            AMBRO_ASSERT(o->m_state == STATE_CHECKSUM)
            
            char *received = o->m_buffer + (o->m_temp + 1);
            BufferSizeType received_len = o->m_command.length - (o->m_temp + 1);
            
            if (AMBRO_UNLIKELY(!compare_checksum(o->m_checksum, received, received_len))) {
                o->m_command.num_parts = GCODE_ERROR_CHECKSUM;
            }
        }
        
        static bool handle_exhausted_line (Context c, GcodeParser *o)
        {
            return false;
        }
        
        static void newline_handled_hook (Context c, GcodeParser *o)
        {
        }
    };
    
    template <typename Dummy>
    struct TypeHelper<GcodeParserTypeFile, Dummy> {
        static const bool ChecksumEnabled = false;
        static const bool CommentsEnabled = true;
        static const bool EofEnabled = true;
        
        static void init_hook (Context c, GcodeParser *o)
        {
            o->m_continuing_comment_line = false;
        }
        
        static void init_command_hook (Context c, GcodeParser *o)
        {
            if (o->m_continuing_comment_line) {
                o->m_state = STATE_COMMENT;
            }
        }
        
        static bool finish_part_hook (Context c, GcodeParser *o, char code)
        {
            return false;
        }
        
        static void checksum_add_hook (Context c, GcodeParser *o, char ch)
        {
        }
        
        static void checksum_check_hook (Context c, GcodeParser *o)
        {
        }
        
        static bool handle_exhausted_line (Context c, GcodeParser *o)
        {
            if (o->m_state == STATE_COMMENT && o->m_command.num_parts == 0) {
                o->m_continuing_comment_line = true;
                o->m_command.num_parts = GCODE_ERROR_NO_PARTS;
                o->m_state = STATE_NOCMD;
                return true;
            }
            return false;
        }
        
        static void newline_handled_hook (Context c, GcodeParser *o)
        {
            o->m_continuing_comment_line = false;
        }
    };
    
    using TheTypeHelper = TypeHelper<ParserType>;
    
    static CommandPart * cast_part_ref (PartRef part_ref)
    {
        return (CommandPart *)part_ref.ptr;
    }
    
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
    
    void finish_part (Context c)
    {
        AMBRO_ASSERT(m_command.num_parts >= 0)
        AMBRO_ASSERT(m_state == STATE_INSIDE)
        AMBRO_ASSERT(is_code(m_buffer[m_temp]))
        
        if (AMBRO_UNLIKELY(m_command.num_parts == Params::MaxParts)) {
            m_command.num_parts = GCODE_ERROR_TOO_MANY_PARTS;
            return;
        }
        
        char code = m_buffer[m_temp];
        
        BufferSizeType in_pos = m_temp + 1;
        BufferSizeType out_pos = in_pos;
        
        while (in_pos < m_command.length) {
            char ch = m_buffer[in_pos++];
            if (ch == '\\' && m_command.num_parts > 0) {
                if (m_command.length - in_pos < 2) {
                    m_command.num_parts = GCODE_ERROR_BAD_ESCAPE;
                    return;
                }
                int digit_h = read_hex_digit(m_buffer[in_pos++]);
                int digit_l = read_hex_digit(m_buffer[in_pos++]);
                if (digit_h < 0 || digit_l < 0) {
                    m_command.num_parts = GCODE_ERROR_BAD_ESCAPE;
                    return;
                }
                unsigned char byte = (digit_h << 4) | digit_l;
                m_buffer[out_pos++] = *(char *)&byte;
            } else {
                m_buffer[out_pos++] = ch;
            }
        }
        
        m_buffer[out_pos] = '\0';
        
        if (TheTypeHelper::finish_part_hook(c, this, code)) {
            return;
        }
        
        m_command.parts[m_command.num_parts].code = code;
        m_command.parts[m_command.num_parts].data = m_buffer + (m_temp + 1);
        m_command.num_parts++;
    }
    
    static int read_hex_digit (char ch)
    {
        return
            (ch >= '0' && ch <= '9') ? (ch - '0') :
            (ch >= 'A' && ch <= 'F') ? (10 + (ch - 'A')) :
            (ch >= 'a' && ch <= 'f') ? (10 + (ch - 'a')) :
            -1;
    }
    
private:
    uint8_t m_state;
    char *m_buffer;
    BufferSizeType m_temp;
    Command m_command;
};

APRINTER_ALIAS_STRUCT_EXT(SerialGcodeParserService, (
    APRINTER_AS_VALUE(int, MaxParts)
), (
    template <typename Context, typename TBufferSizeType, typename FpType>
    using Parser = GcodeParser<Context, TBufferSizeType, FpType, GcodeParserTypeSerial, SerialGcodeParserService>;
))

APRINTER_ALIAS_STRUCT_EXT(FileGcodeParserService, (
    APRINTER_AS_VALUE(int, MaxParts)
), (
    template <typename Context, typename TBufferSizeType, typename FpType>
    using Parser = GcodeParser<Context, TBufferSizeType, FpType, GcodeParserTypeFile, FileGcodeParserService>;
))

#include <aprinter/EndNamespace.h>

#endif
