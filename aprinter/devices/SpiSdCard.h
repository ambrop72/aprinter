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

#ifndef AMBROLIB_SPI_SDCARD_H
#define AMBROLIB_SPI_SDCARD_H

#include <stdint.h>

#include <aprinter/base/Object.h>
#include <aprinter/meta/WrapFunction.h>
#include <aprinter/meta/MinMax.h>
#include <aprinter/meta/BitsInInt.h>
#include <aprinter/base/Likely.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, int MaxCommands, typename InitHandler, typename CommandHandler, typename Params>
class SpiSdCard {
public:
    struct Object;
    
private:
    struct SpiHandler;
    
    static const int SpiMaxCommands = MaxValue(6, 6 * MaxCommands);
    static const int SpiCommandBits = BitsInInt<SpiMaxCommands>::Value;
    using TheDebugObject = DebugObject<Context, Object>;
    using TheSpi = typename Params::SpiService::template Spi<Context, Object, SpiHandler, SpiCommandBits>;
    using SpiCommandSizeType = typename TheSpi::CommandSizeType;
    
public:
    using BlockIndexType = uint32_t;
    static size_t const BlockSize = 512;
    
    class ReadState {
        friend SpiSdCard;
        uint8_t buf[6];
        SpiCommandSizeType spi_end_index;
    };
    
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        o->m_state = STATE_INACTIVE;
        Context::Pins::template set<SsPin>(c, true);
        Context::Pins::template setOutput<SsPin>(c);
        
        TheDebugObject::init(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::deinit(c);
        
        Context::Pins::template set<SsPin>(c, true);
        if (o->m_state != STATE_INACTIVE) {
            TheSpi::deinit(c);
        }
    }
    
    static void activate (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->m_state == STATE_INACTIVE)
        
        TheSpi::init(c);
        TheSpi::cmdWriteByte(c, 0xff, 128 - 1);
        o->m_state = STATE_INIT1;
    }
    
    static void deactivate (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->m_state != STATE_INACTIVE)
        
        deactivate_common(c);
    }
    
    static BlockIndexType getCapacityBlocks (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->m_state == STATE_RUNNING)
        AMBRO_ASSERT(o->m_capacity_blocks > 0)
        
        return o->m_capacity_blocks;
    }
    
    static void queueReadBlock (Context c, BlockIndexType block, uint8_t *data1, size_t data1_len, uint8_t *data2, ReadState *state)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->m_state == STATE_RUNNING)
        AMBRO_ASSERT(block < o->m_capacity_blocks)
        AMBRO_ASSERT(data1_len > 0)
        AMBRO_ASSERT(data1_len <= 512)
        
        uint32_t addr = o->m_sdhc ? block : (block * 512);
        sd_command(c, CMD_READ_SINGLE_BLOCK, addr, true, state->buf, state->buf);
        TheSpi::cmdReadUntilDifferent(c, 0xff, 255, 0xff, state->buf + 1);
        TheSpi::cmdReadBuffer(c, data1, data1_len, 0xff);
        if (data1_len < 512) {
            TheSpi::cmdReadBuffer(c, data2, 512 - data1_len, 0xff);
        }
        TheSpi::cmdWriteByte(c, 0xff, 2 - 1);
        state->spi_end_index = TheSpi::getEndIndex(c);
    }
    
    static bool checkReadBlock (Context c, ReadState *state, bool *out_error)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->m_state == STATE_RUNNING)
        
        if (!TheSpi::indexReached(c, state->spi_end_index)) {
            return false;
        }
        *out_error = (state->buf[0] != 0 || state->buf[1] != 0xfe);
        return true;
    }
    
    static void unsetEvent (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->m_state == STATE_RUNNING)
        
        TheSpi::unsetEvent(c);
    }
    
    using GetSpi = TheSpi;
    
