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

#ifndef AMBROLIB_BINARY_GCODE_PARSER_H
#define AMBROLIB_BINARY_GCODE_PARSER_H

#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include <aprinter/meta/AliasStruct.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Likely.h>
#include <aprinter/printer/GcodeCommand.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename TBufferSizeType, typename FpType, typename Params>
class BinaryGcodeParser
: public GcodeCommand<Context, FpType>,
  private SimpleDebugObject<Context>
{
    static_assert(Params::MaxParts <= 14, "");
    
    enum {
        CMD_TYPE_G0 = 1,
        CMD_TYPE_G1 = 2,
        CMD_TYPE_G92 = 3,
        CMD_TYPE_EOF = 14,
        CMD_TYPE_LONG = 15,
    };
    
    enum {
        DATA_TYPE_FLOAT = 1,
        DATA_TYPE_DOUBLE = 2,
        DATA_TYPE_UINT32 = 3,
        DATA_TYPE_UINT64 = 4,
        DATA_TYPE_VOID = 5
    };
    
public:
    using BufferSizeType = TBufferSizeType;
    using PartsSizeType = int8_t;
    using TheGcodeCommand = GcodeCommand<Context, FpType>;
    using PartRef = typename TheGcodeCommand::PartRef;
    
private:
    struct Part {
        uint8_t data_type;
        char code;
        uint8_t data_size;
        uint8_t *data;
    };
    
public:
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
        
        m_state = STATE_HEADER;
        m_buffer = (uint8_t *)buffer;
        m_length = 0;
        m_num_parts = assume_error;
    }
    
    bool extendCommand (Context c, BufferSizeType avail, bool line_buffer_exhausted=false)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_state != STATE_NOCMD)
        AMBRO_ASSERT(avail >= m_length)
        
        while (1) {
            switch (m_state) {
                case STATE_HEADER: {
                    AMBRO_ASSERT(m_length == 0)
                    if (m_num_parts < 0) {
                        goto finish;
                    }
                    if (avail < 1) {
                        return false;
                    }
                    m_length = 1;
                    m_num_parts = m_buffer[0] & 0x0f;
                    if (m_num_parts > Params::MaxParts) {
                        m_num_parts = GCODE_ERROR_TOO_MANY_PARTS;
                        goto finish;
                    }
                    m_state = STATE_INDEX;
                    switch (m_buffer[0] >> 4) {
                        case CMD_TYPE_G0: {
                            m_cmd_code = 'G';
                            m_cmd_num = 0;
                        } break;
                        case CMD_TYPE_G1: {
                            m_cmd_code = 'G';
                            m_cmd_num = 1;
                        } break;
                        case CMD_TYPE_G92: {
                            m_cmd_code = 'G';
                            m_cmd_num = 92;
                        } break;
                        case CMD_TYPE_EOF: {
                            m_num_parts = GCODE_ERROR_EOF;
                            goto finish;
                        } break;
                        case CMD_TYPE_LONG: {
                            m_state = STATE_HEADER_LONG;
                        } break;
                    }
                } break;
                
                case STATE_HEADER_LONG: {
                    AMBRO_ASSERT(m_length == 1)
                    if (avail < 3) {
                        return false;
                    }
                    m_length = 3;
                    m_cmd_code = 'A' + (m_buffer[1] >> 3);
                    m_cmd_num = ((uint16_t)(m_buffer[1] & 0x7) << 8) | m_buffer[2];
                    m_state = STATE_INDEX;
                } break;
                
                case STATE_INDEX: {
                    AMBRO_ASSERT(m_length == 1 || m_length == 3)
                    if (avail - m_length < m_num_parts) {
                        return false;
                    }
                    BufferSizeType index_offset = m_length;
                    m_length += m_num_parts;
                    m_total_size = m_length;
                    for (PartsSizeType i = 0; i < m_num_parts; i++) {
                        uint8_t index_byte = m_buffer[index_offset + i];
                        BufferSizeType data_size;
                        switch (index_byte >> 5) {
                            case DATA_TYPE_FLOAT:
                            case DATA_TYPE_UINT32:
                                data_size = 4;
                                break;
                            case DATA_TYPE_VOID:
                                data_size = 0;
                                break;
                            default:
                                m_num_parts = GCODE_ERROR_INVALID_PART;
                                goto finish;
                        }
                        m_parts[i].data_type = index_byte >> 5;
                        m_parts[i].code = 'A' + (index_byte & 0x1f);
                        m_parts[i].data_size = data_size;
                        m_total_size += data_size;
                    }
                    m_state = STATE_PAYLOAD;
                } break;
                
                case STATE_PAYLOAD: {
                    if (avail < m_total_size) {
                        return false;
                    }
                    BufferSizeType offset = m_length;
                    for (PartsSizeType i = 0; i < m_num_parts; i++) {
                        m_parts[i].data = m_buffer + offset;
                        offset += m_parts[i].data_size;
                    }
                    m_length = m_total_size;
                    goto finish;
                } break;
            }
        }
        
    finish:
        m_state = STATE_NOCMD;
        return true;
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
        
        return m_length;
    }
    
    PartsSizeType getNumParts (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_state == STATE_NOCMD)
        
        return m_num_parts;
    }
    
    char getCmdCode (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_state == STATE_NOCMD)
        AMBRO_ASSERT(m_num_parts >= 0)
        
        return m_cmd_code;
    }
    
    uint16_t getCmdNumber (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_state == STATE_NOCMD)
        AMBRO_ASSERT(m_num_parts >= 0)
        
        return m_cmd_num;
    }
    
    PartRef getPart (Context c, PartsSizeType i)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_state == STATE_NOCMD)
        AMBRO_ASSERT(m_num_parts >= 0)
        AMBRO_ASSERT(i >= 0)
        AMBRO_ASSERT(i < m_num_parts)
        
        return PartRef{&m_parts[i]};
    }
    
    char getPartCode (Context c, PartRef part)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_state == STATE_NOCMD)
        AMBRO_ASSERT(m_num_parts >= 0)
        
        return cast_part_ref(part)->code;
    }
    
    FpType getPartFpValue (Context c, PartRef part)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_state == STATE_NOCMD)
        AMBRO_ASSERT(m_num_parts >= 0)
        
        switch (cast_part_ref(part)->data_type) {
            case DATA_TYPE_FLOAT: {
                float val;
                static_assert(sizeof(val) == 4, "");
                memcpy(&val, cast_part_ref(part)->data, sizeof(val));
                return val;
            } break;
            
            case DATA_TYPE_UINT32: {
                uint32_t val;
                static_assert(sizeof(val) == 4, "");
                memcpy(&val, cast_part_ref(part)->data, sizeof(val));
                return val;
            } break;
            
            default:
                return 0.0f;
        }
    }
    
    uint32_t getPartUint32Value (Context c, PartRef part)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_state == STATE_NOCMD)
        AMBRO_ASSERT(m_num_parts >= 0)
        
        switch (cast_part_ref(part)->data_type) {
            case DATA_TYPE_UINT32: {
                uint32_t val;
                static_assert(sizeof(val) == 4, "");
                memcpy(&val, cast_part_ref(part)->data, sizeof(val));
                return val;
            } break;
            
            default:
                return 0;
        }
    }
    
    char const * getPartStringValue (Context c, PartRef part)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_state == STATE_NOCMD)
        AMBRO_ASSERT(m_num_parts >= 0)
        
        return nullptr;
    }
    
private:
    enum {STATE_NOCMD, STATE_HEADER, STATE_HEADER_LONG, STATE_INDEX, STATE_PAYLOAD};
    
    static Part * cast_part_ref (PartRef part_ref)
    {
        return (Part *)part_ref.ptr;
    }
    
    uint8_t m_state;
    uint8_t *m_buffer;
    BufferSizeType m_length;
    uint8_t m_cmd_code;
    uint16_t m_cmd_num;
    PartsSizeType m_num_parts;
    BufferSizeType m_total_size;
    Part m_parts[Params::MaxParts];
};

APRINTER_ALIAS_STRUCT_EXT(BinaryGcodeParserService, (
    APRINTER_AS_VALUE(int, MaxParts)
), (
    template <typename Context, typename TBufferSizeType, typename FpType>
    using Parser = BinaryGcodeParser<Context, TBufferSizeType, FpType, BinaryGcodeParserService>;
))

#include <aprinter/EndNamespace.h>

#endif
