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

#ifndef APRINTER_LINEAR_DUTY_FORMULA_H
#define APRINTER_LINEAR_DUTY_FORMULA_H

#include <aprinter/meta/FixedPoint.h>
#include <aprinter/meta/BitsInInt.h>

#include <aprinter/BeginNamespace.h>

template <typename DutyCycleType, DutyCycleType MaxDutyCycle, int PowerBits>
class LinearDutyFormula {
public:
    using PowerFixedType = FixedPoint<(PowerBits + 1), false, -PowerBits>;
    
    static DutyCycleType powerToDuty (PowerFixedType power)
    {
        auto res = FixedResMultiply(power, Factor);
        return (res.m_bits.m_int > MaxDutyCycle) ? MaxDutyCycle : res.m_bits.m_int;
    }
    
private:
    static int const FactorBits = BitsInInt<MaxDutyCycle>::Value;
    using FactorFixedType = FixedPoint<FactorBits, false, 0>;
    static constexpr FactorFixedType Factor = FactorFixedType::importBitsConstexpr(MaxDutyCycle);
};

template <int PowerBits>
struct LinearDutyFormulaService {
    template <typename DutyCycleType, DutyCycleType MaxDutyCycle>
    using DutyFormula = LinearDutyFormula<DutyCycleType, MaxDutyCycle, PowerBits>;
};

#include <aprinter/EndNamespace.h>

#endif