private:
    using SsPin = typename Params::SsPin;
    
    enum {
        STATE_INACTIVE,
        STATE_INIT1,
        STATE_INIT2,
        STATE_INIT3,
        STATE_INIT4,
        STATE_INIT5,
        STATE_INIT6,
        STATE_INIT7,
        STATE_INIT8,
        STATE_RUNNING
    };
    
    static const uint8_t CMD_GO_IDLE_STATE = 0;
    static const uint8_t CMD_SEND_IF_COND = 8;
    static const uint8_t CMD_SEND_CSD = 9;
    static const uint8_t CMD_SET_BLOCKLEN = 16;
    static const uint8_t CMD_READ_SINGLE_BLOCK = 17;
    static const uint8_t CMD_APP_CMD = 55;
    static const uint8_t CMD_READ_OCR = 58;
    static const uint8_t ACMD_SD_SEND_OP_COND = 41;
    static const uint8_t R1_IN_IDLE_STATE = (1 << 0);
    static const uint32_t OCR_CCS = (UINT32_C(1) << 30);
    static const uint32_t OCR_CPUS = (UINT32_C(1) << 31);
    
    static uint8_t crc7 (uint8_t const *data, uint8_t count, uint8_t crc)
    { 
        for (uint8_t a = 0; a < count; a++) {
            uint8_t byte = data[a];
            for (uint8_t i = 0; i < 8; i++) {
                crc <<= 1;
                if ((byte ^ crc) & 0x80) {
                    crc ^= 0x09;
                }
                byte <<= 1;
            }
        }
        return (crc & 0x7f);
    }
    
    static void sd_command (Context c, uint8_t cmd, uint32_t param, bool checksum, uint8_t *request_buf, uint8_t *response_buf)
    {
        auto *o = Object::self(c);
        
        request_buf[0] = cmd | 0x40;
        request_buf[1] = param >> 24;
        request_buf[2] = param >> 16;
        request_buf[3] = param >> 8;
        request_buf[4] = param;
        request_buf[5] = 1;
        if (checksum) {
            request_buf[5] |= crc7(request_buf, 5, 0) << 1;
        }
        TheSpi::cmdWriteBuffer(c, 0xff, request_buf, 6);
        TheSpi::cmdReadUntilDifferent(c, 0xff, 255, 0xff, response_buf);
    }
    
    static void sd_send_csd (Context c)
    {
        auto *o = Object::self(c);
        sd_command(c, CMD_SEND_CSD, 0, true, o->m_buf1, o->m_buf1);
        TheSpi::cmdReadUntilDifferent(c, 0xff, 255, 0xff, o->m_buf1 + 1);
        TheSpi::cmdWriteByte(c, 0xff, 5 - 1);
        TheSpi::cmdReadBuffer(c, o->m_buf2, 6, 0xff);
        TheSpi::cmdWriteByte(c, 0xff, 7 - 1);
    }
    
    static void spi_handler (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->m_state != STATE_INACTIVE)
        
        if (AMBRO_LIKELY(o->m_state == STATE_RUNNING)) {
            return CommandHandler::call(c);
        }
        if (!TheSpi::endReached(c)) {
            return;
        }
        switch (o->m_state) {
            case STATE_INIT1: {
                Context::Pins::template set<SsPin>(c, false);
                sd_command(c, CMD_GO_IDLE_STATE, 0, true, o->m_buf1, o->m_buf1);
                o->m_state = STATE_INIT2;
                o->m_count = 255;
            } break;
            case STATE_INIT2: {
                if (o->m_buf1[0] != R1_IN_IDLE_STATE) {
                    o->m_count--;
                    if (o->m_count == 0) {
                        return error(c, 1);
                    }
                    sd_command(c, CMD_GO_IDLE_STATE, 0, true, o->m_buf1, o->m_buf1);
                    return;
                }
                sd_command(c, CMD_SEND_IF_COND, UINT32_C(0x1AA), true, o->m_buf1, o->m_buf1);
                o->m_state = STATE_INIT3;
            } break;
            case STATE_INIT3: {
                if (o->m_buf1[0] != 1) {
                    return error(c, 2);
                }
                sd_command(c, CMD_APP_CMD, 0, true, o->m_buf2, o->m_buf2);
                sd_command(c, ACMD_SD_SEND_OP_COND, UINT32_C(0x40000000), true, o->m_buf1, o->m_buf1);
                o->m_state = STATE_INIT4;
                o->m_count = 255;
            } break;
            case STATE_INIT4: {
                if (o->m_buf2[0] != 0 || o->m_buf1[0] != 0) {
                    o->m_count--;
                    if (o->m_count == 0) {
                        return error(c, 3);
                    }
                    sd_command(c, CMD_APP_CMD, 0, true, o->m_buf2, o->m_buf2);
                    sd_command(c, ACMD_SD_SEND_OP_COND, UINT32_C(0x40000000), true, o->m_buf1, o->m_buf1);
                    return;
                }
                sd_command(c, CMD_READ_OCR, 0, true, o->m_buf1, o->m_buf1);
                TheSpi::cmdReadBuffer(c, o->m_buf1 + 1, 4, 0xff);
                o->m_state = STATE_INIT5;
            } break;
            case STATE_INIT5: {
                if (o->m_buf1[0] != 0) {
                    return error(c, 4);
                }
                uint32_t ocr = ((uint32_t)o->m_buf1[1] << 24) | ((uint32_t)o->m_buf1[2] << 16) | ((uint32_t)o->m_buf1[3] << 8) | ((uint32_t)o->m_buf1[4] << 0);
                if (!(ocr & OCR_CPUS)) {
                    return error(c, 5);
                }
                o->m_sdhc = ocr & OCR_CCS;
                if (!o->m_sdhc) {
                    sd_command(c, CMD_SET_BLOCKLEN, 512, true, o->m_buf1, o->m_buf1);
                    o->m_state = STATE_INIT6;
                } else {
                    sd_send_csd(c);
                    o->m_state = STATE_INIT7;
                }
            } break;
            case STATE_INIT6: {
                if (o->m_buf1[0] != 0) {
                    return error(c, 6);
                }
                sd_send_csd(c);
                o->m_state = STATE_INIT7;
            } break;
            case STATE_INIT7: {
                if (o->m_buf1[0] != 0) {
                    return error(c, 7);
                }
                if (o->m_buf1[1] != 0xfe) {
                    return error(c, 8);
                }
                if (o->m_sdhc) {
                    uint32_t c_size = o->m_buf2[4] | ((uint32_t)o->m_buf2[3] << 8) | ((uint32_t)(o->m_buf2[2] & 0x3f) << 16);
                    if (c_size >= UINT32_C(0x3fffff)) {
                        return error(c, 10);
                    }
                    o->m_capacity_blocks = (c_size + 1) * UINT32_C(1024);
                } else {
                    uint8_t read_bl_len = o->m_buf2[0] & 0xf;
                    uint8_t c_size_mult = (o->m_buf2[5] >> 7) | ((o->m_buf2[4] & 0x3) << 1);
                    uint16_t c_size = (o->m_buf2[3] >> 6) | ((uint16_t)o->m_buf2[2] << 2) | ((uint16_t)(o->m_buf2[1] & 0x3) << 10);
                    uint16_t mult = ((uint16_t)1 << (c_size_mult + 2));
                    uint32_t blocknr = (uint32_t)(c_size + 1) * mult;
                    uint16_t block_len = (uint16_t)1 << read_bl_len;
                    o->m_capacity_blocks = blocknr * (block_len / 512);
                }
                if (o->m_capacity_blocks == 0) {
                    return error(c, 9);
                }
                o->m_state = STATE_RUNNING;
                return InitHandler::call(c, 0);
            } break;
        }
    }
    
    static void deactivate_common (Context c)
    {
        auto *o = Object::self(c);
        
        Context::Pins::template set<SsPin>(c, true);
        TheSpi::deinit(c);
        o->m_state = STATE_INACTIVE;
    }
    
    static void error (Context c, uint8_t code)
    {
        deactivate_common(c);
        return InitHandler::call(c, code);
    }
    
    struct SpiHandler : public AMBRO_WFUNC_TD(&SpiSdCard::spi_handler) {};
    
public:
    struct Object : public ObjBase<SpiSdCard, ParentObject, MakeTypeList<
        TheDebugObject,
        TheSpi
    >> {
        uint8_t m_state;
        union {
            uint8_t m_count;
            bool m_sdhc;
        };
        union {
            struct {
                uint8_t m_buf1[6];
                uint8_t m_buf2[6];
            };
            uint32_t m_capacity_blocks;
        };
    };
};

template <
    typename TSsPin,
    typename TSpiService
>
struct SpiSdCardService {
    using SsPin = TSsPin;
    using SpiService = TSpiService;
    
    template <typename Context, typename ParentObject, int MaxCommands, typename InitHandler, typename CommandHandler>
    using SdCard = SpiSdCard<Context, ParentObject, MaxCommands, InitHandler, CommandHandler, SpiSdCardService>;
};

#include <aprinter/EndNamespace.h>

#endif
