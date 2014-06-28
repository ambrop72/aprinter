/*
 * Copyright (c) 2014 Ambroz Bizjak
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

#ifndef APRINTER_EXPR_H
#define APRINTER_EXPR_H

#include <aprinter/meta/TypeList.h>
#include <aprinter/meta/TypeListReverse.h>
#include <aprinter/meta/If.h>
#include <aprinter/meta/WrapValue.h>
#include <aprinter/meta/RemoveReference.h>
#include <aprinter/meta/ConstexprMath.h>

#include <aprinter/BeginNamespace.h>

template <typename FullExpr>
struct Expr {
    static FullExpr e ();
};

template <typename TType, typename ValueProvider>
struct ConstantExpr : public Expr<ConstantExpr<TType, ValueProvider>> {
    using Type = TType;
    static bool const IsConstexpr = true;
    
    template <typename... Args>
    static constexpr Type eval (Args... args)
    {
        return ValueProvider::value();
    }
};

template <typename Type, Type Value>
using SimpleConstantExpr = ConstantExpr<Type, WrapValue<Type, Value>>;

template <typename ValueProvider>
using DoubleConstantExpr = ConstantExpr<double, ValueProvider>;

#define APRINTER_FP_CONST_EXPR__HELPER1(the_value, counter) \
struct APrinterExprFpConst__##counter : public APrinter::DoubleConstantExpr<APrinterExprFpConst__##counter> { \
    static constexpr double value () { return (the_value); } \
}

#define APRINTER_FP_CONST_EXPR__HELPER2(the_value, counter) APRINTER_FP_CONST_EXPR__HELPER1((the_value), counter)

#define APRINTER_FP_CONST_EXPR(the_value) APRINTER_FP_CONST_EXPR__HELPER2((the_value), __COUNTER__)

template <typename TType, typename EvalFunc>
struct VariableExpr : public Expr<VariableExpr<TType, EvalFunc>> {
    using Type = TType;
    static bool const IsConstexpr = false;
    
    template <typename... Args>
    static Type eval (Args... args)
    {
        return EvalFunc::call(args...);
    }
};

template <typename... Operands>
struct Expr__IsConstexpr;

template <typename Operand, typename... TailOperands>
struct Expr__IsConstexpr<Operand, TailOperands...> {
    static bool const Value = (Operand::IsConstexpr && Expr__IsConstexpr<TailOperands...>::Value);
};

template <>
struct Expr__IsConstexpr<> {
    static bool const Value = true;
};

template <typename Func, typename... Operands>
struct ConstexprNaryExpr : public Expr<ConstexprNaryExpr<Func, Operands...>> {
    using Type = decltype(Func::call(typename Operands::Type()...));
    static bool const IsConstexpr = true;
    
    template <typename... Args>
    static constexpr Type eval (Args... args)
    {
        return Func::call(Operands::eval(args...)...);
    }
};

template <typename Func, typename... Operands>
struct RuntimeNaryExpr : public Expr<RuntimeNaryExpr<Func, Operands...>> {
    using Type = decltype(Func::call(typename Operands::Type()...));
    static bool const IsConstexpr = false;
    
    template <typename... Args>
    static Type eval (Args... args)
    {
        return Func::call(Operands::eval(args...)...);
    }
};

template <typename Func, typename... Operands>
using NaryExpr = If<Expr__IsConstexpr<Operands...>::Value, ConstexprNaryExpr<Func, Operands...>, RuntimeNaryExpr<Func, Operands...>>;

#define APRINTER_DEFINE_UNARY_EXPR(Operator, Name) \
struct ExprFunc##Name { \
    template <typename Arg1> \
    static constexpr auto call (Arg1 arg1) -> RemoveReference<decltype(Operator arg1)> \
    { \
        return Operator arg1; \
    } \
}; \
template <typename Op1> \
NaryExpr<ExprFunc##Name, Op1> operator Operator (Expr<Op1>);

APRINTER_DEFINE_UNARY_EXPR(+, UnaryPlus)
APRINTER_DEFINE_UNARY_EXPR(-, UnaryMinus)
APRINTER_DEFINE_UNARY_EXPR(!, LogicalNegation)
APRINTER_DEFINE_UNARY_EXPR(~, BitwiseNot)

#define APRINTER_DEFINE_BINARY_EXPR(Operator, Name) \
struct ExprFunc##Name { \
    template <typename Arg1, typename Arg2> \
    static constexpr auto call (Arg1 arg1, Arg2 arg2) -> RemoveReference<decltype(arg1 Operator arg2)> \
    { \
        return arg1 Operator arg2; \
    } \
}; \
template <typename Op1, typename Op2> \
NaryExpr<ExprFunc##Name, Op1, Op2> operator Operator (Expr<Op1>, Expr<Op2>);

APRINTER_DEFINE_BINARY_EXPR(+,  Addition)
APRINTER_DEFINE_BINARY_EXPR(-,  Subtraction)
APRINTER_DEFINE_BINARY_EXPR(*,  Multiplication)
APRINTER_DEFINE_BINARY_EXPR(/,  Division)
APRINTER_DEFINE_BINARY_EXPR(%,  Modulo)
APRINTER_DEFINE_BINARY_EXPR(==, EqualTo)
APRINTER_DEFINE_BINARY_EXPR(!=, NotEqualTo)
APRINTER_DEFINE_BINARY_EXPR(>,  GreaterThan)
APRINTER_DEFINE_BINARY_EXPR(<,  LessThan)
APRINTER_DEFINE_BINARY_EXPR(>=, GreaterThenOrEqualTo)
APRINTER_DEFINE_BINARY_EXPR(<=, LessThanOrEqualTo)
APRINTER_DEFINE_BINARY_EXPR(&&, LogicalAnd)
APRINTER_DEFINE_BINARY_EXPR(||, LogicalOr)
APRINTER_DEFINE_BINARY_EXPR(&,  BitwiseAnd)
APRINTER_DEFINE_BINARY_EXPR(|,  BitwiseOr)
APRINTER_DEFINE_BINARY_EXPR(^,  BitwiseXor)
APRINTER_DEFINE_BINARY_EXPR(<<, BitwiseLeftShift)
APRINTER_DEFINE_BINARY_EXPR(>>, BitwiseRightShift)

struct ExprFuncIf {
    template <typename Arg1, typename Arg2, typename Arg3>
    static constexpr auto call (Arg1 arg1, Arg2 arg2, Arg3 arg3) -> RemoveReference<decltype(arg1 ? arg2 : arg3)>
    {
        return arg1 ? arg2 : arg3;
    }
};
template <typename Op1, typename Op2, typename Op3>
NaryExpr<ExprFuncIf, Op1, Op2, Op3> ExprIf (Op1, Op2, Op3);

template <typename TargetType>
struct ExprFuncCast {
    template <typename Arg1>
    static constexpr auto call (Arg1 arg1) -> RemoveReference<decltype((TargetType)arg1)>
    {
        return (TargetType)arg1;
    }
};
template <typename TargetType, typename Op1>
NaryExpr<ExprFuncCast<TargetType>, Op1> ExprCast (Op1);

struct ExprFuncFmin {
    template <typename Arg1, typename Arg2>
    static constexpr auto call (Arg1 arg1, Arg2 arg2) -> double
    {
        return ConstexprFmin(arg1, arg2);
    }
};
template <typename Op1, typename Op2>
NaryExpr<ExprFuncFmin, Op1, Op2> ExprFmin (Op1, Op2);

struct ExprFuncFmax {
    template <typename Arg1, typename Arg2>
    static constexpr auto call (Arg1 arg1, Arg2 arg2) -> double
    {
        return ConstexprFmax(arg1, arg2);
    }
};
template <typename Op1, typename Op2>
NaryExpr<ExprFuncFmax, Op1, Op2> ExprFmax (Op1, Op2);

template <typename Type, Type Value>
SimpleConstantExpr<Type, Value> ExprConst ();

template <bool Value>
SimpleConstantExpr<bool, Value> ExprBoolConst ();

#include <aprinter/EndNamespace.h>

#endif
