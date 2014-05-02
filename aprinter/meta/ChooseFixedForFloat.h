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

#ifndef AMBROLIB_CHOOSE_FIXED_FOR_FLOAT_H
#define AMBROLIB_CHOOSE_FIXED_FOR_FLOAT_H

#include <aprinter/meta/BitsInFloat.h>
#include <aprinter/meta/FixedPoint.h>
#include <aprinter/meta/MinMax.h>

#include <aprinter/BeginNamespace.h>

template <int Bits, typename FloatValue1>
using ChooseFixedForFloat = FixedPoint<Bits, (FloatValue1::value() < 0), (BitsInFloat(absolute(FloatValue1::value())) - Bits)>;

template <int Bits, typename FloatValue1, typename FloatValue2>
using ChooseFixedForFloatTwo = FixedPoint<Bits, (FloatValue1::value() < 0 || FloatValue2::value() < 0), (BitsInFloat(max(absolute(FloatValue1::value()), absolute(FloatValue2::value()))) - Bits)>;

#include <aprinter/EndNamespace.h>

#endif
