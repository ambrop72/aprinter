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

#ifndef AMBROLIB_AXIS_HOMER_H
#define AMBROLIB_AXIS_HOMER_H

#include <stdint.h>

#include <aprinter/meta/WrapFunction.h>
#include <aprinter/meta/TupleGet.h>
#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Hints.h>
#include <aprinter/meta/MinMax.h>
#include <aprinter/meta/BasicMetaUtils.h>
#include <aprinter/meta/ExprFixedPoint.h>
#include <aprinter/meta/TypeListUtils.h>
#include <aprinter/printer/planning/MotionPlanner.h>
#include <aprinter/printer/Configuration.h>

namespace APrinter {

template <typename Arg>
class AxisHomerGlobal {
    using Context        = typename Arg::GeneralParams::Context;
    using ThePrinterMain = typename Arg::GeneralParams::ThePrinterMain;
    using Params         = typename Arg::Params;
    
    using Config = typename ThePrinterMain::Config;
    using CSwitchInvert = decltype(ExprCast<bool>(Config::e(Params::SwitchInvert::i())));
    
public:
    static void init (Context c)
    {
        Context::Pins::template setInput<typename Params::SwitchPin, typename Params::SwitchPinInputMode>(c);
    }
    
    template <typename ThisContext>
    AMBRO_ALWAYS_INLINE
    static bool endstop_is_triggered (ThisContext c)
    {
        return (Context::Pins::template get<typename Params::SwitchPin>(c) != APRINTER_CFG(Config, CSwitchInvert, c));
    }
    
    struct Object {};
    
    using ConfigExprs = MakeTypeList<CSwitchInvert>;
};

template <typename Arg>
class AxisHomer {
    using ParentObject    = typename Arg::ParentObject;
    using TheGlobal       = typename Arg::TheGlobal;
    using TheAxisDriver   = typename Arg::TheAxisDriver;
    using FinishedHandler = typename Arg::FinishedHandler;
    using Params          = typename Arg::Params;
    using GeneralParams   = typename Arg::GeneralParams;
    
    using Context          = typename GeneralParams::Context;
    using ThePrinterMain   = typename GeneralParams::ThePrinterMain;
    using MaxStepsPerCycle = typename GeneralParams::MaxStepsPerCycle;
    using MaxAccel         = typename GeneralParams::MaxAccel;
    using DistConversion   = typename GeneralParams::DistConversion;
    using TimeConversion   = typename GeneralParams::TimeConversion;
    using HomeDir          = typename GeneralParams::HomeDir;
    static int const PlannerStepBits          = GeneralParams::PlannerStepBits;
    static int const StepperSegmentBufferSize = GeneralParams::StepperSegmentBufferSize;
    static int const MaxLookaheadBufferSize   = GeneralParams::MaxLookaheadBufferSize;
    
public:
    struct Object;
    
private:
    using Config = typename ThePrinterMain::Config;
    using FpType = typename ThePrinterMain::FpType;
    using TheCommand = typename ThePrinterMain::TheCommand;
    
    struct PlannerPullHandler;
    struct PlannerFinishedHandler;
    struct PlannerAbortedHandler;
    struct PlannerUnderrunCallback;
    struct PlannerPrestepCallback;
    
    static int const LookaheadBufferSize = MinValue(MaxLookaheadBufferSize, 3);
    static int const LookaheadCommitCount = 1;
    
    using SpeedConversion = decltype(DistConversion() / TimeConversion());
    using AccelConversion = decltype(DistConversion() / (TimeConversion() * TimeConversion()));
    
    using FastSteps = decltype(Config::e(Params::FastMaxDist::i()) * DistConversion());
    using RetractSteps = decltype(Config::e(Params::RetractDist::i()) * DistConversion());
    using SlowSteps = decltype(Config::e(Params::SlowMaxDist::i()) * DistConversion());
    
    using PlannerMaxSpeedRec = APRINTER_FP_CONST_EXPR(0.0);
    using PlannerMaxAccelRec = decltype(ExprRec(MaxAccel() * AccelConversion()));
    using PlannerDistanceFactor = APRINTER_FP_CONST_EXPR(1.0);
    using PlannerCorneringDistance = APRINTER_FP_CONST_EXPR(1.0);
    
