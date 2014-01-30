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

#ifndef AMBROLIB_PID_CONTROL_H
#define AMBROLIB_PID_CONTROL_H

#include <aprinter/meta/FixedPoint.h>
#include <aprinter/base/Likely.h>
#include <aprinter/base/Inline.h>
#include <aprinter/base/ProgramMemory.h>

#include <aprinter/BeginNamespace.h>

template <
    typename TP, typename TI, typename TD, typename TIStateMin, typename TIStateMax,
    typename TDHistory
>
struct PidControlParams {
    using P = TP;
    using I = TI;
    using D = TD;
    using IStateMin = TIStateMin;
    using IStateMax = TIStateMax;
    using DHistory = TDHistory;
};

template <typename Params, typename MeasurementInterval, typename ValueFixedType, typename FpType>
class PidControl {
public:
    using OutputFixedType = FixedPoint<8, false, -8>;
    static const bool InterruptContextAllowed = false;
    static const bool SupportsConfig = true;
    
    struct Config {
        FpType p;
        FpType i;
        FpType d;
        FpType istatemin;
        FpType istatemax;
        FpType dhistory;
        FpType c5;
    };
    
    static Config makeDefaultConfig ()
    {
        return makeConfig((FpType)Params::P::value(), (FpType)Params::I::value(), (FpType)Params::D::value(), (FpType)Params::IStateMin::value(), (FpType)Params::IStateMax::value(), (FpType)Params::DHistory::value());
    }
    
    template <typename Context, typename TheChannelCommon>
    static void setConfigCommand (Context c, TheChannelCommon *cc, Config *config)
    {
        *config = makeConfig(
            cc->get_command_param_fp(c, 'P', config->p),
            cc->get_command_param_fp(c, 'I', config->i),
            cc->get_command_param_fp(c, 'D', config->d),
            cc->get_command_param_fp(c, 'M', config->istatemin),
            cc->get_command_param_fp(c, 'A', config->istatemax),
            cc->get_command_param_fp(c, 'H', config->dhistory)
        );
    }
    
    template <typename Context, typename TheChannelCommon>
    static void printConfig (Context c, TheChannelCommon *cc, Config const *config)
    {
        cc->reply_append_pstr(c, AMBRO_PSTR(" P"));
        cc->reply_append_fp(c, config->p);
        cc->reply_append_pstr(c, AMBRO_PSTR(" I"));
        cc->reply_append_fp(c, config->i);
        cc->reply_append_pstr(c, AMBRO_PSTR(" D"));
        cc->reply_append_fp(c, config->d);
        cc->reply_append_pstr(c, AMBRO_PSTR(" M"));
        cc->reply_append_fp(c, config->istatemin);
        cc->reply_append_pstr(c, AMBRO_PSTR(" A"));
        cc->reply_append_fp(c, config->istatemax);
        cc->reply_append_pstr(c, AMBRO_PSTR(" H"));
        cc->reply_append_fp(c, config->dhistory);
    }
    
    void init ()
    {
        m_first = true;
        m_integral = 0.0f;
        m_derivative = 0.0f;
    }
    
    AMBRO_ALWAYS_INLINE
    OutputFixedType addMeasurement (ValueFixedType value, ValueFixedType target, Config const *config)
    {
        FpType value_double = value.template fpValue<FpType>();
        FpType err = target.template fpValue<FpType>() - value_double;
        if (AMBRO_LIKELY(!m_first)) {
            m_integral += ((FpType)MeasurementInterval::value() * config->i) * err;
            m_integral = FloatMax(config->istatemin, FloatMin(config->istatemax, m_integral));
            m_derivative = (config->dhistory * m_derivative) + (config->c5 * (m_last - value_double));
        }
        m_first = false;
        m_last = value_double;
        return OutputFixedType::template importFpSaturatedRound<FpType>((config->p * err) + m_integral + m_derivative);
    }
    
private:
    static Config makeConfig (FpType p, FpType i, FpType d, FpType istatemin, FpType istatemax, FpType dhistory)
    {
        Config c;
        c.p = p;
        c.i = i;
        c.d = d;
        c.istatemin = istatemin;
        c.istatemax = istatemax;
        c.dhistory = dhistory;
        c.c5 = (1.0f - dhistory) * d * (FpType)(1.0 / MeasurementInterval::value());
        return c;
    }
    
    bool m_first;
    FpType m_last;
    FpType m_integral;
    FpType m_derivative;
};

#include <aprinter/EndNamespace.h>

#endif
