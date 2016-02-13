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

#ifndef APRINTER_SDIO_SDCARD_H
#define APRINTER_SDIO_SDCARD_H

#include <stddef.h>
#include <stdint.h>

#include <aprinter/meta/WrapFunction.h>
#include <aprinter/meta/WrapValue.h>
#include <aprinter/meta/AliasStruct.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Callback.h>
#include <aprinter/hal/generic/SdioInterface.h>
#include <aprinter/misc/ClockUtils.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, typename InitHandler, typename CommandHandler, typename Params>
class SdioSdCard {
public:
    struct Object;
    
private:
    using TheDebugObject = DebugObject<Context, Object>;
    struct SdioCommandHandler;
    using TheSdio = typename Params::SdioService::template Sdio<Context, Object, SdioCommandHandler>;
    using TheClockUtils = ClockUtils<Context>;
    using TimeType = typename TheClockUtils::TimeType;
    
    enum {
        STATE_INACTIVE, STATE_POWERON, STATE_POWER_CLOCKS, STATE_GO_IDLE, STATE_IF_COND,
        STATE_OP_COND_APP, STATE_OP_COND, STATE_SEND_CID, STATE_RELATIVE_ADDR,
        STATE_SEND_CSD, STATE_SELECT, STATE_WIDEBUS_APP, STATE_WIDEBUS,
        STATE_RUNNING
    };
    
    enum {IO_STATE_IDLE, IO_STATE_READING, IO_STATE_WRITING};
    
    static TimeType const PowerOnTimeTicks = 0.0015 * TheClockUtils::time_freq;
    static TimeType const PowerClocksTimeTicks = 0.001 * TheClockUtils::time_freq;
    static TimeType const InitTimeoutTicks = 1.2 * TheClockUtils::time_freq;
    static TimeType const ProgrammingTimeoutTicks = 3.0 * TheClockUtils::time_freq;
    static uint32_t const IfCondArgumentResponse = UINT32_C(0x1AA);
    static uint32_t const OpCondArgument = UINT32_C(0x40100000);
    
    static const uint8_t CMD_GO_IDLE_STATE = 0;
    static const uint8_t CMD_ALL_SEND_CID = 2;
    static const uint8_t CMD_SEND_RELATIVE_ADDR = 3;
    static const uint8_t CMD_SELECT_DESELECT = 7;
    static const uint8_t CMD_SEND_IF_COND = 8;
    static const uint8_t CMD_SEND_CSD = 9;
    static const uint8_t CMD_SEND_STATUS = 13;
    static const uint8_t CMD_READ_SINGLE_BLOCK = 17;
    static const uint8_t CMD_WRITE_BLOCK = 24;
    static const uint8_t CMD_APP_CMD = 55;
    static const uint8_t ACMD_SET_BUS_WIDTH = 6;
    static const uint8_t ACMD_SD_SEND_OP_COND = 41;
    
    static const uint32_t OCR_CCS = (UINT32_C(1) << 30);
    static const uint32_t OCR_CPUS = (UINT32_C(1) << 31);
    
public:
    using BlockIndexType = uint32_t;
    static size_t const BlockSize = TheSdio::BlockSize;
    using DataWordType = uint32_t;
    
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        TheSdio::init(c);
        o->timer.init(c, APRINTER_CB_STATFUNC_T(&SdioSdCard::timer_handler));
        o->state = STATE_INACTIVE;
        
        TheDebugObject::init(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::deinit(c);
        
        o->timer.deinit(c);
        TheSdio::deinit(c);
    }
    
    static void activate (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == STATE_INACTIVE)
        
