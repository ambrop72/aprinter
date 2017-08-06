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

#ifndef AMBROLIB_AT91SAM3X_PWM_H
#define AMBROLIB_AT91SAM3X_PWM_H

#include <stdint.h>
#include <stddef.h>
#include <sam/drivers/pmc/pmc.h>

#include <aprinter/base/Object.h>
#include <aprinter/meta/TypeListUtils.h>
#include <aprinter/meta/BasicMetaUtils.h>
#include <aprinter/meta/ChooseInt.h>
#include <aprinter/meta/FuncUtils.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/hal/at91/At91SamPins.h>

namespace APrinter {

template <uint8_t TPreA, uint8_t TDivA, uint8_t TPreB, uint8_t TDivB>
struct At91Sam3xPwmParams {
    static uint8_t const PreA = TPreA;
    static uint8_t const DivA = TDivA;
    static uint8_t const PreB = TPreB;
    static uint8_t const DivB = TDivB;
};

template <int TChan, char TSignal, typename TPin, typename TPerip>
struct At91Sam3xPwmConnn {
    static int const Chan = TChan;
    static char const Signal = TSignal;
    using Pin = TPin;
    using Periph = TPerip;
};

using At91Sam3xPwmConnections = MakeTypeList<
    At91Sam3xPwmConnn<0, 'H', At91SamPin<At91SamPioA, 8>, At91SamPeriphB>,
    At91Sam3xPwmConnn<0, 'H', At91SamPin<At91SamPioB, 12>, At91SamPeriphB>,
    At91Sam3xPwmConnn<0, 'H', At91SamPin<At91SamPioC, 3>, At91SamPeriphB>,
//    At91Sam3xPwmConnn<0, 'H', At91SamPin<At91SamPioE, 15>, At91SamPeriphA>,
    At91Sam3xPwmConnn<1, 'H', At91SamPin<At91SamPioA, 19>, At91SamPeriphB>,
    At91Sam3xPwmConnn<1, 'H', At91SamPin<At91SamPioB, 13>, At91SamPeriphB>,
    At91Sam3xPwmConnn<1, 'H', At91SamPin<At91SamPioC, 5>, At91SamPeriphB>,
//    At91Sam3xPwmConnn<1, 'H', At91SamPin<At91SamPioE, 16>, At91SamPeriphA>,
    At91Sam3xPwmConnn<2, 'H', At91SamPin<At91SamPioA, 13>, At91SamPeriphB>,
    At91Sam3xPwmConnn<2, 'H', At91SamPin<At91SamPioB, 14>, At91SamPeriphB>,
    At91Sam3xPwmConnn<2, 'H', At91SamPin<At91SamPioC, 7>, At91SamPeriphB>,
    At91Sam3xPwmConnn<3, 'H', At91SamPin<At91SamPioA, 9>, At91SamPeriphB>,
    At91Sam3xPwmConnn<3, 'H', At91SamPin<At91SamPioB, 15>, At91SamPeriphB>,
    At91Sam3xPwmConnn<3, 'H', At91SamPin<At91SamPioC, 3>, At91SamPeriphB>,
//    At91Sam3xPwmConnn<3, 'H', At91SamPin<At91SamPioF, 3>, At91SamPeriphA>,
    At91Sam3xPwmConnn<4, 'H', At91SamPin<At91SamPioC, 20>, At91SamPeriphB>,
//    At91Sam3xPwmConnn<4, 'H', At91SamPin<At91SamPioE, 20>, At91SamPeriphA>,
    At91Sam3xPwmConnn<5, 'H', At91SamPin<At91SamPioC, 19>, At91SamPeriphB>,
//    At91Sam3xPwmConnn<5, 'H', At91SamPin<At91SamPioE, 22>, At91SamPeriphA>,
    At91Sam3xPwmConnn<6, 'H', At91SamPin<At91SamPioC, 18>, At91SamPeriphB>,
//    At91Sam3xPwmConnn<6, 'H', At91SamPin<At91SamPioE, 24>, At91SamPeriphA>,
//    At91Sam3xPwmConnn<7, 'H', At91SamPin<At91SamPioE, 26>, At91SamPeriphA>,
    At91Sam3xPwmConnn<0, 'L', At91SamPin<At91SamPioA, 21>, At91SamPeriphB>,
    At91Sam3xPwmConnn<0, 'L', At91SamPin<At91SamPioB, 16>, At91SamPeriphB>,
    At91Sam3xPwmConnn<0, 'L', At91SamPin<At91SamPioC, 2>, At91SamPeriphB>,
//    At91Sam3xPwmConnn<0, 'L', At91SamPin<At91SamPioE, 18>, At91SamPeriphA>,
    At91Sam3xPwmConnn<1, 'L', At91SamPin<At91SamPioA, 12>, At91SamPeriphB>,
    At91Sam3xPwmConnn<1, 'L', At91SamPin<At91SamPioB, 17>, At91SamPeriphB>,
    At91Sam3xPwmConnn<1, 'L', At91SamPin<At91SamPioC, 4>, At91SamPeriphB>,
    At91Sam3xPwmConnn<2, 'L', At91SamPin<At91SamPioA, 20>, At91SamPeriphB>,
    At91Sam3xPwmConnn<2, 'L', At91SamPin<At91SamPioB, 18>, At91SamPeriphB>,
    At91Sam3xPwmConnn<2, 'L', At91SamPin<At91SamPioC, 6>, At91SamPeriphB>,
//    At91Sam3xPwmConnn<2, 'L', At91SamPin<At91SamPioE, 17>, At91SamPeriphA>,
    At91Sam3xPwmConnn<3, 'L', At91SamPin<At91SamPioA, 0>, At91SamPeriphB>,
    At91Sam3xPwmConnn<3, 'L', At91SamPin<At91SamPioB, 19>, At91SamPeriphB>,
    At91Sam3xPwmConnn<3, 'L', At91SamPin<At91SamPioC, 8>, At91SamPeriphB>,
    At91Sam3xPwmConnn<4, 'L', At91SamPin<At91SamPioC, 21>, At91SamPeriphB>,
//    At91Sam3xPwmConnn<4, 'L', At91SamPin<At91SamPioE, 19>, At91SamPeriphA>,
    At91Sam3xPwmConnn<5, 'L', At91SamPin<At91SamPioC, 22>, At91SamPeriphB>,
//    At91Sam3xPwmConnn<5, 'L', At91SamPin<At91SamPioE, 21>, At91SamPeriphA>,
    At91Sam3xPwmConnn<6, 'L', At91SamPin<At91SamPioC, 23>, At91SamPeriphB>,
//    At91Sam3xPwmConnn<6, 'L', At91SamPin<At91SamPioE, 23>, At91SamPeriphA>,
    At91Sam3xPwmConnn<7, 'L', At91SamPin<At91SamPioC, 24>, At91SamPeriphB>
//    At91Sam3xPwmConnn<7, 'L', At91SamPin<At91SamPioE, 25>, At91SamPeriphA>
>;

template <typename, typename, typename>
class At91Sam3xPwmChannel;

template <typename Context, typename ParentObject, typename Params>
class At91Sam3xPwm {
public:
    struct Object;
    
private:
    template <typename, typename, typename>
    friend class At91Sam3xPwmChannel;
    
