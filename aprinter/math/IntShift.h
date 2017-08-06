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

#ifndef AMBROLIB_INT_SHIFT_H
#define AMBROLIB_INT_SHIFT_H

#include <stdint.h>

#include <aprinter/meta/ChooseInt.h>
#include <aprinter/meta/PowerOfTwo.h>

namespace APrinter {

template <int NumBits, bool Signed, int ShiftCount>
class IntShiftRight {
public:
    static_assert(ShiftCount >= 0, "");
    typedef ChooseInt<NumBits, Signed> OpType;
    typedef ChooseInt<NumBits - ShiftCount, Signed> ResType;
    
    template <typename Option = int>
    __attribute__((always_inline)) inline static ResType call (OpType op)
    {
        return (op / PowerOfTwo<OpType, ShiftCount>::Value);
    }
};

template <int NumBits, bool Signed, int ShiftCount>
class IntShiftLeft {
public:
    static_assert(ShiftCount >= 0, "");
    typedef ChooseInt<NumBits, Signed> OpType;
    static const int ResBits = NumBits + ShiftCount;
    typedef ChooseInt<ResBits, Signed> ResType;
    
    template <typename Option = int>
    __attribute__((always_inline)) inline static ResType call (OpType op)
    {
        return (op * PowerOfTwo<ResType, ShiftCount>::Value);
    }
};

template <int NumBits, bool Signed, int ShiftCount>
class IntUndoShiftLeft {
public:
    static_assert(ShiftCount >= 0, "");
    typedef ChooseInt<NumBits, Signed> OpType;
    typedef ChooseInt<NumBits - ShiftCount, Signed> ResType;
    
    template <typename Option = int>
    __attribute__((always_inline)) inline static ResType call (OpType op)
    {
        // WARNING relying on implementation defined semantics
        // of right-shifting negative integers
        return (op >> ShiftCount);
    }
};

}

#endif
