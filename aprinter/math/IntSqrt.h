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
#include <aprinter/meta/Modulo.h>
#include <aprinter/meta/IntTypeInfo.h>

#ifdef AMBROLIB_AVR
#include <avr-asm-ops/sqrt_26_large.h>
#endif

#include <aprinter/BeginNamespace.h>

template <int NumBits, bool Round>
class IntSqrt {
public:
    static_assert(NumBits >= 3, "");
    typedef typename ChooseInt<NumBits, false>::Type OpType;
    typedef typename ChooseInt<((NumBits + 1 + Round) / 2), false>::Type ResType;
    
    template <typename Option = int>
    __attribute__((always_inline)) inline static ResType call (OpType op, Option opt = 0)
    {
        return
#ifdef AMBROLIB_AVR
            (NumBits <= 26 && !Round) ? sqrt_26_large(op, opt) :
            (NumBits <= 26 && Round) ? sqrt_26_large_round(op, opt) :
#endif
            DefaultSqrt<OverflowPossible>::call(op);
    }
    
    // small implementation is always here for testing others against
    static ResType good_sqrt (OpType op_arg)
    {
        TempType op = op_arg;
        TempType res = 0;
        TempType one = PowerOfTwo<TempType, (TempBits - 2)>::value;
        
        while (one > op) {
            one >>= 2;
        }
        
        while (one != 0) {
            if (op >= res + one) {
                op -= res + one;
                res = (res >> 1) + one;
            } else {
                res >>= 1;
            }
            one >>= 2;
        }
        
        if (Round && op > res) {
            res++;
        }
        
        return res;
    }

private:
    static const int TempBits = NumBits + Modulo(NumBits, 2);
    using TempType = typename ChooseInt<TempBits, false>::Type;
    static const bool OverflowPossible = (TempBits == IntTypeInfo<TempType>::NumBits);
    
    template <bool TheOverflowPossible, typename Dummy0 = void>
    struct DefaultSqrt {
        static ResType call (OpType op_arg)
        {
            return Work<0>::call(op_arg, 0);
        }
        
        template <int I, typename Dummy = void>
        struct Work {
            static ResType call (TempType op, TempType res)
            {
                TempType one = PowerOfTwo<TempType, (TempBits - 2 - (2 * I))>::value;
                if (op >= res + one) {
                    op -= res + one;
                    res = (res >> 1) + one;
                } else {
                    res >>= 1;
                }
                return Work<(I + 1)>::call(op, res);
            }
        };
        
        template <typename Dummy>
        struct Work<(TempBits / 2), Dummy> {
            static ResType call (TempType op, TempType res)
            {
                if (Round && op > res) {
                    res++;
                }
                return res;
            }
        };
    };
    
    template <typename Dummy0>
    struct DefaultSqrt<false, Dummy0> {
        static ResType call (OpType op_arg)
        {
            return Work<0>::call(op_arg, PowerOfTwo<TempType, TempBits - 2>::value);
        }
        
        template <int I, typename Dummy = void>
        struct Work {
            static ResType call (TempType op, TempType res)
            {
                static const TempType one = PowerOfTwo<TempType, TempBits - 2 - I>::value;
                static const TempType prev_one = one << 1;
                static const TempType next_one = one >> 1;
                static const TempType res_add_nobit = (TempType)(next_one - one);
                static const TempType res_add_bit = (TempType)(res_add_nobit + prev_one);
                if (op >= res) {
                    op -= res;
                    res += res_add_bit;
                } else {
                    res += res_add_nobit;
                }
                op <<= 1;
                return Work<(I + 1)>::call(op, res);
            }
        };
        
        template <typename Dummy>
        struct Work<((TempBits / 2) - 1), Dummy> {
            static ResType call (TempType op, TempType res)
            {
                if (op >= res) {
                    if (Round) {
                        op -= res;
                    }
                    res += PowerOfTwo<TempType, (TempBits / 2)>::value;
                }
                if (Round) {
                    op <<= 1;
                    if (op > res) {
                        res += PowerOfTwo<TempType, (TempBits / 2)>::value;
                    }
                }
                return res >> (TempBits / 2);
            }
        };
    };
    
};

#include <aprinter/EndNamespace.h>

#endif
