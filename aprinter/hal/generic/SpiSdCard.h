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
#include <stddef.h>

#include <aprinter/meta/WrapFunction.h>
#include <aprinter/meta/MinMax.h>
#include <aprinter/meta/BitsInInt.h>
#include <aprinter/meta/AliasStruct.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/Likely.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/BinaryTools.h>
#include <aprinter/base/TransferVector.h>
#include <aprinter/misc/CrcItuT.h>
#include <aprinter/misc/ClockUtils.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, typename InitHandler, typename CommandHandler, typename Params>
class SpiSdCard {
public:
    struct Object;
    
private:
    struct SpiHandler;
    
    static const int SpiMaxCommands = 3;
    static const int SpiCommandBits = BitsInInt<SpiMaxCommands>::Value;
    using TheDebugObject = DebugObject<Context, Object>;
    using TheSpi = typename Params::SpiService::template Spi<Context, Object, SpiHandler, SpiCommandBits>;
    using TheClockUtils = ClockUtils<Context>;
    using TimeType = typename TheClockUtils::TimeType;
    
    static TimeType const IdleStateTimeoutTicks = 1.2 * TheClockUtils::time_freq;
    static TimeType const InitTimeoutTicks = 1.2 * TheClockUtils::time_freq;
    static TimeType const WriteBusyTimeoutTicks = 5.0 * TheClockUtils::time_freq;
    
public:
    using BlockIndexType = uint32_t;
    static size_t const BlockSize = 512;
    using DataWordType = uint8_t;
    static size_t const MaxIoBlocks = 1;
    static int const MaxIoDescriptors = 1;
    
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
    
    static bool isWritable (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->m_state == STATE_RUNNING)
        
        // TBD: checks if card is actually writable
        return true;
    }
    
    static void startReadOrWrite (Context c, bool is_write, BlockIndexType block, size_t num_blocks, TransferVector<DataWordType> data_vector)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->m_state == STATE_RUNNING)
        AMBRO_ASSERT(o->m_io_state == IO_STATE_IDLE)
        AMBRO_ASSERT(block < o->m_capacity_blocks)
        AMBRO_ASSERT(num_blocks == 1)
        AMBRO_ASSERT(data_vector.num_descriptors == 1)
        AMBRO_ASSERT(data_vector.descriptors[0].num_words == BlockSize)
        
        o->m_request_buf = data_vector.descriptors[0].buffer_ptr;
        uint32_t addr = o->m_sdhc ? block : (block * 512);
        sd_command(c, (is_write ? CMD_WRITE_BLOCK : CMD_READ_SINGLE_BLOCK), addr, true, o->m_io_buf, o->m_io_buf);
        if (!is_write) {
            TheSpi::cmdReadUntilDifferent(c, 0xff, 255, 0xff, o->m_io_buf + 1);
        }
        o->m_io_state = is_write ? IO_STATE_WRITING_CMD : IO_STATE_READING_CMD;
    }
    
    using GetSpi = TheSpi;
    
