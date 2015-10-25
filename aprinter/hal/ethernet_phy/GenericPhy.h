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

#ifndef APRINTER_GENERIC_PHY_H
#define APRINTER_GENERIC_PHY_H

#include <stdint.h>

#include <aprinter/base/Object.h>
#include <aprinter/base/Callback.h>
#include <aprinter/base/Assert.h>
#include <aprinter/hal/common/MiiCommon.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, typename ClientParams, typename Params>
class GenericPhy {
public:
    struct Object;
    
private:
    using TimeType = typename Context::Clock::TimeType;
    using PhyRequester = typename ClientParams::PhyRequester;
    
    static uint8_t const PhyAddr = Params::PhyAddr;
    
    static uint8_t const SupportedSpeeds = PhyRequester::SupportedSpeeds;
    static uint8_t const SupportedPause = PhyRequester::SupportedPause;
    
    static TimeType const BeforeResetTicks = 0.5 * Context::Clock::time_freq;
    static TimeType const ResetWaitTicks = 0.6 * Context::Clock::time_freq;
    static TimeType const StatusPollTicks = 0.2 * Context::Clock::time_freq;
    static uint16_t const AutoNegMaxPolls = 50;
    
    enum class State : uint8_t {
        IDLE,
        WAIT_TO_RESET,
        RESET_WRITE,
        RESET_WAIT,
        INIT_READ_STATUS,
        INIT_READ_ANEG_ADV,
        INIT_WRITE_ANEG_ADV,
        INIT_READ_CONTROL,
        INIT_WRITE_CONTROL,
        LINK_DOWN_WAIT,
        LINK_DOWN_READ_STATUS,
        ANEG_READ_CONTROL,
        ANEG_WRITE_CONTROL,
        ANEG_WAIT,
        ANEG_READ_STATUS,
        ANEG_READ_LP_ABILITY,
        LINK_UP_WAIT,
        LINK_UP_READ_STATUS
    };
    
    struct RegNum { enum : uint8_t {
        CONTROL                 = 0,
        STATUS                  = 1,
        PHY_ID_1                = 2,
        PHY_ID_2                = 3,
        ANEG_ADV                = 4,
        ANEG_LP_ABILITY         = 5,
        ANEG_EXPANSION          = 6,
        ANEG_NP_TX              = 7,
        ANEG_LP_RX_NP           = 8,
        MASTER_SLAVE_CONTROL    = 9,
        MASTER_SLAVE_STATUS     = 10,
        PSE_CONTROL             = 11,
        PSE_STATUS              = 12,
        MMD_ACCESS_CONTROL      = 13,
        MMD_ACCESS_ADDRESS_DATA = 14,
        EXTENDED_STATUS         = 15
    }; };
    
    struct ControlReg { enum : uint16_t {
        RESET        = (uint16_t)1 << 15,
        ANEG_ENABLE  = (uint16_t)1 << 12,
        ISOLATE      = (uint16_t)1 << 10,
        RESTART_ANEG = (uint16_t)1 << 9
    }; };
    
    struct StatusReg { enum : uint16_t {
        SUPPORT_100BASE_X_FD = (uint16_t)1 << 14,
        SUPPORT_100BASE_X_HD = (uint16_t)1 << 13,
        SUPPORT_10MB_FD      = (uint16_t)1 << 12,
        SUPPORT_10MB_HD      = (uint16_t)1 << 11,
        ANEG_COMPLETE        = (uint16_t)1 << 5,
        ANEG_ABILITY         = (uint16_t)1 << 3,
        LINK_STATUS          = (uint16_t)1 << 2,
        EXTENDED_CAP         = (uint16_t)1 << 0
    }; };
    
