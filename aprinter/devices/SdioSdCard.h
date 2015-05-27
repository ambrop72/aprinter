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

#include <stdint.h>

#include <aprinter/base/Object.h>
#include <aprinter/meta/WrapFunction.h>
#include <aprinter/meta/WrapDouble.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/WrapBuffer.h>
#include <aprinter/structure/DoubleEndedList.h>
#include <aprinter/devices/SdioInterface.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, int MaxCommands, typename InitHandler, typename CommandHandler, typename Params>
class SdioSdCard {
public:
    struct Object;
    
private:
    using TheDebugObject = DebugObject<Context, Object>;
    struct SdioCommandHandler;
    struct SdioDataHandler;
    using TheSdio = typename Params::SdioService::template Sdio<Context, Object, SdioCommandHandler, SdioDataHandler>;
    using TimeType = typename Context::Clock::TimeType;
    
    enum {
        STATE_INACTIVE, STATE_POWERON, STATE_GO_IDLE, STATE_IF_COND,
        STATE_OP_COND_APP, STATE_OP_COND, STATE_SEND_CID, STATE_RELATIVE_ADDR,
        STATE_SEND_CSD, STATE_SELECT, STATE_RUNNING
    };
    
    enum {READ_STATE_IDLE, READ_STATE_BUSY};
    
    static TimeType const PowerOnTimeTicks = 0.0015 * Context::Clock::time_freq;
    static uint16_t const OpCmdAttempts = 255;
    
    static const uint8_t CMD_GO_IDLE_STATE = 0;
    static const uint8_t CMD_ALL_SEND_CID = 2;
    static const uint8_t CMD_SEND_RELATIVE_ADDR = 3;
    static const uint8_t CMD_SELECT_DESELECT = 7;
    static const uint8_t CMD_SEND_IF_COND = 8;
    static const uint8_t CMD_SEND_CSD = 9;
    static const uint8_t CMD_READ_SINGLE_BLOCK = 17;
    static const uint8_t CMD_APP_CMD = 55;
    static const uint8_t ACMD_SD_SEND_OP_COND = 41;
    
    static const uint32_t OCR_CCS = (UINT32_C(1) << 30);
    static const uint32_t OCR_CPUS = (UINT32_C(1) << 31);
    
public:
    using BlockIndexType = uint32_t;
    static size_t const BlockSize = TheSdio::BlockSize;
    
    class ReadState {
        friend SdioSdCard;
        
        DoubleEndedListNode<ReadState> queue_node;
        BlockIndexType block;
        WrapBuffer buf;
        bool completed;
        bool error;
    };
    
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
    
    static void queueReadBlock (Context c, BlockIndexType block, uint8_t *data1, size_t wrap, uint8_t *data2, ReadState *state)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == STATE_RUNNING)
        AMBRO_ASSERT(block < o->capacity_blocks)
        
        state->block = block;
        state->buf = WrapBuffer::Make(wrap, (char *)data1, (char *)data2);
        state->completed = false;
        o->queue_list.append(state);
        
        if (!o->timer.isSet(c)) {
            o->timer.prependNowNotAlready(c);
        }
    }
    
    static bool checkReadBlock (Context c, ReadState *state, bool *out_error)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == STATE_RUNNING)
        
        if (!state->completed) {
            return false;
        }
        *out_error = state->error;
        return true;
    }
    
    static void unsetEvent (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == STATE_RUNNING)
        
        // Nothing. This is meant to cancel any stray callbacks
        // when the user has completed all operations, but there
        // won't be any in the first place in this implementation.
    }
    
