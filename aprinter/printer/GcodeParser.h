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

#include <aprinter/meta/ChooseInt.h>
#include <aprinter/base/Object.h>
#include <aprinter/math/FloatTools.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Likely.h>
#include <aprinter/base/Optimize.h>

#include <aprinter/BeginNamespace.h>

template <int TMaxParts>
struct GcodeParserParams {
    static const int MaxParts = TMaxParts;
};

struct GcodeParserTypeSerial {};
struct GcodeParserTypeFile {};

template <typename Context, typename ParentObject, typename Params, typename TBufferSizeType, typename ParserType>
class GcodeParser {
    static_assert(Params::MaxParts > 0, "");
    
public:
    struct Object;
    
private:
    using TheDebugObject = DebugObject<Context, Object>;
    
public:
    using BufferSizeType = TBufferSizeType;
    using PartsSizeType = ChooseIntForMax<Params::MaxParts, true>;
    
    enum {
        ERROR_NO_PARTS = -1,
        ERROR_TOO_MANY_PARTS = -2,
        ERROR_INVALID_PART = -3,
        ERROR_CHECKSUM = -4,
        ERROR_RECV_OVERRUN = -5,
        ERROR_EOF = -6,
        ERROR_BAD_ESCAPE = -7
    };
    
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
    
    using PartRef = CommandPart *;
    
    static void init (Context c)
    {
        auto *o = Object::self(c);
        o->m_state = STATE_NOCMD;
        
        TheDebugObject::init(c);
    }
    
    static void deinit (Context c)
    {
        TheDebugObject::deinit(c);
    }
    
    static bool haveCommand (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        return (o->m_state != STATE_NOCMD);
    }
    