        TheSdio::startPowerOn(c, SdioIface::InterfaceParams{false, false});
        TimeType timer_time = Context::Clock::getTime(c) + PowerOnTimeTicks;
        o->timer.appendAt(c, timer_time);
        o->state = STATE_POWERON;
    }
    
    static void deactivate (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state != STATE_INACTIVE)
        
        deactivate_common(c);
    }
    
    static BlockIndexType getCapacityBlocks (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == STATE_RUNNING)
        AMBRO_ASSERT(o->capacity_blocks > 0)
        
        return o->capacity_blocks;
    }
    
    static bool isWritable (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == STATE_RUNNING)
        
        // TBD: checks if card is actually writable
        return true;
    }
    
    static void startReadBlock (Context c, BlockIndexType block, DataWordType *buffer)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == STATE_RUNNING)
        AMBRO_ASSERT(o->io_state == IO_STATE_IDLE)
        AMBRO_ASSERT(block < o->capacity_blocks)
        
        uint32_t addr = o->is_sdhc ? block : (block * 512);
        TheSdio::startCommand(c, SdioIface::CommandParams{CMD_READ_SINGLE_BLOCK, addr, SdioIface::RESPONSE_SHORT, 0, SdioIface::DATA_DIR_READ, 1, buffer});
        
        o->io_state = IO_STATE_READING;
        o->cmd_finished = false;
    }
    
    static void startWriteBlock (Context c, BlockIndexType block, DataWordType const *buffer)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == STATE_RUNNING)
        AMBRO_ASSERT(o->io_state == IO_STATE_IDLE)
        AMBRO_ASSERT(block < o->capacity_blocks)
        
        uint32_t addr = o->is_sdhc ? block : (block * 512);
        TheSdio::startCommand(c, SdioIface::CommandParams{CMD_WRITE_BLOCK, addr, SdioIface::RESPONSE_SHORT, 0, SdioIface::DATA_DIR_WRITE, 1, (DataWordType *)buffer});
        
        o->io_state = IO_STATE_WRITING;
        o->cmd_finished = false;
    }
    
    using GetSdio = TheSdio;
    