    struct PlannerAxisSpec : public MotionPlannerAxisSpec<TheAxisDriver, PlannerStepBits, PlannerDistanceFactor, PlannerCorneringDistance, PlannerMaxSpeedRec, PlannerMaxAccelRec, PlannerPrestepCallback> {};
    using PlannerAxes = MakeTypeList<PlannerAxisSpec>;
    APRINTER_MAKE_INSTANCE(Planner, (MotionPlannerArg<Context, Object, Config, PlannerAxes, StepperSegmentBufferSize, LookaheadBufferSize, LookaheadCommitCount, FpType, MaxStepsPerCycle, PlannerPullHandler, PlannerFinishedHandler, PlannerAbortedHandler, PlannerUnderrunCallback, EmptyTypeList, EmptyTypeList>))
    using PlannerCommand = typename Planner::SplitBuffer;
    
    using TheDebugObject = DebugObject<Context, Object>;
    
public:
    using StepFixedType = typename Planner::template Axis<0>::StepFixedType;
    
private:
    using CMaxVRecFast = decltype(ExprCast<FpType>(ExprRec(Config::e(Params::FastSpeed::i()) * SpeedConversion())));
    using CMaxVRecRetract = decltype(ExprCast<FpType>(ExprRec(Config::e(Params::RetractSpeed::i()) * SpeedConversion())));
    using CMaxVRecSlow = decltype(ExprCast<FpType>(ExprRec(Config::e(Params::SlowSpeed::i()) * SpeedConversion())));
    using CFixedStepsFast = decltype(ExprFixedPointImport<StepFixedType>(FastSteps()));
    using CFixedStepsRetract = decltype(ExprFixedPointImport<StepFixedType>(RetractSteps()));
    using CFixedStepsSlow = decltype(ExprFixedPointImport<StepFixedType>(SlowSteps()));
    using CHomeDir = decltype(ExprCast<bool>(HomeDir()));
    
    enum {STATE_FAST, STATE_RETRACT, STATE_SLOW, STATE_END};
    
public:
    static void init (Context c, TheCommand *err_output)
    {
        auto *o = Object::self(c);
        
        Planner::init(c, true);
        o->m_state = STATE_FAST;
        o->m_command_sent = false;
        o->m_err_output = err_output;
        
        TheDebugObject::init(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::deinit(c);
        
        if (o->m_state != STATE_END) {
            Planner::deinit(c);
        }
    }
    
    using TheAxisDriverConsumer = typename Planner::template TheAxisDriverConsumer<0>;
    
private:
    static void planner_pull_handler (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->m_state != STATE_END)
        
        if (o->m_command_sent) {
            Planner::waitFinished(c);
            return;
        }
        
        PlannerCommand *cmd = Planner::getBuffer(c);
        auto *axis_cmd = TupleGetElem<0>(cmd->axes.axes());
        FpType max_v_rec;
        switch (o->m_state) {
            case STATE_FAST: {
                axis_cmd->dir = APRINTER_CFG(Config, CHomeDir, c);
                axis_cmd->x = APRINTER_CFG(Config, CFixedStepsFast, c);
                max_v_rec = APRINTER_CFG(Config, CMaxVRecFast, c);
            } break;
            case STATE_RETRACT: {
                axis_cmd->dir = !APRINTER_CFG(Config, CHomeDir, c);
                axis_cmd->x = APRINTER_CFG(Config, CFixedStepsRetract, c);
                max_v_rec = APRINTER_CFG(Config, CMaxVRecRetract, c);
            } break;
            case STATE_SLOW: {
                axis_cmd->dir = APRINTER_CFG(Config, CHomeDir, c);
                axis_cmd->x = APRINTER_CFG(Config, CFixedStepsSlow, c);
                max_v_rec = APRINTER_CFG(Config, CMaxVRecSlow, c);
            } break;
        }
        cmd->axes.rel_max_v_rec = axis_cmd->x.template fpValue<FpType>() * max_v_rec;
        
        if (axis_cmd->x.bitsValue() != 0) {
            Planner::axesCommandDone(c);
        } else {
            Planner::emptyDone(c);
        }
        o->m_command_sent = true;
    }
    struct PlannerPullHandler : public AMBRO_WFUNC_TD(&AxisHomer::planner_pull_handler) {};
    
    static void planner_finished_handler (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->m_state != STATE_END)
        AMBRO_ASSERT(o->m_command_sent)
        
        Planner::deinit(c);
        
        if (o->m_state != STATE_RETRACT) {
            return complete_with_error(c, AMBRO_PSTR("EndstopNotTriggered"));
        }
        
