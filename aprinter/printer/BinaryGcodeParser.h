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

#include <aprinter/meta/BitsInInt.h>
#include <aprinter/meta/ChooseInt.h>
#include <aprinter/meta/Object.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Likely.h>

#include <aprinter/BeginNamespace.h>

template <int TMaxParts>
struct BinaryGcodeParserParams {
    static const int MaxParts = TMaxParts;
};

template <typename Context, typename ParentObject, typename Params, typename TBufferSizeType>
class BinaryGcodeParser {
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
    struct Object;
    using BufferSizeType = TBufferSizeType;
    using PartsSizeType = typename ChooseInt<BitsInInt<Params::MaxParts>::value, true>::Type;
    
private:
    struct Part {
        uint8_t data_type;
        char code;
        uint8_t data_size;
        uint8_t *data;
    };
    
public:
    enum {
        ERROR_NO_PARTS = -1,
        ERROR_TOO_MANY_PARTS = -2,
        ERROR_INVALID_PART = -3,
        ERROR_CHECKSUM = -4,
        ERROR_RECV_OVERRUN = -5,
        ERROR_EOF = -6
    };
    
    using PartRef = Part *;
    
    static void init (Context c)
    {
        auto *o = Object::self(c);
        o->m_state = STATE_NOCMD;
        
        o->debugInit(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        o->debugDeinit(c);
    }
    
    static bool haveCommand (Context c)
    {
        auto *o = Object::self(c);
        o->debugAccess(c);
        
        return (o->m_state != STATE_NOCMD);
    }
    
    static void startCommand (Context c, char *buffer, int8_t assume_error)
    {
        auto *o = Object::self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(o->m_state == STATE_NOCMD)
        AMBRO_ASSERT(buffer)
        AMBRO_ASSERT(assume_error <= 0)
        
        o->m_state = STATE_HEADER;
        o->m_buffer = (uint8_t *)buffer;
        o->m_length = 0;
        o->m_num_parts = assume_error;
    }
    
    static bool extendCommand (Context c, BufferSizeType avail)
    {
        auto *o = Object::self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(o->m_state != STATE_NOCMD)
        AMBRO_ASSERT(avail >= o->m_length)
        
        while (1) {
            switch (o->m_state) {
                case STATE_HEADER: {
                    AMBRO_ASSERT(o->m_length == 0)
                    if (o->m_num_parts < 0) {
                        goto finish;
                    }
                    if (avail < 1) {
                        return false;
                    }
                    o->m_length = 1;
                    o->m_num_parts = o->m_buffer[0] & 0x0f;
                    if (o->m_num_parts > Params::MaxParts) {
                        o->m_num_parts = ERROR_TOO_MANY_PARTS;
                        goto finish;
                    }
                    o->m_state = STATE_INDEX;
                    switch (o->m_buffer[0] >> 4) {
                        case CMD_TYPE_G0: {
                            o->m_cmd_code = 'G';
                            o->m_cmd_num = 0;
                        } break;
                        case CMD_TYPE_G1: {
                            o->m_cmd_code = 'G';
                            o->m_cmd_num = 1;
                        } break;
                        case CMD_TYPE_G92: {
                            o->m_cmd_code = 'G';
                            o->m_cmd_num = 92;
                        } break;
                        case CMD_TYPE_EOF: {
                            o->m_num_parts = ERROR_EOF;
                            goto finish;
                        } break;
                        case CMD_TYPE_LONG: {
                            o->m_state = STATE_HEADER_LONG;
                        } break;
                    }
                } break;
                
                case STATE_HEADER_LONG: {
                    AMBRO_ASSERT(o->m_length == 1)
                    if (avail < 3) {
                        return false;
                    }
                    o->m_length = 3;
                    o->m_cmd_code = 'A' + (o->m_buffer[1] >> 3);
                    o->m_cmd_num = ((uint16_t)(o->m_buffer[1] & 0x7) << 8) | o->m_buffer[2];
                    o->m_state = STATE_INDEX;
                } break;
                
                case STATE_INDEX: {
                    AMBRO_ASSERT(o->m_length == 1 || o->m_length == 3)
                    if (avail - o->m_length < o->m_num_parts) {
                        return false;
                    }
                    BufferSizeType index_offset = o->m_length;
                    o->m_length += o->m_num_parts;
                    o->m_total_size = o->m_length;
                    for (PartsSizeType i = 0; i < o->m_num_parts; i++) {
                        uint8_t index_byte = o->m_buffer[index_offset + i];
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
                                o->m_num_parts = ERROR_INVALID_PART;
                                goto finish;
                        }
                        o->m_parts[i].data_type = index_byte >> 5;
                        o->m_parts[i].code = 'A' + (index_byte & 0x1f);
                        o->m_parts[i].data_size = data_size;
                        o->m_total_size += data_size;
                    }
                    o->m_state = STATE_PAYLOAD;
                } break;
                
                case STATE_PAYLOAD: {
                    if (avail < o->m_total_size) {
                        return false;
                    }
                    BufferSizeType offset = o->m_length;
                    for (PartsSizeType i = 0; i < o->m_num_parts; i++) {
                        o->m_parts[i].data = o->m_buffer + offset;
                        offset += o->m_parts[i].data_size;
                    }
                    o->m_length = o->m_total_size;
                    goto finish;
                } break;
            }
        }
        
    finish:
        o->m_state = STATE_NOCMD;
        return true;
    }
    
