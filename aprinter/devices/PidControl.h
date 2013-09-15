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
#include <aprinter/meta/ChooseFixedForFloat.h>
#include <aprinter/meta/WrapDouble.h>
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

template <typename Params, typename MeasurementInterval, typename ValueFixedType>
class PidControl {
public:
    using OutputFixedType = FixedPoint<8, false, -8>;
    
private:
    static const int PBits = 14;
    static const int IntervalIBits = 16;
    static const int IntegralBits = 15;
    static const int DHistoryBits = 16;
    static const int DNewBits = 16;
    static const int DerivativeBits = 15;
    
    using IntervalI = AMBRO_WRAP_DOUBLE(MeasurementInterval::value() * Params::I::value());
    using DNew = AMBRO_WRAP_DOUBLE((1.0 - Params::DHistory::value()) * Params::D::value() / MeasurementInterval::value());
    
    using PFixedType = ChooseFixedForFloat<PBits, typename Params::P>;
    using IntervalIFixedType = ChooseFixedForFloat<IntervalIBits, IntervalI>;
    using IntegralFixedType = ChooseFixedForFloatTwo<IntegralBits, typename Params::IStateMin, typename Params::IStateMax>;
    using DHistoryFixedType = ChooseFixedForFloat<DHistoryBits, typename Params::DHistory>;
    using DNewFixedType = ChooseFixedForFloat<DNewBits, DNew>;
    using DerivativeFixedType = decltype((DNewFixedType() * (ValueFixedType() - ValueFixedType())).template bitsDown<DerivativeBits>());
    
public:
    void init (ValueFixedType target)
    {
        m_first = true;
        m_target = target;
        m_integral = IntegralFixedType::importBits(0);
        m_derivative = DerivativeFixedType::importBits(0);
    }
    
    void setTarget (ValueFixedType target)
    {
        m_target = target;
    }
    
    OutputFixedType addMeasurement (ValueFixedType value)
    {
        auto err = m_target - value;
        if (AMBRO_LIKELY(!m_first)) {
            auto d_old_part = FixedResMultiply<DerivativeFixedType::exp>(m_derivative, DHistoryFixedType::importDoubleSaturated(Params::DHistory::value()));
            auto d_delta = m_last - value;
            auto d_new_part = FixedResMultiply<DerivativeFixedType::exp>(d_delta, DNewFixedType::importDoubleSaturated(DNew::value())); 
            auto derivative_new = d_old_part + d_new_part;
            m_derivative = derivative_new.template dropBitsSaturated<DerivativeFixedType::num_bits>();
            auto integral_add = FixedResMultiply<IntegralFixedType::exp>(err, IntervalIFixedType::importDoubleSaturated(IntervalI::value()));
            auto integral_new = (m_integral + integral_add);
            m_integral = FixedMin(IntegralFixedType::importDoubleSaturated(Params::IStateMax::value()), FixedMax(IntegralFixedType::importDoubleSaturated(Params::IStateMin::value()), integral_new));
        }
        m_first = false;
        m_last = value;
        static const int SumExp = OutputFixedType::exp - 2;
        auto p_part = FixedResMultiply<SumExp>(err, PFixedType::importDoubleSaturated(Params::P::value()));
        auto i_part = m_integral.template shiftBits<SumExp - IntegralFixedType::exp>();
        auto d_part = m_derivative.template shiftBits<SumExp - DerivativeFixedType::exp>();
        auto result = p_part + i_part + d_part;
        return result.template shiftBits<OutputFixedType::exp - SumExp>().template dropBitsSaturated<OutputFixedType::num_bits, OutputFixedType::is_signed>();
    }
    
private:
    bool m_first;
    ValueFixedType m_target;
    ValueFixedType m_last;
    IntegralFixedType m_integral;
    DerivativeFixedType m_derivative;
};

#include <aprinter/EndNamespace.h>

#endif