    using TheDebugObject = DebugObject<Context, Object>;
    
    static_assert(Params::PreA <= 10, "");
    static_assert(Params::PreB <= 10, "");
    
public:
    static void init (Context c)
    {
        pmc_enable_periph_clk(ID_PWM);
        
        PWM->PWM_DIS = UINT32_MAX;
        PWM->PWM_IDR1 = UINT32_MAX;
        PWM->PWM_IDR2 = UINT32_MAX;
        PWM->PWM_SCM = 0;
        PWM->PWM_CLK = PWM_CLK_PREA(Params::PreA) | PWM_CLK_DIVA(Params::DivA) | PWM_CLK_PREB(Params::PreB) | PWM_CLK_DIVB(Params::DivB);
        
        TheDebugObject::init(c);
    }
    
    static void deinit (Context c)
    {
        TheDebugObject::deinit(c);
        
        PWM->PWM_CLK = 0;
        
        pmc_disable_periph_clk(ID_PWM);
    }
    
public:
    struct Object : public ObjBase<At91Sam3xPwm, ParentObject, MakeTypeList<TheDebugObject>> {};
};

template <typename Context, typename ParentObject, typename Params>
class At91Sam3xPwmChannel {
public:
    struct Object;
    
private:
    using TheDebugObject = DebugObject<Context, Object>;
    using ThePwm = typename Context::Pwm;
    static int const ChannelNumber = Params::ChannelNumber;
    static_assert(ChannelNumber >= 0 && ChannelNumber < 8, "");
    static_assert(Params::ChannelPrescaler >= 0 && Params::ChannelPrescaler <= 12, "");
    static_assert(Params::ChannelPeriod <= UINT32_C(0xFFFFFF), "");
    
