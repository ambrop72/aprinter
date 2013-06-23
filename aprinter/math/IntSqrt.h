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

#ifndef AMBROLIB_INT_SQRT_H
#define AMBROLIB_INT_SQRT_H

#include <stdint.h>

#include <aprinter/meta/ChooseInt.h>
#include <aprinter/meta/PowerOfTwo.h>

#ifdef AMBROLIB_AVR
#include <avr-asm-ops/sqrt_29_large.h>
#endif

#include <aprinter/BeginNamespace.h>

template <int NumBits>
class IntSqrt {
public:
    typedef typename ChooseInt<NumBits, false>::Type OpType;
    typedef typename ChooseInt<((NumBits + 1) / 2), false>::Type ResType;
    
    static ResType call (OpType op)
    {
        return
#ifdef AMBROLIB_AVR
            (NumBits <= 29) ? sqrt_29_large(op) :
#endif
            default_sqrt(op);
    }
    
private:
    static ResType default_sqrt (OpType op)
    {
        OpType res = 0;
        OpType one = PowerOfTwo<OpType, (NumBits - 2)>::value;
        
        while (one > op) {
            one >>= 2;
        }
        
        while (one != 0) {
            if (op >= res + one) {
                op = op - (res + one);
                res = res +  2 * one;
            }
            res >>= 1;
            one >>= 2;
        }
        
        return res;
    }
};

#include <aprinter/EndNamespace.h>

#endif