    struct AnegAdvReg { enum : uint16_t {
        ADV_ASYM_PAUSE_FD    = (uint16_t)1 << 11,
        ADV_PAUSE_FD         = (uint16_t)1 << 10,
        ADV_100BASE_T4       = (uint16_t)1 << 9,
        ADV_100BASE_TX_FD    = (uint16_t)1 << 8,
        ADV_100BASE_TX       = (uint16_t)1 << 7,
        ADV_10BASE_T_FD      = (uint16_t)1 << 6,
        ADV_10BASE_T         = (uint16_t)1 << 5
    }; };
    
public:
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        o->timer.init(c, APRINTER_CB_STATFUNC_T(&GenericPhy::timer_handler));
        o->state = State::IDLE;
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        
        o->timer.deinit(c);
    }
    
    static void reset (Context c)
    {
        auto *o = Object::self(c);
        
        o->timer.unset(c);
        o->state = State::IDLE;
    }
    
    static void activate (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->state == State::IDLE)
        
        go_resetting(c);
    }
    
    static void phyMaintCompleted (Context c, MiiPhyMaintResult result)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->state != State::IDLE)
        
        if (result.error) {
            return handle_error(c);
        }
        
        switch (o->state) {
            case State::RESET_WRITE: {
                o->state = State::RESET_WAIT;
                set_timer_after(c, ResetWaitTicks);
            } break;
            
            case State::INIT_READ_STATUS: {
                if (!(result.data & StatusReg::ANEG_ABILITY) || !(result.data & StatusReg::EXTENDED_CAP)) {
                    return handle_error(c);
                }
                
                o->initial_status = result.data;
                
                o->state = State::INIT_READ_ANEG_ADV;
                PhyRequester::startPhyMaintenance(c, MiiPhyMaintCommand{PhyAddr, RegNum::ANEG_ADV, PhyMaintCommandIoType::READ_ONLY});
            } break;
            
            case State::INIT_READ_ANEG_ADV: {
                uint16_t advert = result.data;
                
                advert &= ~(AnegAdvReg::ADV_10BASE_T|AnegAdvReg::ADV_10BASE_T_FD|
                            AnegAdvReg::ADV_100BASE_TX|AnegAdvReg::ADV_100BASE_TX_FD|
                            AnegAdvReg::ADV_PAUSE_FD|AnegAdvReg::ADV_ASYM_PAUSE_FD|
                            AnegAdvReg::ADV_100BASE_T4);
                
                uint16_t adv_speeds = 0;
                
                if ((o->initial_status & StatusReg::SUPPORT_10MB_HD) && ((SupportedSpeeds & MiiSpeed::SPEED_10M_HD))) {
                    adv_speeds |= AnegAdvReg::ADV_10BASE_T;
                }
                if ((o->initial_status & StatusReg::SUPPORT_10MB_FD) && ((SupportedSpeeds & MiiSpeed::SPEED_10M_FD))) {
                    adv_speeds |= AnegAdvReg::ADV_10BASE_T_FD;
                }
                if ((o->initial_status & StatusReg::SUPPORT_100BASE_X_HD) && ((SupportedSpeeds & MiiSpeed::SPEED_100M_HD))) {
                    adv_speeds |= AnegAdvReg::ADV_100BASE_TX;
                }
                if ((o->initial_status & StatusReg::SUPPORT_100BASE_X_FD) && ((SupportedSpeeds & MiiSpeed::SPEED_100M_FD))) {
                    adv_speeds |= AnegAdvReg::ADV_100BASE_TX_FD;
                }
                
                if (adv_speeds == 0) {
                    return handle_error(c);
                }
                advert |= adv_speeds;
                
                if ((SupportedPause & MiiPauseAbility::RX_AND_TX) || (SupportedPause & MiiPauseAbility::RX_ONLY)) {
                    advert |= AnegAdvReg::ADV_PAUSE_FD;
                    if ((SupportedPause & MiiPauseAbility::RX_ONLY)) {
                        advert |= AnegAdvReg::ADV_ASYM_PAUSE_FD;
                    }
                }
                else if ((SupportedPause & MiiPauseAbility::TX_ONLY)) {
                    advert |= AnegAdvReg::ADV_ASYM_PAUSE_FD;
                }
                
                o->advert = advert;
                o->restart_aneg_on_init = advert != result.data;
                
                o->state = State::INIT_WRITE_ANEG_ADV;
                PhyRequester::startPhyMaintenance(c, MiiPhyMaintCommand{PhyAddr, RegNum::ANEG_ADV, PhyMaintCommandIoType::READ_WRITE, advert});
            } break;
            
            case State::INIT_WRITE_ANEG_ADV: {
                o->state = State::INIT_READ_CONTROL;
                read_control(c);
            } break;
            
            case State::INIT_READ_CONTROL: {
                uint16_t control = adjust_control_for_aneg(result.data, o->restart_aneg_on_init);
                o->state = State::INIT_WRITE_CONTROL;
                write_control(c, control);
            } break;
            
            case State::INIT_WRITE_CONTROL: {
                go_waiting_link(c);
            } break;
            
            case State::LINK_DOWN_READ_STATUS: {
                if (!(result.data & StatusReg::LINK_STATUS)) {
                    o->state = State::LINK_DOWN_WAIT;
                    o->timer.appendAfterPrevious(c, StatusPollTicks);
                    return;
                }
                
                go_waiting_aneg(c);
            } break;
            
            case State::ANEG_READ_STATUS: {
                if (!(result.data & StatusReg::LINK_STATUS)) {
                    o->state = State::LINK_DOWN_WAIT;
                    o->timer.appendAfterPrevious(c, StatusPollTicks);
                    return;
                }
                
                if (!(result.data & StatusReg::ANEG_COMPLETE)) {
                    if (o->counter >= AutoNegMaxPolls) {
                        o->state = State::ANEG_READ_CONTROL;
                        read_control(c);
                        return;
                    }
                    o->state = State::ANEG_WAIT;
                    o->counter++;
                    o->timer.appendAfterPrevious(c, StatusPollTicks);
                    return;
                }
                
                o->state = State::ANEG_READ_LP_ABILITY;
                PhyRequester::startPhyMaintenance(c, MiiPhyMaintCommand{PhyAddr, RegNum::ANEG_LP_ABILITY, PhyMaintCommandIoType::READ_ONLY});
            } break;
            
            case State::ANEG_READ_CONTROL: {
                uint16_t control = adjust_control_for_aneg(result.data, true);
                o->state = State::ANEG_WRITE_CONTROL;
                write_control(c, control);
            } break;
            
            case State::ANEG_WRITE_CONTROL: {
                go_waiting_aneg(c);
            } break;
            
            case State::ANEG_READ_LP_ABILITY: {
                uint16_t common_advert = o->advert & result.data;
                
                uint8_t speed;
                if ((common_advert & AnegAdvReg::ADV_100BASE_TX_FD)) {
                    speed = MiiSpeed::SPEED_100M_FD;
                }
                else if ((common_advert & AnegAdvReg::ADV_100BASE_TX)) {
                    speed = MiiSpeed::SPEED_100M_HD;
                }
                else if ((common_advert & AnegAdvReg::ADV_10BASE_T_FD)) {
                    speed = MiiSpeed::SPEED_10M_FD;
                }
                else {
                    speed = MiiSpeed::SPEED_10M_HD;
                }
                
                uint8_t pause_config = 0;
                
                if (speed == MiiSpeed::SPEED_100M_FD || speed == MiiSpeed::SPEED_10M_FD) {
                    uint8_t pause_table_bits =
                        (bool(o->advert   & AnegAdvReg::ADV_PAUSE_FD     ) << 3) |
                        (bool(o->advert   & AnegAdvReg::ADV_ASYM_PAUSE_FD) << 2) |
                        (bool(result.data & AnegAdvReg::ADV_PAUSE_FD     ) << 1) |
                        (bool(result.data & AnegAdvReg::ADV_ASYM_PAUSE_FD) << 0);
                    
                    switch (pause_table_bits) {
                        case 0b0111:
                            pause_config = MiiPauseConfig::TX_ENABLE;
                            break;
                        case 0b1010:
                        case 0b1011:
                        case 0b1110:
                        case 0b1111:
                            pause_config = MiiPauseConfig::TX_ENABLE|MiiPauseConfig::RX_ENABLE;
                            break;
                        case 0b1101:
                            pause_config = MiiPauseConfig::RX_ENABLE;
                            break;
                    }
                }
                
                o->state = State::LINK_UP_WAIT;
                o->timer.appendNowNotAlready(c);
                
                return PhyRequester::linkIsUp(c, MiiLinkParams{speed, pause_config});
            } break;
            
            case State::LINK_UP_READ_STATUS: {
                if ((result.data & StatusReg::LINK_STATUS)) {
                    o->state = State::LINK_UP_WAIT;
                    o->timer.appendAfterPrevious(c, StatusPollTicks);
                    return;
                }
                
                go_waiting_link(c);
                
                return PhyRequester::linkIsDown(c);
            } break;
            
            default: AMBRO_ASSERT(false);
        }
    }
    
