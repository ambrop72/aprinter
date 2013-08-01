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

#include <aprinter/base/Likely.h>

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

template <typename Params, typename MeasurementInterval>
class PidControl {
public:
    void init (double target)
    {
        m_first = true;
        m_target = target;
        m_integral = 0.0;
        m_derivative = 0.0;
    }
    
    void setTarget (double target)
    {
        m_target = target;
    }
    
    double addMeasurement (double value)
    {
        double err = m_target - value;
        if (AMBRO_LIKELY(!m_first)) {
            m_integral += (MeasurementInterval::value() * Params::I::value()) * err;
            m_integral = fmax(Params::IStateMin::value(), fmin(Params::IStateMax::value(), m_integral));
            m_derivative = (Params::DHistory::value() * m_derivative) + (((1.0 - Params::DHistory::value()) * Params::D::value() / MeasurementInterval::value()) * (m_last - value));
        }
        m_first = false;
        m_last = value;
        return (Params::P::value() * err) + m_integral + m_derivative;
    }
    
private:
    bool m_first;
    double m_target;
    double m_last;
    double m_integral;
    double m_derivative;
};

#include <aprinter/EndNamespace.h>

#endif
