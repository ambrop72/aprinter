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

#ifndef APRINTER_AT91SAM_EMAC_MII_H
#define APRINTER_AT91SAM_EMAC_MII_H

#include <stdint.h>
#include <string.h>
#include <limits.h>

#include <emac.h>
#include <gpio.h>
#include <rstc.h>

#include <aprinter/meta/MinMax.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/Callback.h>
#include <aprinter/base/Assert.h>
#include <aprinter/hal/common/MiiCommon.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, typename ClientParams, typename Params>
class At91SamEmacMii {
    static_assert(Params::Rmii, "Only RMII is currently supported.");
    
public:
    struct Object;
    
private:
    using TimeType = typename Context::Clock::TimeType;
    using SendBufferType = typename ClientParams::SendBufferType;
    using RecvBufferType = typename ClientParams::RecvBufferType;
    
    enum class InitState : uint8_t {INACTIVE, WAIT_RST, RUNNING};
    enum class PhyMaintState : uint8_t {IDLE, BUSY};
    
    static TimeType const ResetPollTicks = 0.1 * Context::Clock::time_freq;
    static uint16_t const MaxResetPolls = 50;
    static size_t const RwBufSize = 1536;
    static TimeType const PhyMaintPollTicks = 0.01 * Context::Clock::time_freq;
    static uint16_t const MaxPhyMaintPolls = 200;
    
public:
    static uint8_t const SupportedSpeeds = MiiSpeed::SPEED_10M_HD|MiiSpeed::SPEED_10M_FD|MiiSpeed::SPEED_100M_HD|MiiSpeed::SPEED_100M_FD;
    static uint8_t const SupportedPause = MiiPauseAbility::RX_ONLY;
    
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        gpio_configure_pin(PIN_EEMAC_EREFCK, PIN_EMAC_FLAGS);
        gpio_configure_pin(PIN_EMAC_ETX0,    PIN_EMAC_FLAGS);
        gpio_configure_pin(PIN_EMAC_ETX1,    PIN_EMAC_FLAGS);
        gpio_configure_pin(PIN_EMAC_ETXEN,   PIN_EMAC_FLAGS);
        gpio_configure_pin(PIN_EMAC_ECRSDV,  PIN_EMAC_FLAGS);
        gpio_configure_pin(PIN_EMAC_ERX0,    PIN_EMAC_FLAGS);
        gpio_configure_pin(PIN_EMAC_ERX1,    PIN_EMAC_FLAGS);
        gpio_configure_pin(PIN_EMAC_ERXER,   PIN_EMAC_FLAGS);
        gpio_configure_pin(PIN_EMAC_EMDC,    PIN_EMAC_FLAGS);
        gpio_configure_pin(PIN_EMAC_EMDIO,   PIN_EMAC_FLAGS);
        
        o->timer.init(c, APRINTER_CB_STATFUNC_T(&At91SamEmacMii::timer_handler));
        o->init_state = InitState::INACTIVE;
        o->phy_maint_state = PhyMaintState::IDLE;
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        
        reset_internal(c);
        o->timer.deinit(c);
    }
    
    static void reset (Context c)
    {
        reset_internal(c);
    }
    
    static void activate (Context c, uint8_t const *mac_addr)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->init_state == InitState::INACTIVE)
        
        rstc_set_external_reset(RSTC, 13);
        rstc_reset_extern(RSTC);
        
        o->init_state = InitState::WAIT_RST;
        o->timer.appendNowNotAlready(c);
        o->mac_addr = mac_addr;
        o->poll_counter = 1;
    }
    
    static bool sendFrame (Context c, SendBufferType send_buffer)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->init_state == InitState::RUNNING)
        
        size_t total_length = send_buffer.getTotalLength();
        if (total_length > RwBufSize) {
            return false;
        }
        
        size_t buf_pos = 0;
        do {
            size_t chunk_length = send_buffer.getChunkLength();
            memcpy(o->frame_buf + buf_pos, send_buffer.getChunkPtr(), chunk_length);
            buf_pos += chunk_length;
        } while (send_buffer.nextChunk());
        
        memset(o->frame_buf + buf_pos, 0, RwBufSize - buf_pos);
        
        auto write_res = emac_dev_write(&o->emac_dev, o->frame_buf, total_length, nullptr);
        if (write_res != EMAC_OK) {
            return false;
        }
        
        return true;
    }
    
    static bool recvFrame (Context c, RecvBufferType recv_buffer)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->init_state == InitState::RUNNING)
        
        uint32_t length;
        auto read_res = emac_dev_read(&o->emac_dev, o->frame_buf, RwBufSize, &length);
        if (read_res != EMAC_OK) {
            return false;
        }
        
        size_t max_length = recv_buffer.getMaxLength();
        if (length > max_length) {
            return false;
        }
        
        size_t buf_pos = 0;
        do {
            size_t max_chunk_length = recv_buffer.getMaxChunkLength();
            size_t chunk_length = MinValue(max_chunk_length, (size_t)(length - buf_pos));
            memcpy(recv_buffer.getChunkPtr(), o->frame_buf + buf_pos, chunk_length);
            recv_buffer.chunkWritten(chunk_length);
            buf_pos += chunk_length;
        } while (buf_pos < length);
        
        return true;
    }
    
    static void startPhyMaintenance (Context c, MiiPhyMaintCommand command)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->init_state == InitState::RUNNING)
        AMBRO_ASSERT(o->phy_maint_state == PhyMaintState::IDLE)
        
        bool read_write = (command.io_type == PhyMaintCommandIoType::READ_WRITE);
        
        bool start = emac_is_phy_idle(EMAC);
        if (start) {
            emac_enable_management(EMAC, 1);
            emac_maintain_phy(EMAC, command.phy_address, command.reg_address, read_write, command.data);
        }
        
        o->phy_maint_state = PhyMaintState::BUSY;
        o->timer.appendAt(c, (TimeType)(Context::Clock::getTime(c) + PhyMaintPollTicks));
        o->poll_counter = start ? 1 : 0;
        o->phy_maint_rw = read_write;
    }
    
    static void configureLink (Context c, MiiLinkParams link_params)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->init_state == InitState::RUNNING)
        
        switch (link_params.mode) {
            case MiiSpeed::SPEED_10M_HD: {
                emac_set_speed(EMAC, 0);
                emac_enable_full_duplex(EMAC, 0);
            } break;
            
            case MiiSpeed::SPEED_10M_FD: {
                emac_set_speed(EMAC, 0);
                emac_enable_full_duplex(EMAC, 1);
            } break;
            
            case MiiSpeed::SPEED_100M_HD: {
                emac_set_speed(EMAC, 1);
                emac_enable_full_duplex(EMAC, 0);
            } break;
            
            case MiiSpeed::SPEED_100M_FD: {
                emac_set_speed(EMAC, 1);
                emac_enable_full_duplex(EMAC, 1);
            } break;
            
            default: AMBRO_ASSERT(false);
        }
        
        emac_enable_pause_frame(EMAC, bool(link_params.pause_config & MiiPauseConfig::RX_ENABLE));
        
        emac_enable_transceiver_clock(EMAC, 1);
    }
    
    static void resetLink (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->init_state == InitState::RUNNING)
        
        emac_enable_transceiver_clock(EMAC, 0);
    }
    
