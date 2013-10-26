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

#include <math.h>

#include <aprinter/meta/FixedPoint.h>
#include <aprinter/base/Likely.h>
#include <aprinter/base/Inline.h>

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

template <typename Params, typename MeasurementInterval, typename ValueFixedType>
class PidControl {
public:
    using OutputFixedType = FixedPoint<8, false, -8>;
    static const bool InterruptContextAllowed = false;
    static const bool SupportsConfig = true;
    
    struct Config {
        double p;
        double i;
        double d;
        double istatemin;
        double istatemax;
        double dhistory;
        double c5;
    };
    
    static Config makeDefaultConfig ()
    {
        return makeConfig(Params::P::value(), Params::I::value(), Params::D::value(), Params::IStateMin::value(), Params::IStateMax::value(), Params::DHistory::value());
    }
    
    template <typename Context, typename Main>
    static void setConfigCommand (Context c, Main *m, Config *config)
    {
        *config = makeConfig(
            m->get_command_param_double('P', config->p),
            m->get_command_param_double('I', config->i),
            m->get_command_param_double('D', config->d),
            m->get_command_param_double('M', config->istatemin),
            m->get_command_param_double('A', config->istatemax),
            m->get_command_param_double('H', config->dhistory)
        );
    }
    
    template <typename Context, typename Main>
    static void printConfig (Context c, Main *m, Config const *config)
    {
        m->reply_append_str(c, " P");
        m->reply_append_double(c, config->p);
        m->reply_append_str(c, " I");
        m->reply_append_double(c, config->i);
        m->reply_append_str(c, " D");
        m->reply_append_double(c, config->d);
        m->reply_append_str(c, " M");
        m->reply_append_double(c, config->istatemin);
        m->reply_append_str(c, " A");
        m->reply_append_double(c, config->istatemax);
        m->reply_append_str(c, " H");
        m->reply_append_double(c, config->dhistory);
    }
    
    void init ()
    {
        m_first = true;
        m_integral = 0.0;
        m_derivative = 0.0;
    }
    
    AMBRO_ALWAYS_INLINE OutputFixedType addMeasurement (ValueFixedType value, ValueFixedType target, Config const *config)
    {
        double value_double = value.doubleValue();
        double err = target.doubleValue() - value_double;
        if (AMBRO_LIKELY(!m_first)) {
            m_integral += (MeasurementInterval::value() * config->i) * err;
            m_integral = fmax(config->istatemin, fmin(config->istatemax, m_integral));
            m_derivative = (config->dhistory * m_derivative) + (config->c5 * (m_last - value_double));
        }
        m_first = false;
        m_last = value_double;
        return OutputFixedType::importDoubleSaturated((config->p * err) + m_integral + m_derivative);
    }
    
private:
    static Config makeConfig (double p, double i, double d, double istatemin, double istatemax, double dhistory)
    {
        Config c;
        c.p = p;
        c.i = i;
        c.d = d;
        c.istatemin = istatemin;
        c.istatemax = istatemax;
        c.dhistory = dhistory;
        c.c5 = (1.0 - dhistory) * d / MeasurementInterval::value();
        return c;
    }
    
    bool m_first;
    double m_last;
    double m_integral;
    double m_derivative;
};

#include <aprinter/EndNamespace.h>

#endif
