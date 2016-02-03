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
#include <rstc.h>

#include <aprinter/meta/MinMax.h>
#include <aprinter/meta/TypeListUtils.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/Callback.h>
#include <aprinter/base/Assert.h>
#include <aprinter/hal/common/MiiCommon.h>
#include <aprinter/hal/at91/At91SamPins.h>
#include <aprinter/base/Lock.h>
#include <aprinter/system/InterruptLock.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, typename ClientParams, typename Params>
class At91SamEmacMii {
    static_assert(ClientParams::Rmii, "Only RMII is currently supported.");
    
public:
    struct Object;
    
private:
    using TimeType = typename Context::Clock::TimeType;
    using SendBufferType = typename ClientParams::SendBufferType;
    
    enum class InitState : uint8_t {INACTIVE, WAIT_RST, RUNNING};
    enum class PhyMaintState : uint8_t {IDLE, BUSY};
    
    static TimeType const ResetPollTicks = 0.1 * Context::Clock::time_freq;
    static uint16_t const MaxResetPolls = 50;
    static TimeType const PhyMaintPollTicks = 0.01 * Context::Clock::time_freq;
    static uint16_t const MaxPhyMaintPolls = 200;
    
    static int const RxFrameOffset = 2;
    
    using FastEvent = typename Context::EventLoop::template FastEventSpec<At91SamEmacMii>;
    
public:
    static uint8_t const SupportedSpeeds = MiiSpeed::SPEED_10M_HD|MiiSpeed::SPEED_10M_FD|MiiSpeed::SPEED_100M_HD|MiiSpeed::SPEED_100M_FD;
    static uint8_t const SupportedPause = MiiPauseAbility::RX_ONLY;
    
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        using Mode = At91SamPinPeriphMode<At91SamPinPullModeNormal>;
        using Periph = At91SamPeriphA;
        Context::Pins::template setPeripheral<At91SamPin<At91SamPioB, 0>, Mode>(c, Periph());
        Context::Pins::template setPeripheral<At91SamPin<At91SamPioB, 1>, Mode>(c, Periph());
        Context::Pins::template setPeripheral<At91SamPin<At91SamPioB, 2>, Mode>(c, Periph());
        Context::Pins::template setPeripheral<At91SamPin<At91SamPioB, 3>, Mode>(c, Periph());
        Context::Pins::template setPeripheral<At91SamPin<At91SamPioB, 4>, Mode>(c, Periph());
        Context::Pins::template setPeripheral<At91SamPin<At91SamPioB, 5>, Mode>(c, Periph());
        Context::Pins::template setPeripheral<At91SamPin<At91SamPioB, 6>, Mode>(c, Periph());
        Context::Pins::template setPeripheral<At91SamPin<At91SamPioB, 7>, Mode>(c, Periph());
        Context::Pins::template setPeripheral<At91SamPin<At91SamPioB, 8>, Mode>(c, Periph());
        Context::Pins::template setPeripheral<At91SamPin<At91SamPioB, 9>, Mode>(c, Periph());
        
        Context::EventLoop::template initFastEvent<FastEvent>(c, At91SamEmacMii::event_handler);
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
        
        o->init_state = InitState::WAIT_RST;
        o->timer.appendNowNotAlready(c);
        o->mac_addr = mac_addr;
        o->poll_counter = 1;
    }
    
    static bool sendFrame (Context c, SendBufferType *send_buffer)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->init_state == InitState::RUNNING)
        
        size_t total_length = send_buffer->getTotalLength();
        
        void *dev_buffer;
        auto write_start_res = emac_dev_write_start(&o->emac_dev, total_length, &dev_buffer);
        if (write_start_res != EMAC_OK) {
            if (write_start_res == EMAC_TX_BUSY) {
                // Try again after cleaning up transmitted frames in the TX buffer.
                // Since we're calling emac_handler we also have to schedule the
                // event_handler, to avoid losing any receive events.
                emac_handler(&o->emac_dev);
                Context::EventLoop::template triggerFastEvent<FastEvent>(c);
                write_start_res = emac_dev_write_start(&o->emac_dev, total_length, &dev_buffer);
            }
            if (write_start_res != EMAC_OK) {
                return false;
            }
        }
        
        size_t buf_pos = 0;
        do {
            size_t chunk_length = send_buffer->getChunkLength();
            AMBRO_ASSERT(chunk_length <= total_length - buf_pos)
            memcpy((char *)dev_buffer + buf_pos, send_buffer->getChunkPtr(), chunk_length);
            buf_pos += chunk_length;
        } while (send_buffer->nextChunk());
        AMBRO_ASSERT(buf_pos == total_length)
        
        auto write_end_res = emac_dev_write_end(&o->emac_dev, total_length);
        if (write_end_res != EMAC_OK) {
            return false;
        }
        
        return true;
    }
    
    static void startPhyMaintenance (Context c, MiiPhyMaintCommand command)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->init_state == InitState::RUNNING)
        AMBRO_ASSERT(o->phy_maint_state == PhyMaintState::IDLE)
        
        bool start = emac_is_phy_idle(EMAC);
        if (start) {
            bool read_or_write = (command.io_type == PhyMaintCommandIoType::READ_ONLY);
            emac_enable_management(EMAC, 1);
            emac_maintain_phy(EMAC, command.phy_address, command.reg_address, read_or_write, command.data);
        }
        
        o->phy_maint_state = PhyMaintState::BUSY;
        o->timer.appendAt(c, (TimeType)(Context::Clock::getTime(c) + PhyMaintPollTicks));
        o->poll_counter = start ? 1 : 0;
    }
    
    static void configureLink (Context c, MiiLinkParams link_params)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->init_state == InitState::RUNNING)
        
        switch (link_params.speed) {
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
    
    static void emac_irq (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->init_state == InitState::RUNNING)
        
        // Ideally we would be calling emac_handler here but the emac driver code
        // seems to be written without regard to interrupt safety. So, we temporarily
        // disable the interrupt and delegate this to the main loop.
        // Note, we must make regular calls to emac_handler because that cleans up
        // after transmitted frames, preventing overrun of the tx buffer.
        
        NVIC_DisableIRQ(EMAC_IRQn);
        Context::EventLoop::template triggerFastEvent<FastEvent>(c);
    }
    