    static void resetCommand (Context c)
    {
        auto *o = Object::self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(o->m_state != STATE_NOCMD)
        
        o->m_state = STATE_NOCMD;
    }
    
    static BufferSizeType getLength (Context c)
    {
        auto *o = Object::self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(o->m_state == STATE_NOCMD)
        
        return o->m_length;
    }
    
    static PartsSizeType getNumParts (Context c)
    {
        auto *o = Object::self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(o->m_state == STATE_NOCMD)
        
        return o->m_num_parts;
    }
    
    static char getCmdCode (Context c)
    {
        auto *o = Object::self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(o->m_state == STATE_NOCMD)
        AMBRO_ASSERT(o->m_num_parts >= 0)
        
        return o->m_cmd_code;
    }
    
    static uint16_t getCmdNumber (Context c)
    {
        auto *o = Object::self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(o->m_state == STATE_NOCMD)
        AMBRO_ASSERT(o->m_num_parts >= 0)
        
        return o->m_cmd_num;
    }
    
    static PartRef getPart (Context c, PartsSizeType i)
    {
        auto *o = Object::self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(o->m_state == STATE_NOCMD)
        AMBRO_ASSERT(o->m_num_parts >= 0)
        AMBRO_ASSERT(i >= 0)
        AMBRO_ASSERT(i < o->m_num_parts)
        
        return &o->m_parts[i];
    }
    
    static char getPartCode (Context c, PartRef part)
    {
        auto *o = Object::self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(o->m_state == STATE_NOCMD)
        AMBRO_ASSERT(o->m_num_parts >= 0)
        
        return part->code;
    }
    
    template <typename FpType>
    static FpType getPartFpValue (Context c, PartRef part)
    {
        auto *o = Object::self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(o->m_state == STATE_NOCMD)
        AMBRO_ASSERT(o->m_num_parts >= 0)
        
        switch (part->data_type) {
            case DATA_TYPE_FLOAT: {
                float val;
                static_assert(sizeof(val) == 4, "");
                memcpy(&val, part->data, sizeof(val));
                return val;
            } break;
            
            case DATA_TYPE_UINT32: {
                uint32_t val;
                static_assert(sizeof(val) == 4, "");
                memcpy(&val, part->data, sizeof(val));
                return val;
            } break;
            
            default:
                return 0.0f;
        }
    }
    
    static uint32_t getPartUint32Value (Context c, PartRef part)
    {
        auto *o = Object::self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(o->m_state == STATE_NOCMD)
        AMBRO_ASSERT(o->m_num_parts >= 0)
        
        switch (part->data_type) {
            case DATA_TYPE_UINT32: {
                uint32_t val;
                static_assert(sizeof(val) == 4, "");
                memcpy(&val, part->data, sizeof(val));
                return val;
            } break;
            
            default:
                return 0;
        }
    }
    
private:
    enum {STATE_NOCMD, STATE_HEADER, STATE_HEADER_LONG, STATE_INDEX, STATE_PAYLOAD};
    
public:
    struct Object : public ObjBase<BinaryGcodeParser, ParentObject, EmptyTypeList>,
        public DebugObject<Context, void>
    {
        uint8_t m_state;
        uint8_t *m_buffer;
        BufferSizeType m_length;
        uint8_t m_cmd_code;
        uint16_t m_cmd_num;
        PartsSizeType m_num_parts;
        BufferSizeType m_total_size;
        Part m_parts[Params::MaxParts];
    };
};

#include <aprinter/EndNamespace.h>

#endif