private:
    static void set_timer_after (Context c, TimeType after_ticks)
    {
        auto *o = Object::self(c);
        o->timer.appendAt(c, (TimeType)(Context::Clock::getTime(c) + after_ticks));
    }
    
    static void read_control (Context c)
    {
        PhyRequester::startPhyMaintenance(c, MiiPhyMaintCommand{PhyAddr, RegNum::CONTROL, PhyMaintCommandIoType::READ_ONLY});
    }
    
    static uint16_t adjust_control_for_aneg (uint16_t value, bool restart_aneg)
    {
        value &= ~(ControlReg::ISOLATE);
        value |= ControlReg::ANEG_ENABLE;
        if (restart_aneg) {
            value |= ControlReg::RESTART_ANEG;
        }
        return value;
    }
    
    static void write_control (Context c, uint16_t value)
    {
        PhyRequester::startPhyMaintenance(c, MiiPhyMaintCommand{PhyAddr, RegNum::CONTROL, PhyMaintCommandIoType::READ_WRITE, value});
    }
    
    static void read_status (Context c)
    {
        PhyRequester::startPhyMaintenance(c, MiiPhyMaintCommand{PhyAddr, RegNum::STATUS, PhyMaintCommandIoType::READ_ONLY});
    }
    
    static void timer_handler (Context c)
    {
        auto *o = Object::self(c);
        
        switch (o->state) {
            case State::WAIT_TO_RESET: {
                go_resetting(c);
            } break;
            
            case State::RESET_WAIT: {
                o->state = State::INIT_READ_STATUS;
                read_status(c);
            } break;
            
            case State::LINK_DOWN_WAIT: {
                o->state = State::LINK_DOWN_READ_STATUS;
                read_status(c);
            } break;
            
            case State::ANEG_WAIT: {
                o->state = State::ANEG_READ_STATUS;
                read_status(c);
            } break;
            
            case State::LINK_UP_WAIT: {
                o->state = State::LINK_UP_READ_STATUS;
                read_status(c);
            } break;
            
            default: AMBRO_ASSERT(false);
        }
    }
    
    static void handle_error (Context c)
    {
        auto *o = Object::self(c);
        
        bool call_link_down = (o->state == State::LINK_UP_WAIT || o->state == State::LINK_UP_READ_STATUS);
        
        o->state = State::WAIT_TO_RESET;
        set_timer_after(c, BeforeResetTicks);
        
        if (call_link_down) {
            return PhyRequester::linkIsDown(c);
        }
    }
    
    static void go_resetting (Context c)
    {
        auto *o = Object::self(c);
        
        o->state = State::RESET_WRITE;
        write_control(c, ControlReg::RESET);
    }
    
    static void go_waiting_link (Context c)
    {
        auto *o = Object::self(c);
        
        o->state = State::LINK_DOWN_WAIT;
        o->timer.appendNowNotAlready(c);
    }
    
    static void go_waiting_aneg (Context c)
    {
        auto *o = Object::self(c);
        
        o->state = State::ANEG_WAIT;
        o->counter = 0;
        o->timer.appendNowNotAlready(c);
    }
    
public:
    struct Object : public ObjBase<GenericPhy, ParentObject, EmptyTypeList> {
        typename Context::EventLoop::TimedEvent timer;
        State state;
        bool restart_aneg_on_init;
        uint16_t initial_status;
        uint16_t advert;
        uint16_t counter;
    };
};

template <
    bool TRmii,
    uint8_t TPhyAddr
>
struct GenericPhyService {
    static bool const Rmii = TRmii;
    static uint8_t const PhyAddr = TPhyAddr;
    
    template <typename Context, typename ParentObject, typename ClientParams>
    using Phy = GenericPhy<Context, ParentObject, ClientParams, GenericPhyService>;
};

#include <aprinter/EndNamespace.h>

#endif