private:
    static void reset_internal (Context c)
    {
        auto *o = Object::self(c);
        
        if (o->init_state == InitState::RUNNING) {
            AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
                NVIC_DisableIRQ(EMAC_IRQn);
            }
            
            emac_enable_management(EMAC, 0);
            emac_network_control(EMAC, 0);
            emac_disable_interrupt(EMAC, UINT32_MAX);
            
            NVIC_ClearPendingIRQ(EMAC_IRQn);
            
            pmc_disable_periph_clk(ID_EMAC);
        }
        
        Context::EventLoop::template resetFastEvent<FastEvent>(c);
        o->timer.unset(c);
        o->init_state = InitState::INACTIVE;
        o->phy_maint_state = PhyMaintState::IDLE;
    }
    
    static void timer_handler (Context c)
    {
        auto *o = Object::self(c);
        
        switch (o->init_state) {
            case InitState::WAIT_RST: {
                if (!(rstc_get_status(RSTC) & RSTC_SR_NRSTL)) {
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
                emac_options.uc_copy_all_frame = 1;
                emac_options.uc_no_boardcast = 0;
                memcpy(emac_options.uc_mac_addr, o->mac_addr, 6);
                
                o->emac_dev = emac_device_t();
                o->emac_dev.p_hw = EMAC;
                emac_dev_init(EMAC, &o->emac_dev, &emac_options);
                
                emac_enable_interrupt(EMAC, EMAC_IER_RCOMP);
                
                emac_set_clock(EMAC, F_MCK);
                
                emac_enable_rmii(EMAC, ClientParams::Rmii);
                
                emac_set_rx_buffer_offset(EMAC, RxFrameOffset);
                
                NVIC_ClearPendingIRQ(EMAC_IRQn);
                NVIC_SetPriority(EMAC_IRQn, INTERRUPT_PRIORITY);
                
                o->init_state = InitState::RUNNING;
                
                AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
                    NVIC_EnableIRQ(EMAC_IRQn);
                }
                
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
                result.data = emac_get_phy_data(EMAC);
                
                emac_enable_management(EMAC, 0);
                
            end_phy_maint:
                o->phy_maint_state = PhyMaintState::IDLE;
                return ClientParams::PhyMaintHandler::call(c, result);
            } break;
            
            default: AMBRO_ASSERT(false);
        }
    }
    
    static void event_handler (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->init_state == InitState::RUNNING)
        
        emac_handler(&o->emac_dev);
        
        // Clear pending IRQ and re-enable the interrupt.
        // Note, clearing here after the emac_handler call is safe,
        // there is no risk of losing interrupts, because the interrupts
        // are level-triggered (as far as I can tell).
        NVIC_ClearPendingIRQ(EMAC_IRQn);
        NVIC_EnableIRQ(EMAC_IRQn);
        
        uint8_t *data1;
        uint8_t *data2;
        uint32_t size1;
        uint32_t size2;
        emac_dev_read_state_t state;
        
        uint32_t read_res = emac_dev_read_start(&o->emac_dev, &state, &data1, &data2, &size1, &size2);
        if (read_res == EMAC_RX_NULL) {
            return;
        }
        
        if (read_res != EMAC_OK) {
            data1 = nullptr;
            data2 = nullptr;
            size1 = 0;
            size2 = 0;
        } else {
            data1 += RxFrameOffset;
        }
        
        ClientParams::ReceiveHandler::call(c, data1, data2, size1, size2);
        
        if (read_res == EMAC_OK) {
            emac_dev_read_end(&o->emac_dev, &state);
        }
        
        Context::EventLoop::template triggerFastEvent<FastEvent>(c);
    }
    
public:
    using EventLoopFastEvents = MakeTypeList<FastEvent>;
    
    struct Object : public ObjBase<At91SamEmacMii, ParentObject, EmptyTypeList> {
        typename Context::EventLoop::TimedEvent timer;
        InitState init_state;
        PhyMaintState phy_maint_state;
        uint8_t const *mac_addr;
        uint16_t poll_counter;
        emac_device_t emac_dev;
    };
};

#define APRINTER_AT91SAM_EMAC_MII_GLOBAL(the_mii, context) \
extern "C" \
__attribute__((used)) \
void EMAC_Handler (void) \
{ \
    the_mii::emac_irq(MakeInterruptContext((context))); \
}

struct At91SamEmacMiiService {
    template <typename Context, typename ParentObject, typename ClientParams>
    using Mii = At91SamEmacMii<Context, ParentObject, ClientParams, At91SamEmacMiiService>;
};

#include <aprinter/EndNamespace.h>

#endif