private:
    static void reset_internal (Context c)
    {
        auto *o = Object::self(c);
        
        if (o->init_state == InitState::RUNNING) {
            emac_enable_management(EMAC, 0);
            emac_network_control(EMAC, 0);
            pmc_disable_periph_clk(ID_EMAC);
        }
        
        o->timer.unset(c);
        o->init_state = InitState::INACTIVE;
        o->phy_maint_state = PhyMaintState::IDLE;
    }
    
    static void timer_handler (Context c)
    {
        auto *o = Object::self(c);
        
        switch (o->init_state) {
            case InitState::WAIT_RST: {
                if ((rstc_get_status(RSTC) & RSTC_SR_NRSTL)) {
                    if (o->poll_counter >= MaxResetPolls) {
                        reset_internal(c);
                        return ClientParams::ActivateHandler::call(c, true);
                    }
                    o->poll_counter++;
                    o->timer.appendAfterPrevious(c, ResetPollTicks);
                    return;
                }
                
                pmc_enable_periph_clk(ID_EMAC);
                
                emac_options_t emac_options = emac_options_t();
                emac_options.uc_copy_all_frame = 0;
                emac_options.uc_no_broadcast = 0;
                memcpy(emac_options.uc_mac_addr, o->mac_addr, 6);
                
                o->emac_dev = emac_device_t();
                o->emac_dev.p_hw = EMAC;
                emac_dev_init(EMAC, &o->emac_dev, &emac_options);
                
                emac_enable_rmii(EMAC, Params::Rmii);
                
                o->init_state = InitState::RUNNING;
                return ClientParams::ActivateHandler::call(c, false);
            } break;
            
            case InitState::RUNNING: {
                AMBRO_ASSERT(o->phy_maint_state == PhyMaintState::BUSY)
                
                MiiPhyMaintResult result;
                result.error = true;
                result.data = 0;
                
                if (o->poll_counter == 0) {
                    goto end_phy_maint;
                }
                
                if (!emac_is_phy_idle(EMAC)) {
                    if (o->poll_counter >= MaxPhyMaintPolls) {
                        emac_enable_management(EMAC, 0);
                        goto end_phy_maint;
                    }
                    o->poll_counter++;
                    o->timer.appendAfterPrevious(c, PhyMaintPollTicks);
                    return;
                }
                
                result.error = false;
                if (o->phy_maint_rw) {
                    result.data = emac_get_phy_data(EMAC);
                }
                emac_enable_management(EMAC, 0);
                
            end_phy_maint:
                o->phy_maint_state = PhyMaintState::IDLE;
                return ClientParams::PhyMaintHandler::call(c, result);
            } break;
            
            default: AMBRO_ASSERT(false);
        }
    }
    
public:
    struct Object : public ObjBase<At91SamEmacMii, ParentObject, EmptyTypeList> {
        typename Context::EventLoop::TimedEvent timer;
        InitState init_state;
        PhyMaintState phy_maint_state;
        uint8_t const *mac_addr;
        uint16_t poll_counter;
        emac_device_t emac_dev;
        bool phy_maint_rw;
        char frame_buf[RwBufSize];
    };
};

struct At91SamEmacMiiService {
    template <typename Context, typename ParentObject, typename ClientParams>
    using Mii = At91SamEmacMii<Context, ParentObject, ClientParams, At91SamEmacMiiService>;
};

#include <aprinter/EndNamespace.h>

#endif
