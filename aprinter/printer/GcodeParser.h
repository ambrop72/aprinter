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
#include <aprinter/meta/Position.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Likely.h>

#include <aprinter/BeginNamespace.h>

template <int TMaxParts>
struct GcodeParserParams {
    static const int MaxParts = TMaxParts;
};

struct GcodeParserTypeSerial {};
struct GcodeParserTypeFile {};

template <typename ParserType>
struct GcodeParserExtraMembers {};

template <typename Position, typename Context, typename Params, typename TBufferSizeType, typename ParserType>
class GcodeParser
: private DebugObject<Context, void>, private GcodeParserExtraMembers<ParserType>
{
    static_assert(Params::MaxParts > 0, "");
    
    static GcodeParser * self (Context c)
    {
        return PositionTraverse<typename Context::TheRootPosition, Position>(c.root());
    }
    
public:
    using BufferSizeType = TBufferSizeType;
    using PartsSizeType = typename ChooseInt<BitsInInt<Params::MaxParts>::value, true>::Type;
    
    enum {
        ERROR_NO_PARTS = -1, // must be -1
        ERROR_TOO_MANY_PARTS = -2,
        ERROR_INVALID_PART = -3,
        ERROR_CHECKSUM = -4,
        ERROR_RECV_OVERRUN = -5
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
        GcodeParser *o = self(c);
        o->m_state = STATE_NOCMD;
        
        o->debugInit(c);
    }
    
    static void deinit (Context c)
    {
        GcodeParser *o = self(c);
        o->debugDeinit(c);
    }
    
    static bool haveCommand (Context c)
    {
        GcodeParser *o = self(c);
        o->debugAccess(c);
        
        return (o->m_state != STATE_NOCMD);
    }
    
    static void startCommand (Context c, char *buffer, int8_t assume_error)
    {
        GcodeParser *o = self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(o->m_state == STATE_NOCMD)
        AMBRO_ASSERT(buffer)
        AMBRO_ASSERT(assume_error <= 0)
        
        o->m_state = STATE_OUTSIDE;
        o->m_buffer = buffer;
        o->m_command.length = 0;
        o->m_command.num_parts = assume_error;
        TheTypeHelper::init_command_hook(c);
    }
    
    static bool extendCommand (Context c, BufferSizeType avail)
    {
        GcodeParser *o = self(c);
        o->debugAccess(c);
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
                        o->m_command.num_parts--; // becomes ERROR_NO_PARTS if num_parts==0
                        if (o->m_command.num_parts >= 0) {
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
        GcodeParser *o = self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(o->m_state != STATE_NOCMD)
        
        o->m_state = STATE_NOCMD;
    }
    
    static char * getBuffer (Context c)
    {
        GcodeParser *o = self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(o->m_state != STATE_NOCMD)
        
        return o->m_buffer;
    }
    
    static Command * getCmd (Context c)
    {
        GcodeParser *o = self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(o->m_state == STATE_NOCMD)
        
        return &o->m_command;
    }
    
    static PartsSizeType getNumParts (Context c)
    {
        GcodeParser *o = self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(o->m_state == STATE_NOCMD)
        
        return o->m_command.num_parts;
    }
    
    static char getCmdCode (Context c)
    {
        GcodeParser *o = self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(o->m_state == STATE_NOCMD)
        AMBRO_ASSERT(o->m_command.num_parts >= 0)
        
        return o->m_command.parts[0].code;
    }
    
    static uint16_t getCmdNumber (Context c)
    {
        GcodeParser *o = self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(o->m_state == STATE_NOCMD)
        AMBRO_ASSERT(o->m_command.num_parts >= 0)
        
        return o->m_command.cmd_number;
    }
    
    static PartRef getPart (Context c, PartsSizeType i)
    {
        GcodeParser *o = self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(o->m_state == STATE_NOCMD)
        AMBRO_ASSERT(o->m_command.num_parts >= 0)
        AMBRO_ASSERT(i >= 0)
        AMBRO_ASSERT(i < o->m_command.num_parts)
        
        return &o->m_command.parts[1 + i];
    }
    
    static char getPartCode (Context c, PartRef part)
    {
        GcodeParser *o = self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(o->m_state == STATE_NOCMD)
        AMBRO_ASSERT(o->m_command.num_parts >= 0)
        
        return part->code;
    }
    
    static double getPartDoubleValue (Context c, PartRef part)
    {
        GcodeParser *o = self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(o->m_state == STATE_NOCMD)
        AMBRO_ASSERT(o->m_command.num_parts >= 0)
        
        return strtod(part->data, NULL);
    }
    
    static uint32_t getPartUint32Value (Context c, PartRef part)
    {
        GcodeParser *o = self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(o->m_state == STATE_NOCMD)
        AMBRO_ASSERT(o->m_command.num_parts >= 0)
        
        return strtoul(part->data, NULL, 10);
    }
    
private:
    enum {STATE_NOCMD, STATE_OUTSIDE, STATE_INSIDE, STATE_COMMENT, STATE_CHECKSUM};
    
    template <typename TheParserType, typename Dummy = void>
    struct TypeHelper;
    
    template <typename Dummy>
    struct TypeHelper<GcodeParserTypeSerial, Dummy> {
        static const bool ChecksumEnabled = true;
        static const bool CommentsEnabled = false;
        
        static void init_command_hook (Context c)
        {
            GcodeParser *o = self(c);
            o->m_checksum = 0;
            o->m_command.have_line_number = false;
        }
        
        static bool finish_part_hook (Context c, char code)
        {
            GcodeParser *o = self(c);
            if (AMBRO_UNLIKELY(!o->m_command.have_line_number && o->m_command.num_parts == 0 && code == 'N')) {
                o->m_command.have_line_number = true;
                o->m_command.line_number = strtoul(o->m_buffer + (o->m_temp + 1), NULL, 10);
                return true;
            }
            return false;
        }
        
        static void checksum_add_hook (Context c, char ch)
        {
            GcodeParser *o = self(c);
            o->m_checksum ^= (unsigned char)ch;
        }
        
        static void checksum_check_hook (Context c)
        {
            GcodeParser *o = self(c);
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
    
    static void finish_part (Context c)
    {
        GcodeParser *o = self(c);
        AMBRO_ASSERT(o->m_command.num_parts >= 0)
        AMBRO_ASSERT(o->m_state == STATE_INSIDE)
        AMBRO_ASSERT(is_code(o->m_buffer[o->m_temp]))
        
        if (AMBRO_UNLIKELY(o->m_command.num_parts == Params::MaxParts)) {
            o->m_command.num_parts = ERROR_TOO_MANY_PARTS;
            return;
        }
        
        char code = o->m_buffer[o->m_temp];
        
        o->m_buffer[o->m_command.length] = '\0';
        
        if (TheTypeHelper::finish_part_hook(c, code)) {
            return;
        }
        
        o->m_command.parts[o->m_command.num_parts].code = code;
        o->m_command.parts[o->m_command.num_parts].data = o->m_buffer + (o->m_temp + 1);
        o->m_command.num_parts++;
    }
    
    uint8_t m_state;
    char *m_buffer;
    BufferSizeType m_temp;
    Command m_command;
};

template <>
struct GcodeParserExtraMembers<GcodeParserTypeSerial> {
    uint8_t m_checksum;
};

#include <aprinter/EndNamespace.h>

#endif