private:
    static void timer_handler (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == STATE_POWERON || o->state == STATE_POWER_CLOCKS)
        
        if (o->state == STATE_POWERON) {
            TheSdio::completePowerOn(c);
            TimeType timer_time = Context::Clock::getTime(c) + PowerClocksTimeTicks;
            o->timer.appendAt(c, timer_time);
            o->state = STATE_POWER_CLOCKS;
        } else {
            TheSdio::startCommand(c, SdioIface::CommandParams{CMD_GO_IDLE_STATE, 0, SdioIface::RESPONSE_NONE});
            o->state = STATE_GO_IDLE;
        }
    }
    
    static void sdio_command_handler (Context c, SdioIface::CommandResults results, SdioIface::DataErrorCode data_error)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        switch (o->state) {
            case STATE_GO_IDLE: {
                if (results.error_code != SdioIface::CMD_ERROR_NONE) {
                    return init_error(c, 1);
                }
                TheSdio::startCommand(c, SdioIface::CommandParams{CMD_SEND_IF_COND, IfCondArgumentResponse, SdioIface::RESPONSE_SHORT});
                o->state = STATE_IF_COND;
            } break;
            
            case STATE_IF_COND: {
                if (results.error_code != SdioIface::CMD_ERROR_NONE) {
                    return init_error(c, 2);
                }
                if (results.response[0] != IfCondArgumentResponse) {
                    return init_error(c, 3);
                }
                send_app_cmd(c, 0);
                o->state = STATE_OP_COND_APP;
                o->poll_timer.setAfter(c, InitTimeoutTicks);
            } break;
            
            case STATE_OP_COND_APP: {
                if (!check_r1_response(results)) {
                    return init_error(c, 4);
                }
                TheSdio::startCommand(c, SdioIface::CommandParams{ACMD_SD_SEND_OP_COND, OpCondArgument, SdioIface::RESPONSE_SHORT, SdioIface::CMD_FLAG_NO_CRC_CHECK|SdioIface::CMD_FLAG_NO_CMDNUM_CHECK});
                o->state = STATE_OP_COND;
            } break;
            
            case STATE_OP_COND: {
                if (results.error_code != SdioIface::CMD_ERROR_NONE) {
                    return init_error(c, 5);
                }
                uint32_t ocr = results.response[0];
                if (!(ocr & OCR_CPUS)) {
                    if (o->poll_timer.isExpired(c)) {
                        return init_error(c, 6);
                    }
                    send_app_cmd(c, 0);
                    o->state = STATE_OP_COND_APP;
                    return;
                }
                o->is_sdhc = ocr & OCR_CCS;
                TheSdio::startCommand(c, SdioIface::CommandParams{CMD_ALL_SEND_CID, 0, SdioIface::RESPONSE_LONG, SdioIface::CMD_FLAG_NO_CMDNUM_CHECK});
                o->state = STATE_SEND_CID;
            } break;
            
            case STATE_SEND_CID: {
                if (results.error_code != SdioIface::CMD_ERROR_NONE) {
                    return init_error(c, 7);
                }
                TheSdio::startCommand(c, SdioIface::CommandParams{CMD_SEND_RELATIVE_ADDR, 0, SdioIface::RESPONSE_SHORT});
                o->state = STATE_RELATIVE_ADDR;
            } break;
            
            case STATE_RELATIVE_ADDR: {
                if (results.error_code != SdioIface::CMD_ERROR_NONE) {
                    return init_error(c, 8);
                }
                uint32_t response = results.response[0];
                if ((response & UINT32_C(0xE000))) {
                    return init_error(c, 9);
                }
                o->rca = response >> 16;
                TheSdio::reconfigureInterface(c, SdioIface::InterfaceParams{true, false});
                TheSdio::startCommand(c, SdioIface::CommandParams{CMD_SEND_CSD, (uint32_t)o->rca << 16, SdioIface::RESPONSE_LONG, SdioIface::CMD_FLAG_NO_CMDNUM_CHECK});
                o->state = STATE_SEND_CSD;
            } break;
            
            case STATE_SEND_CSD: {
                if (results.error_code != SdioIface::CMD_ERROR_NONE) {
                    return init_error(c, 9);
                }
                if (o->is_sdhc) {
                    uint32_t c_size = (results.response[2] >> 16) | ((results.response[1] & 0x3f) << 16);
                    if (c_size >= UINT32_C(0x3fffff)) {
                        return init_error(c, 11);
                    }
                    o->capacity_blocks = (c_size + 1) * UINT32_C(1024);
                } else {
                    uint8_t read_bl_len = (results.response[1] >> 16) & 0xf;
                    uint8_t c_size_mult = (results.response[2] >> 15) & 0x7;
                    uint16_t c_size = (results.response[2] >> 30) | ((results.response[1] & 0x3ff) << 2);
                    uint16_t mult = ((uint16_t)1 << (c_size_mult + 2));
                    uint32_t blocknr = (uint32_t)(c_size + 1) * mult;
                    uint16_t block_len = (uint16_t)1 << read_bl_len;
                    o->capacity_blocks = blocknr * (block_len / 512);
                }
                if (o->capacity_blocks == 0) {
                    return init_error(c, 10);
                }
                TheSdio::startCommand(c, SdioIface::CommandParams{CMD_SELECT_DESELECT, (uint32_t)o->rca << 16, SdioIface::RESPONSE_SHORT});
                o->state = STATE_SELECT;
            } break;
            
            case STATE_SELECT: {
                if (!check_r1_response(results)) {
                    return init_error(c, 11);
                }
                if (!TheSdio::IsWideMode) {
                    return complete_init(c);
                }
                send_app_cmd(c, o->rca);
                o->state = STATE_WIDEBUS_APP;
            } break;
            
            case STATE_WIDEBUS_APP: {
                if (!check_r1_response(results)) {
                    return init_error(c, 12);
                }
                TheSdio::startCommand(c, SdioIface::CommandParams{ACMD_SET_BUS_WIDTH, 2, SdioIface::RESPONSE_SHORT});
                o->state = STATE_WIDEBUS;
            } break;
            
            case STATE_WIDEBUS: {
                if (!check_r1_response(results)) {
                    return init_error(c, 13);
                }
                TheSdio::reconfigureInterface(c, SdioIface::InterfaceParams{true, true});
                return complete_init(c);
            } break;
            
            case STATE_RUNNING: {
                AMBRO_ASSERT(o->io_state == IO_STATE_READING || o->io_state == IO_STATE_WRITING)
                AMBRO_ASSERT(!o->cmd_finished || o->io_state == IO_STATE_WRITING)
                
                if (!o->cmd_finished) {
                    bool error = (!check_r1_response(results) || data_error != SdioIface::DATA_ERROR_NONE);
                    
                    if (o->io_state == IO_STATE_READING) {
                        return complete_operation(c, error);
                    } else {
                        o->write_error = error;
                        o->cmd_finished = true;
                        o->poll_timer.setAfter(c, ProgrammingTimeoutTicks);
                        TheSdio::startCommand(c, SdioIface::CommandParams{CMD_SEND_STATUS, (uint32_t)o->rca << 16, SdioIface::RESPONSE_SHORT});
                    }
                } else {
                    if (!check_r1_response(results)) {
                        o->write_error = true;
                    } else {
                        uint8_t card_state = (results.response[0] >> 9) & 0xF;
                        if (card_state == 6 || card_state == 7) {
                            if (o->poll_timer.isExpired(c)) {
                                o->write_error = true;
                            } else {
                                TheSdio::startCommand(c, SdioIface::CommandParams{CMD_SEND_STATUS, (uint32_t)o->rca << 16, SdioIface::RESPONSE_SHORT});
                                return;
                            }
                        }
                    }
                    return complete_operation(c, o->write_error);
                }
            } break;
            
            default:
                AMBRO_ASSERT(false);
        }
    }
    struct SdioCommandHandler : public AMBRO_WFUNC_TD(&SdioSdCard::sdio_command_handler) {};
    
    static void deactivate_common (Context c)
    {
        auto *o = Object::self(c);
        
        o->timer.unset(c);
        TheSdio::reset(c);
        
        o->state = STATE_INACTIVE;
    }
    
    static void init_error (Context c, uint8_t error_code)
    {
        deactivate_common(c);
        
        return InitHandler::call(c, error_code);
    }
    
    static bool check_r1_response (SdioIface::CommandResults results)
    {
        return results.error_code == SdioIface::CMD_ERROR_NONE && !(results.response[0] & UINT32_C(0xFDFFE008));
    }
    
    static void send_app_cmd (Context c, uint16_t rca)
    {
        TheSdio::startCommand(c, SdioIface::CommandParams{CMD_APP_CMD, (uint32_t)rca << 16, SdioIface::RESPONSE_SHORT});
    }
    
    static void complete_init (Context c)
    {
        auto *o = Object::self(c);
        
        o->state = STATE_RUNNING;
        o->io_state = IO_STATE_IDLE;
        
        return InitHandler::call(c, 0);
    }
    
    static void complete_operation (Context c, bool error)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->state == STATE_RUNNING)
        AMBRO_ASSERT(o->io_state != IO_STATE_IDLE)
        
        o->io_state = IO_STATE_IDLE;
        
        return CommandHandler::call(c, error);
    }
    
public:
    struct Object : public ObjBase<SdioSdCard, ParentObject, MakeTypeList<
        TheDebugObject,
        TheSdio
    >> {
        typename Context::EventLoop::TimedEvent timer;
        uint8_t state;
        typename TheClockUtils::PollTimer poll_timer;
        bool is_sdhc;
        uint16_t rca;
        uint32_t capacity_blocks;
        uint8_t io_state;
        bool cmd_finished;
        bool write_error;
    };
};

APRINTER_ALIAS_STRUCT_EXT(SdioSdCardService, (
    APRINTER_AS_TYPE(SdioService)
), (
    template <typename Context, typename ParentObject, typename InitHandler, typename CommandHandler>
    using SdCard = SdioSdCard<Context, ParentObject, InitHandler, CommandHandler, SdioSdCardService>;
))

#include <aprinter/EndNamespace.h>

#endif