    template <typename Conn>
    using ConnFilterFunc = WrapBool<(Conn::Chan == ChannelNumber && Conn::Signal == Params::Signal && TypesAreEqual<typename Conn::Pin, typename Params::Pin>::Value)>;
    using ConnSearchRes = FilterTypeList<At91Sam3xPwmConnections, TemplateFunc<ConnFilterFunc>>;
    static_assert(TypeListLength<ConnSearchRes>::Value >= 1, "No connection found for desired PWM channel config!");
    static_assert(TypeListLength<ConnSearchRes>::Value <= 1, "");
    using Conn = TypeListGet<ConnSearchRes, 0>;
    
public:
    using DutyCycleType = ChooseIntForMax<Params::ChannelPeriod, false>;
    static DutyCycleType const MaxDutyCycle = Params::ChannelPeriod;
    
    static void init (Context c)
    {
        ThePwm::TheDebugObject::access(c);
        
        PWM->PWM_CH_NUM[ChannelNumber].PWM_CPRD = PWM_CPRD_CPRD(Params::ChannelPeriod);
        PWM->PWM_CH_NUM[ChannelNumber].PWM_CDTY = 0;
        PWM->PWM_CH_NUM[ChannelNumber].PWM_CMR = Params::ChannelPrescaler | ((Conn::Signal == 'H') != Params::Invert ? PWM_CMR_CPOL : 0);
        PWM->PWM_ENA = (uint32_t)1 << ChannelNumber;
        
        Context::Pins::template setPeripheral<typename Conn::Pin>(c, typename Conn::Periph());
        
        TheDebugObject::init(c);
    }
    
    static void deinit (Context c)
    {
        TheDebugObject::deinit(c);
        
        PWM->PWM_DIS = (uint32_t)1 << ChannelNumber;
    }
    
    template <typename ThisContext>
    static void setDutyCycle (ThisContext c, DutyCycleType duty_cycle)
    {
        AMBRO_ASSERT(duty_cycle <= MaxDutyCycle)
        
        PWM->PWM_CH_NUM[ChannelNumber].PWM_CDTYUPD = duty_cycle;
    }
    
    static void emergencySetOff ()
    {
        PWM->PWM_CH_NUM[ChannelNumber].PWM_CDTYUPD = 0;
    }
    
public:
    struct Object : public ObjBase<At91Sam3xPwmChannel, ParentObject, MakeTypeList<TheDebugObject>> {};
};

template <int TChannelPrescaler, uint32_t TChannelPeriod, int TChannelNumber, typename TPin, char TSignal, bool TInvert>
struct At91Sam3xPwmChannelService {
    static int const ChannelPrescaler = TChannelPrescaler;
    static uint32_t const ChannelPeriod = TChannelPeriod;
    static int const ChannelNumber = TChannelNumber;
    using Pin = TPin;
    static char const Signal = TSignal;
    static bool const Invert = TInvert;
    
    template <typename Context, typename ParentObject>
    using Pwm = At91Sam3xPwmChannel<Context, ParentObject, At91Sam3xPwmChannelService>;
};

}

#endif