        if (TheGlobal::endstop_is_triggered(c)) {
            return complete_with_error(c, AMBRO_PSTR("EndstopTriggeredAfterRetract"));
        }
        
        o->m_state++;
        Planner::init(c, true);
        o->m_command_sent = false;
    }
    struct PlannerFinishedHandler : public AMBRO_WFUNC_TD(&AxisHomer::planner_finished_handler) {};
    
    static void planner_aborted_handler (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(o->m_state == STATE_FAST || o->m_state == STATE_SLOW)
        
        Planner::deinit(c);
        o->m_state++;
        
        if (o->m_state == STATE_END) {
            return FinishedHandler::call(c, true);
        }
        
        Planner::init(c, o->m_state != STATE_RETRACT);
        o->m_command_sent = false;
    }
    struct PlannerAbortedHandler : public AMBRO_WFUNC_TD(&AxisHomer::planner_aborted_handler) {};
    
    static void planner_underrun_callback (Context c)
    {
    }
    struct PlannerUnderrunCallback : public AMBRO_WFUNC_TD(&AxisHomer::planner_underrun_callback) {};
    
    AMBRO_ALWAYS_INLINE
    static bool planner_prestep_callback (typename Planner::template Axis<0>::StepperCommandCallbackContext c)
    {
        return TheGlobal::endstop_is_triggered(c);
    }
    struct PlannerPrestepCallback : public AMBRO_WFUNC_TD(&AxisHomer::planner_prestep_callback) {};
    
    static void complete_with_error (Context c, AMBRO_PGM_P errstr)
    {
        auto *o = Object::self(c);
        o->m_err_output->reply_append_error(c, errstr);
        o->m_err_output->reply_poke(c);
        o->m_state = STATE_END;
        return FinishedHandler::call(c, false);
    }
    
public:
    using ConfigExprs = MakeTypeList<CMaxVRecFast, CMaxVRecRetract, CMaxVRecSlow, CFixedStepsFast, CFixedStepsRetract, CFixedStepsSlow, CHomeDir>;
    
    struct Object : public ObjBase<AxisHomer, ParentObject, MakeTypeList<
        TheDebugObject,
        Planner
    >> {
        uint8_t m_state;
        bool m_command_sent;
        TheCommand *m_err_output;
    };
};

APRINTER_ALIAS_STRUCT_EXT(AxisHomerService, (
    APRINTER_AS_TYPE(SwitchPin),
    APRINTER_AS_TYPE(SwitchPinInputMode),
    APRINTER_AS_TYPE(SwitchInvert),
    APRINTER_AS_TYPE(FastMaxDist),
    APRINTER_AS_TYPE(RetractDist),
    APRINTER_AS_TYPE(SlowMaxDist),
    APRINTER_AS_TYPE(FastSpeed),
    APRINTER_AS_TYPE(RetractSpeed),
    APRINTER_AS_TYPE(SlowSpeed)
), (
    APRINTER_ALIAS_STRUCT_EXT(HomerGeneral, (
        APRINTER_AS_TYPE(Context),
        APRINTER_AS_TYPE(ThePrinterMain),
        APRINTER_AS_VALUE(int, PlannerStepBits),
        APRINTER_AS_VALUE(int, StepperSegmentBufferSize),
        APRINTER_AS_VALUE(int, MaxLookaheadBufferSize),
        APRINTER_AS_TYPE(MaxStepsPerCycle),
        APRINTER_AS_TYPE(MaxAccel),
        APRINTER_AS_TYPE(DistConversion),
        APRINTER_AS_TYPE(TimeConversion),
        APRINTER_AS_TYPE(HomeDir)
    ), (
        APRINTER_ALIAS_STRUCT_EXT(HomerGlobal, (
            APRINTER_AS_TYPE(ParentObject)
        ), (
            using Params = AxisHomerService;
            using GeneralParams = HomerGeneral;
            APRINTER_DEF_INSTANCE(HomerGlobal, AxisHomerGlobal)
        ))
        
        APRINTER_ALIAS_STRUCT_EXT(Homer, (
            APRINTER_AS_TYPE(ParentObject),
            APRINTER_AS_TYPE(TheGlobal),
            APRINTER_AS_TYPE(TheAxisDriver),
            APRINTER_AS_TYPE(FinishedHandler)
        ), (
            using Params = AxisHomerService;
            using GeneralParams = HomerGeneral;
            APRINTER_DEF_INSTANCE(Homer, AxisHomer)
        ))
    ))
))

}

#endif