    AMBRO_OPTIMIZE_SPEED
    static void startCommand (Context c, char *buffer, int8_t assume_error)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->m_state == STATE_NOCMD)
        AMBRO_ASSERT(buffer)
        AMBRO_ASSERT(assume_error <= 0)
        
        o->m_state = STATE_OUTSIDE;
        o->m_buffer = buffer;
        o->m_command.length = 0;
        o->m_command.num_parts = assume_error;
        TheTypeHelper::init_command_hook(c);
    }
    
    AMBRO_OPTIMIZE_SPEED
    static bool extendCommand (Context c, BufferSizeType avail)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->m_state != STATE_NOCMD)
        AMBRO_ASSERT(avail >= o->m_command.length)
        
        for (; o->m_command.length < avail; o->m_command.length++) {
            char ch = o->m_buffer[o->m_command.length];
            
            if (AMBRO_UNLIKELY(ch == '\n')) {
                if (o->m_command.num_parts >= 0) {
                    if (o->m_state == STATE_INSIDE) {
                        finish_part(c);
                    } else if (TheTypeHelper::ChecksumEnabled && o->m_state == STATE_CHECKSUM) {
                        TheTypeHelper::checksum_check_hook(c);
                    }
                    if (o->m_command.num_parts >= 0) {
                        if (o->m_command.num_parts == 0) {
                            o->m_command.num_parts = ERROR_NO_PARTS;
                        } else {
                            o->m_command.num_parts--;
                            o->m_command.cmd_number = atoi(o->m_command.parts[0].data);
                        }
                    }
                }
                o->m_command.length++;
                o->m_state = STATE_NOCMD;
                return true;
            }
            
            if (AMBRO_UNLIKELY(o->m_command.num_parts < 0 || (TheTypeHelper::ChecksumEnabled && o->m_state == STATE_CHECKSUM))) {
                continue;
            }
            
            if (AMBRO_UNLIKELY(TheTypeHelper::ChecksumEnabled && ch == '*')) {
                if (o->m_state == STATE_INSIDE) {
                    finish_part(c);
                }
                o->m_temp = o->m_command.length;
                o->m_state = STATE_CHECKSUM;
                continue;
            }
            
            TheTypeHelper::checksum_add_hook(c, ch);
            
            if (TheTypeHelper::CommentsEnabled) {
                if (AMBRO_UNLIKELY(o->m_state == STATE_COMMENT)) {
                    continue;
                }
                if (AMBRO_UNLIKELY(ch == ';')) {
                    if (o->m_state == STATE_INSIDE) {
                        finish_part(c);
                    }
                    o->m_state = STATE_COMMENT;
                    continue;
                }
            }
            
            if (AMBRO_UNLIKELY(o->m_state == STATE_OUTSIDE)) {
                if (AMBRO_LIKELY(!is_space(ch))) {
                    if (TheTypeHelper::EofEnabled) {
                        if (o->m_command.num_parts == 0 && ch == 'E') {
                            o->m_command.length++;
                            o->m_command.num_parts = ERROR_EOF;
                            o->m_state = STATE_NOCMD;
                            return true;
                        }
                    }
                    if (!is_code(ch)) {
                        o->m_command.num_parts = ERROR_INVALID_PART;
                    }
                    o->m_temp = o->m_command.length;
                    o->m_state = STATE_INSIDE;
                }
            } else {
                if (AMBRO_UNLIKELY(is_space(ch))) {
                    finish_part(c);
                    o->m_state = STATE_OUTSIDE;
                }
            }
        }
        
        return false;
    }
    
    static void resetCommand (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->m_state != STATE_NOCMD)
        
        o->m_state = STATE_NOCMD;
    }
    
    static BufferSizeType getLength (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->m_state == STATE_NOCMD)
        
        return o->m_command.length;
    }
    
    static PartsSizeType getNumParts (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->m_state == STATE_NOCMD)
        
        return o->m_command.num_parts;
    }
    
    static char getCmdCode (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->m_state == STATE_NOCMD)
        AMBRO_ASSERT(o->m_command.num_parts >= 0)
        
        return o->m_command.parts[0].code;
    }
    
    static uint16_t getCmdNumber (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->m_state == STATE_NOCMD)
        AMBRO_ASSERT(o->m_command.num_parts >= 0)
        
        return o->m_command.cmd_number;
    }
    
    static PartRef getPart (Context c, PartsSizeType i)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->m_state == STATE_NOCMD)
        AMBRO_ASSERT(o->m_command.num_parts >= 0)
        AMBRO_ASSERT(i >= 0)
        AMBRO_ASSERT(i < o->m_command.num_parts)
        
        return &o->m_command.parts[1 + i];
    }
    
    static char getPartCode (Context c, PartRef part)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->m_state == STATE_NOCMD)
        AMBRO_ASSERT(o->m_command.num_parts >= 0)
        
        return part->code;
    }
    
    template <typename FpType>
    static FpType getPartFpValue (Context c, PartRef part)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->m_state == STATE_NOCMD)
        AMBRO_ASSERT(o->m_command.num_parts >= 0)
        
        return StrToFloat<FpType>(part->data, NULL);
    }
    
    static uint32_t getPartUint32Value (Context c, PartRef part)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->m_state == STATE_NOCMD)
        AMBRO_ASSERT(o->m_command.num_parts >= 0)
        
        return strtoul(part->data, NULL, 10);
    }
    
    static char const * getPartStringValue (Context c, PartRef part)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->m_state == STATE_NOCMD)
        AMBRO_ASSERT(o->m_command.num_parts >= 0)
        
        return part->data;
    }
    
    static char * getBuffer (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->m_state != STATE_NOCMD)
        
        return o->m_buffer;
    }
    
    static Command * getCmd (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->m_state == STATE_NOCMD)
        
        return &o->m_command;
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
        
        static void init_command_hook (Context c)
        {
            auto *o = Object::self(c);
            o->m_checksum = 0;
            o->m_command.have_line_number = false;
        }
        
        static bool finish_part_hook (Context c, char code)
        {
            auto *o = Object::self(c);
            if (AMBRO_UNLIKELY(!o->m_command.have_line_number && o->m_command.num_parts == 0 && code == 'N')) {
                o->m_command.have_line_number = true;
                o->m_command.line_number = strtoul(o->m_buffer + (o->m_temp + 1), NULL, 10);
                return true;
            }
            return false;
        }
        
        static void checksum_add_hook (Context c, char ch)
        {
            auto *o = Object::self(c);
            o->m_checksum ^= (unsigned char)ch;
        }
        
        static void checksum_check_hook (Context c)
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(o->m_command.num_parts >= 0)
            AMBRO_ASSERT(o->m_state == STATE_CHECKSUM)
            
            char *received = o->m_buffer + (o->m_temp + 1);
            BufferSizeType received_len = o->m_command.length - (o->m_temp + 1);
            
            if (AMBRO_UNLIKELY(!compare_checksum(o->m_checksum, received, received_len))) {
                o->m_command.num_parts = ERROR_CHECKSUM;
            }
        }
    };
    
    template <typename Dummy>
    struct TypeHelper<GcodeParserTypeFile, Dummy> {
        static const bool ChecksumEnabled = false;
        static const bool CommentsEnabled = true;
        static const bool EofEnabled = true;
        
        static void init_command_hook (Context c)
        {
        }
        
        static bool finish_part_hook (Context c, char code)
        {
            return false;
        }
        
        static void checksum_add_hook (Context c, char ch)
        {
        }
        
        static void checksum_check_hook (Context c)
        {
        }
    };
    
    using TheTypeHelper = TypeHelper<ParserType>;
    
    static bool is_code (char ch)
    {
        return ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z'));
    }
    
    static bool is_space (char ch)
    {
        return (ch == ' ' || ch == '\t' || ch == '\r');
    }
    
    AMBRO_OPTIMIZE_SPEED
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
    
    AMBRO_OPTIMIZE_SPEED
    static void finish_part (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->m_command.num_parts >= 0)
        AMBRO_ASSERT(o->m_state == STATE_INSIDE)
        AMBRO_ASSERT(is_code(o->m_buffer[o->m_temp]))
        
        if (AMBRO_UNLIKELY(o->m_command.num_parts == Params::MaxParts)) {
            o->m_command.num_parts = ERROR_TOO_MANY_PARTS;
            return;
        }
        
        char code = o->m_buffer[o->m_temp];
        
        BufferSizeType in_pos = o->m_temp + 1;
        BufferSizeType out_pos = in_pos;
        
        while (in_pos < o->m_command.length) {
            char ch = o->m_buffer[in_pos++];
            if (ch == '\\' && o->m_command.num_parts > 0) {
                if (o->m_command.length - in_pos < 2) {
                    o->m_command.num_parts = ERROR_BAD_ESCAPE;
                    return;
                }
                int digit_h = read_hex_digit(o->m_buffer[in_pos++]);
                int digit_l = read_hex_digit(o->m_buffer[in_pos++]);
                if (digit_h < 0 || digit_l < 0) {
                    o->m_command.num_parts = ERROR_BAD_ESCAPE;
                    return;
                }
                unsigned char byte = (digit_h << 4) | digit_l;
                o->m_buffer[out_pos++] = *(char *)&byte;
            } else {
                o->m_buffer[out_pos++] = ch;
            }
        }
        
        o->m_buffer[out_pos] = '\0';
        
        if (TheTypeHelper::finish_part_hook(c, code)) {
            return;
        }
        
        o->m_command.parts[o->m_command.num_parts].code = code;
        o->m_command.parts[o->m_command.num_parts].data = o->m_buffer + (o->m_temp + 1);
        o->m_command.num_parts++;
    }
    
    static int read_hex_digit (char ch)
    {
        return
            (ch >= '0' && ch <= '9') ? (ch - '0') :
            (ch >= 'A' && ch <= 'F') ? (10 + (ch - 'A')) :
            (ch >= 'a' && ch <= 'f') ? (10 + (ch - 'a')) :
            -1;
    }
    
    template <typename TheParserType, typename Dummy = void>
    struct ExtraMembers {};
    
    template <typename Dummy>
    struct ExtraMembers<GcodeParserTypeSerial, Dummy> {
        uint8_t m_checksum;
    };
    
public:
    struct Object : public ObjBase<GcodeParser, ParentObject, MakeTypeList<TheDebugObject>>,
        public ExtraMembers<ParserType>
    {
        uint8_t m_state;
        char *m_buffer;
        BufferSizeType m_temp;
        Command m_command;
    };
};

template <typename Context, typename ParentObject, typename Params, typename TBufferSizeType>
using FileGcodeParser = GcodeParser<Context, ParentObject, Params, TBufferSizeType, GcodeParserTypeFile>;

#include <aprinter/EndNamespace.h>

#endif