private:
    using SsPin = typename Params::SsPin;
    
    enum {
        STATE_INACTIVE,
        STATE_INIT1,
        STATE_INIT2,
        STATE_INIT3,
        STATE_INIT3A,
        STATE_INIT4,
        STATE_INIT5,
        STATE_INIT5A,
        STATE_INIT6,
        STATE_INIT7,
        STATE_INIT8,
        STATE_RUNNING
    };
    
    enum {
        IO_STATE_IDLE,
        IO_STATE_READING_CMD, IO_STATE_READING_DATA,
        IO_STATE_WRITING_CMD, IO_STATE_WRITING_DATA, IO_STATE_WRITING_DATARESP, IO_STATE_WRITING_BUSY, IO_STATE_WRITING_STATUS
    };
    
    static const uint8_t CMD_GO_IDLE_STATE = 0;
    static const uint8_t CMD_SEND_IF_COND = 8;
    static const uint8_t CMD_SEND_CSD = 9;
    static const uint8_t CMD_SEND_STATUS = 13;
    static const uint8_t CMD_SET_BLOCKLEN = 16;
    static const uint8_t CMD_READ_SINGLE_BLOCK = 17;
    static const uint8_t CMD_WRITE_BLOCK = 24;
    static const uint8_t CMD_APP_CMD = 55;
    static const uint8_t CMD_READ_OCR = 58;
    static const uint8_t CMD_CRC_ON_OFF = 59;
    static const uint8_t ACMD_SD_SEND_OP_COND = 41;
    static const uint8_t R1_IN_IDLE_STATE = (1 << 0);
    static const uint8_t R1_ILLEGAL_COMMAND = (1 << 2);
    static const uint32_t OCR_CCS = (UINT32_C(1) << 30);
    static const uint32_t OCR_CPUS = (UINT32_C(1) << 31);
    static const uint32_t IfCondArgumentResponse = UINT32_C(0x1AA);
    
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
    }
    
    static void sd_receive_csd (Context c)
    {
        auto *o = Object::self(c);
        TheSpi::cmdWriteByte(c, 0xff, 5 - 1);
        TheSpi::cmdReadBuffer(c, o->m_buf2, 6, 0xff);
        TheSpi::cmdWriteByte(c, 0xff, 7 - 1);
    }
    
    static void spi_handler (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->m_state != STATE_INACTIVE)
        
        if (!TheSpi::endReached(c)) {
            return;
        }
        TheSpi::unsetEvent(c);
        
        switch (o->m_state) {
            case STATE_INIT1: {
                Context::Pins::template set<SsPin>(c, false);
                o->m_poll_timer.setAfter(c, IdleStateTimeoutTicks);
                sd_command(c, CMD_GO_IDLE_STATE, 0, true, o->m_buf1, o->m_buf1);
                o->m_state = STATE_INIT2;
            } break;
            
            case STATE_INIT2: {
                if (o->m_buf1[0] != R1_IN_IDLE_STATE) {
                    if (o->m_poll_timer.isExpired(c)) {
                        return error(c, 1);
                    }
                    sd_command(c, CMD_GO_IDLE_STATE, 0, true, o->m_buf1, o->m_buf1);
                    return;
                }
                sd_command(c, CMD_CRC_ON_OFF, 1, true, o->m_buf1, o->m_buf1);
                o->m_state = STATE_INIT3;
            } break;
            
            case STATE_INIT3: {
                if (o->m_buf1[0] != R1_IN_IDLE_STATE) {
                    return error(c, 11);
                }
                sd_command(c, CMD_SEND_IF_COND, IfCondArgumentResponse, true, o->m_buf1, o->m_buf1);
                TheSpi::cmdReadBuffer(c, o->m_buf1 + 1, 4, 0xff);
                o->m_state = STATE_INIT3A;
            } break;
            
            case STATE_INIT3A: {
                if ((o->m_buf1[0] & R1_ILLEGAL_COMMAND)) {
                    return error(c, 2);
                }
                uint32_t r7_response = ReadBinaryInt<uint32_t, BinaryBigEndian>((char *)(o->m_buf1 + 1));
                if ((r7_response & UINT32_C(0xFFF)) != IfCondArgumentResponse) {
                    return error(c, 13);
                }
                o->m_poll_timer.setAfter(c, InitTimeoutTicks);
                sd_command(c, CMD_APP_CMD, 0, true, o->m_buf2, o->m_buf2);
                o->m_state = STATE_INIT4;
            } break;
            
            case STATE_INIT4: {
                sd_command(c, ACMD_SD_SEND_OP_COND, UINT32_C(0x40000000), true, o->m_buf1, o->m_buf1);
                o->m_state = STATE_INIT5;
            } break;
            
            case STATE_INIT5: {
                if (o->m_buf2[0] != 0 || o->m_buf1[0] != 0) {
                    if (o->m_poll_timer.isExpired(c)) {
                        return error(c, 3);
                    }
                    sd_command(c, CMD_APP_CMD, 0, true, o->m_buf2, o->m_buf2);
                    o->m_state = STATE_INIT4;
                    return;
                }
                sd_command(c, CMD_READ_OCR, 0, true, o->m_buf1, o->m_buf1);
                TheSpi::cmdReadBuffer(c, o->m_buf1 + 1, 4, 0xff);
                o->m_state = STATE_INIT5A;
            } break;
            
            case STATE_INIT5A: {
                if (o->m_buf1[0] != 0) {
                    return error(c, 12);
                }
                uint32_t ocr = ReadBinaryInt<uint32_t, BinaryBigEndian>((char *)(o->m_buf1 + 1));
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
                sd_receive_csd(c);
                o->m_state = STATE_INIT8;
            } break;
            
            case STATE_INIT8: {
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
                o->m_io_state = IO_STATE_IDLE;
                return InitHandler::call(c, 0);
            } break;
            
            case STATE_RUNNING: {
                spi_for_io_completed(c);
            } break;
        }
    }
    
    static void spi_for_io_completed (Context c)
    {
        auto *o = Object::self(c);
        
        bool error = true;
        
        switch (o->m_io_state) {
            case IO_STATE_READING_CMD: {
                if (o->m_io_buf[0] != 0 || o->m_io_buf[1] != 0xfe) {
                    goto complete_request;
                }
                TheSpi::cmdReadBuffer(c, o->m_request_buf, BlockSize, 0xff);
                TheSpi::cmdReadBuffer(c, o->m_io_buf + 2, 2, 0xff);
                o->m_io_state = IO_STATE_READING_DATA;
                return;
            } break;
            
            case IO_STATE_READING_DATA: {
                uint16_t checksum_received = ReadBinaryInt<uint16_t, BinaryBigEndian>((char *)(o->m_io_buf + 2));
                uint16_t checksum_computed = CrcItuTUpdate(CrcItuTInitial, (char const *)o->m_request_buf, BlockSize);
                if (checksum_received != checksum_computed) {
                    goto complete_request;
                }
                error = false;
            } break;
            
            case IO_STATE_WRITING_CMD: {
                if (o->m_io_buf[0] != 0) {
                    goto complete_request;
                }
                TheSpi::cmdWriteBuffer(c, 0xfe, o->m_request_buf, BlockSize);
                uint16_t checksum = CrcItuTUpdate(CrcItuTInitial, (char const *)o->m_request_buf, BlockSize);
                WriteBinaryInt<uint16_t, BinaryBigEndian>(checksum, (char *)o->m_io_buf);
                TheSpi::cmdWriteBuffer(c, o->m_io_buf[0], o->m_io_buf + 1, 1);
                o->m_io_state = IO_STATE_WRITING_DATA;
                return;
            } break;
            
            case IO_STATE_WRITING_DATA: {
                TheSpi::cmdReadBuffer(c, o->m_io_buf + 2, 1, 0xff);
                o->m_io_state = IO_STATE_WRITING_DATARESP;
                return;
            } break;
            
            case IO_STATE_WRITING_DATARESP: {
                uint8_t data_response = o->m_io_buf[2];
                if ((data_response & 0x1F) != 5) {
                    goto complete_request;
                }
                TheSpi::cmdReadUntilDifferent(c, 0x00, 255, 0xff, o->m_io_buf);
                o->m_io_state = IO_STATE_WRITING_BUSY;
                o->m_poll_timer.setAfter(c, WriteBusyTimeoutTicks);
                return;
            } break;
            
            case IO_STATE_WRITING_BUSY: {
                if (o->m_io_buf[0] == 0x00) {
                    if (o->m_poll_timer.isExpired(c)) {
                        goto complete_request;
                    }
                    TheSpi::cmdReadUntilDifferent(c, 0x00, 255, 0xff, o->m_io_buf);
                    return;
                }
                sd_command(c, CMD_SEND_STATUS, 0, true, o->m_io_buf, o->m_io_buf);
                TheSpi::cmdReadBuffer(c, o->m_io_buf + 1, 1, 0xff);
                o->m_io_state = IO_STATE_WRITING_STATUS;
                return;
            } break;
            
            case IO_STATE_WRITING_STATUS: {
                if (o->m_io_buf[0] != 0 || o->m_io_buf[1] != 0) {
                    goto complete_request;
                }
                error = false;
            } break;
            
            default: AMBRO_ASSERT(false);
        }
    
    complete_request:
        o->m_io_state = IO_STATE_IDLE;
        return CommandHandler::call(c, error);
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
        uint8_t m_state : 4;
        uint8_t m_io_state : 3;
        bool m_sdhc : 1;
        typename TheClockUtils::PollTimer m_poll_timer;
        union {
            struct {
                uint8_t m_buf1[6];
                uint8_t m_buf2[6];
            };
            struct {
                uint32_t m_capacity_blocks;
                uint8_t m_io_buf[6];
                DataWordType *m_request_buf;
            };
        };
    };
};

APRINTER_ALIAS_STRUCT_EXT(SpiSdCardService, (
    APRINTER_AS_TYPE(SsPin),
    APRINTER_AS_TYPE(SpiService)
), (
    template <typename Context, typename ParentObject, typename InitHandler, typename CommandHandler>
    using SdCard = SpiSdCard<Context, ParentObject, InitHandler, CommandHandler, SpiSdCardService>;
))

#include <aprinter/EndNamespace.h>

#endif
