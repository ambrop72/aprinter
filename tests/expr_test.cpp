/*
 * Copyright (c) 2015 Ambroz Bizjak
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

#include <stdio.h>
#include <inttypes.h>

#include <aprinter/meta/Expr.h>
#include <aprinter/meta/ExprFixedPoint.h>
#include <aprinter/meta/BasicMetaUtils.h>

using namespace APrinter;

// Functions returning variable values.
struct VarFuncC { static bool call () { return 1; } };
struct VarFuncD { static char call () { return 0; } };
struct VarFuncG { static double call () { return 123.7; } };

// Define floating point constants.

// Define constant expressions.
using ConstantA = SimpleConstantExpr<int, 3>;
using ConstantB = SimpleConstantExpr<int, 2>;
using ConstantE = APRINTER_FP_CONST_EXPR(1.2);
using ConstantF = APRINTER_FP_CONST_EXPR(3.5);
using ConstantH = APRINTER_FP_CONST_EXPR(123.7);

// Define variable expressions.
using VariableC = VariableExpr<bool, VarFuncC>;
using VariableD = VariableExpr<char, VarFuncD>;
using VariableG = VariableExpr<double, VarFuncG>;

int main ()
{
    // Check types of basic expressions.
    static_assert(TypesAreEqual<ConstantA::Type, int>::Value, "");
    static_assert(TypesAreEqual<ConstantB::Type, int>::Value, "");
    static_assert(TypesAreEqual<VariableC::Type, bool>::Value, "");
    static_assert(TypesAreEqual<VariableD::Type, char>::Value, "");
    static_assert(TypesAreEqual<ConstantE::Type, double>::Value, "");
    static_assert(TypesAreEqual<ConstantF::Type, double>::Value, "");
    
    // Make and check some composed expressions
    // Check types and constant expressionness.
    
    using APlusB = decltype(ConstantA() + ConstantB());
    static_assert(TypesAreEqual<APlusB::Type, int>::Value, "");
    static_assert(APlusB::IsConstexpr, "");
    
    using BPlusC = decltype(ConstantB() + VariableC());
    static_assert(TypesAreEqual<BPlusC::Type, int>::Value, "");
    static_assert(!BPlusC::IsConstexpr, "");
    
    using EplusF = decltype(ConstantE() + ConstantF());
    static_assert(TypesAreEqual<EplusF::Type, double>::Value, "");
    static_assert(EplusF::IsConstexpr, "");
    
    using Test1 = decltype(ExprIf(VariableC(), ConstantE(), ConstantF()));
    static_assert(TypesAreEqual<Test1::Type, double>::Value, "");
    static_assert(!Test1::IsConstexpr, "");
    
    using MyFixedType = FixedPoint<16, false, 0>;
    using Fixed1 = decltype(ExprFixedPointImport<MyFixedType>(ConstantH()));
    using Fixed2 = decltype(ExprFixedPointImport<MyFixedType>(VariableG()));
    
    // Try some evaluations.
    // If an expression is constexpr, its must have a constexpr value() function.
    constexpr int aplusb = APlusB::value();
    constexpr double eplusf = EplusF::value();
    double test1 = Test1::eval();
    constexpr MyFixedType fixed1 = Fixed1::value();
    MyFixedType fixed2 = Fixed2::eval();
    
    printf("%d\n", aplusb);
    printf("%f\n", eplusf);
    printf("%f\n", test1);
    printf("%" PRIu16 "\n", fixed1.m_bits.m_int);
    printf("%" PRIu16 "\n", fixed2.m_bits.m_int);
}