private:
    using QueueList = DoubleEndedList<ReadState, &ReadState::queue_node>;
    
    static void timer_handler (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        switch (o->state) {
            case STATE_POWERON: {
                TheSdio::completePowerOn(c);
                TheSdio::startCommand(c, SdioIface::CommandParams{CMD_GO_IDLE_STATE, 0, SdioIface::RESPONSE_NONE});
                o->state = STATE_GO_IDLE;
            } break;
            
            case STATE_RUNNING: {
                if (!o->queue_list.isEmpty() && o->read_state == READ_STATE_IDLE) {
                    ReadState *entry = o->queue_list.first();
                    AMBRO_ASSERT(!entry->completed)
                    uint32_t addr = o->is_sdhc ? entry->block : (entry->block * 512);
                    TheSdio::startData(c, SdioIface::DataParams{SdioIface::DATA_DIR_READ, 1, o->buffer});
                    TheSdio::startCommand(c, SdioIface::CommandParams{CMD_READ_SINGLE_BLOCK, addr, SdioIface::RESPONSE_SHORT});
                    o->read_state = READ_STATE_BUSY;
                    o->cmd_finished = false;
                    o->data_finished = false;
                }
            } break;
            
            default:
                AMBRO_ASSERT(false);
        }
    }
    
    static void sdio_command_handler (Context c, SdioIface::CommandResults results)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        switch (o->state) {
            case STATE_GO_IDLE: {
                if (results.error_code != SdioIface::CMD_ERROR_NONE) {
                    return init_error(c, 1);
                }
                TheSdio::startCommand(c, SdioIface::CommandParams{CMD_SEND_IF_COND, UINT32_C(0x1AA), SdioIface::RESPONSE_SHORT});
                o->state = STATE_IF_COND;
            } break;
            
            case STATE_IF_COND: {
                if (results.error_code != SdioIface::CMD_ERROR_NONE) {
                    return init_error(c, 2);
                }
                if (results.response[0] != UINT32_C(0x1AA)) {
                    return init_error(c, 3);
                }
                send_app_cmd(c, 0);
                o->state = STATE_OP_COND_APP;
                o->op_cmd_attempts_left = OpCmdAttempts;
            } break;
            
            case STATE_OP_COND_APP: {
                if (!check_r1_response(results)) {
                    return init_error(c, 4);
                }
                TheSdio::startCommand(c, SdioIface::CommandParams{ACMD_SD_SEND_OP_COND, UINT32_C(0x40000000), SdioIface::RESPONSE_SHORT});
                o->state = STATE_OP_COND;
            } break;
            
            case STATE_OP_COND: {
                if (results.error_code != SdioIface::CMD_ERROR_NONE) {
                    return init_error(c, 5);
                }
                uint32_t ocr = results.response[0];
                if (!(ocr & OCR_CPUS)) {
                    if (o->op_cmd_attempts_left <= 1) {
                        return init_error(c, 6);
                    }
                    send_app_cmd(c, 0);
                    o->state = STATE_OP_COND_APP;
                    o->op_cmd_attempts_left--;
                    return;
                }
                o->is_sdhc = ocr & OCR_CCS;
                TheSdio::startCommand(c, SdioIface::CommandParams{CMD_ALL_SEND_CID, 0, SdioIface::RESPONSE_LONG});
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
                if ((response & UINT32_C(0xE008)) != 0) {
                    return init_error(c, 9);
                }
                o->rca = response >> 16;
                TheSdio::startCommand(c, SdioIface::CommandParams{CMD_SEND_CSD, (uint32_t)rca << 16, SdioIface::RESPONSE_LONG});
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
                TheSdio::startCommand(c, SdioIface::CommandParams{CMD_SELECT_DESELECT, (uint32_t)rca << 16, SdioIface::RESPONSE_SHORT});
                o->state = STATE_SELECT;
            } break;
            
            case STATE_SELECT: {
                if (!check_r1_response(results)) {
                    return init_error(c, 11);
                }
                TheSdio::reconfigureInterface(c, SdioIface::InterfaceParams{true, TheSdio::IsWideMode});
                o->state = STATE_RUNNING;
                o->queue_list.init();
                o->read_state = READ_STATE_IDLE;
                return InitHandler::call(c, 0);
            } break;
            
            case STATE_RUNNING: {
                AMBRO_ASSERT(o->read_state == READ_STATE_BUSY)
                AMBRO_ASSERT(!o->cmd_finished)
                
                if (!check_r1_response(results)) {
                    if (!o->data_finished) {
                        TheSdio::abortData(c);
                    }
                    return complete_operation(c, true);
                }
                o->cmd_finished = true;
                return check_operation_complete(c);
            } break;
            
            default:
                AMBRO_ASSERT(false);
        }
    }
    struct SdioCommandHandler : public AMBRO_WFUNC_TD(&SdioSdCard::sdio_command_handler) {};
    
    static void sdio_data_handler (Context c, typename SdioIface::DataResults results)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->state == STATE_RUNNING)
        AMBRO_ASSERT(o->read_state == READ_STATE_BUSY)
        AMBRO_ASSERT(!o->data_finished)
        
        o->data_finished = true;
        o->data_error = results.error_code;
        return check_operation_complete(c);
    }
    struct SdioDataHandler : public AMBRO_WFUNC_TD(&SdioSdCard::sdio_data_handler) {};
    
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
    
    static bool check_r1_response (Context c, SdioIface::CommandResults results)
    {
        return results.error_code == SdioIface::CMD_ERROR_NONE && (results.response[0] & UINT32_C(0xFDFFE008)) == 0;
    }
    
    static void send_app_cmd (Context c, uint16_t rca)
    {
        TheSdio::startCommand(c, SdioIface::CommandParams{CMD_APP_CMD, (uint32_t)rca << 16, SdioIface::RESPONSE_SHORT});
    }
    
    static void complete_operation (Context c, bool error)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->state == STATE_RUNNING)
        AMBRO_ASSERT(o->read_state == READ_STATE_BUSY)
        
        ReadState *entry = o->queue_list.first();
        AMBRO_ASSERT(!entry->completed)
        
        o->queue_list.removeFirst();
        o->read_state = READ_STATE_IDLE;
        if (!o->timer.isSet(c)) {
            o->timer.prependNowNotAlready(c);
        }
        
        entry->completed = true;
        entry->error = error;
        if (!error) {
            entry->buf.copyIn(0, BlockSize, (char const *)o->buffer);
        }
        
        return CommandHandler::call(c);
    }
    
    static void check_operation_complete (Context c)
    {
        auto *o = Object::self(c);
        
        if (o->cmd_finished && o->data_finished) {
            bool error = o->data_error != SdioIface::DATA_ERROR_NONE;
            return complete_operation(c, error);
        }
    }
    
public:
    struct Object : public ObjBase<SdioSdCard, ParentObject, MakeTypeList<
        TheDebugObject,
        TheSdio
    >> {
        typename Context::EventLoop::QueuedEvent timer;
        uint8_t state;
        uint16_t op_cmd_attempts_left;
        bool is_sdhc;
        uint16_t rca;
        uint32_t capacity_blocks;
        QueueList queue_list;
        uint8_t read_state;
        bool cmd_finished;
        bool data_finished;
        SdioIface::DataErrorCode data_error;
        uint32_t buffer[BlockSize / 4];
    };
};

template <
    int TBusWidth,
    typename TSdioService
>
struct SdioSdCardService {
    static int const BusWidth = TBusWidth;
    using SdioService = TSdioService;
    
    template <typename Context, typename ParentObject, int MaxCommands, typename InitHandler, typename CommandHandler>
    using SdCard = SdioSdCard<Context, ParentObject, MaxCommands, InitHandler, CommandHandler, SdioSdCardService>;
};

#include <aprinter/EndNamespace.h>

#endif
