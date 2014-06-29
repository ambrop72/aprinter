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
    
    static constexpr Type value ()
    {
        return ValueProvider::value();
    }
    
    template <typename... Args>
    static Type eval (Args... args)
    {
        return value();
    }
};

template <typename Type, Type Value>
using SimpleConstantExpr = ConstantExpr<Type, WrapValue<Type, Value>>;

template <typename ValueProvider>
using DoubleConstantExpr = ConstantExpr<double, ValueProvider>;

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
struct Expr__OperandsAreConstexpr;

template <typename Operand, typename... TailOperands>
struct Expr__OperandsAreConstexpr<Operand, TailOperands...> {
    static bool const Value = (Operand::IsConstexpr && Expr__OperandsAreConstexpr<TailOperands...>::Value);
};

template <>
struct Expr__OperandsAreConstexpr<> {
    static bool const Value = true;
};

template <typename Func, typename... Operands>
struct ConstexprNaryExpr : public Expr<ConstexprNaryExpr<Func, Operands...>> {
    using Type = decltype(Func::call(typename Operands::Type()...));
    static bool const IsConstexpr = true;
    
    static constexpr Type value ()
    {
        return Func::call(Operands::value()...);
    }
    
    template <typename... Args>
    static Type eval (Args... args)
    {
        return value();
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
using NaryExpr = If<
    Expr__OperandsAreConstexpr<Operands...>::Value,
    ConstexprNaryExpr<Func, Operands...>,
    RuntimeNaryExpr<Func, Operands...>
>;

#define APRINTER_FP_CONST_EXPR__HELPER1(the_value, counter) \
struct APrinterExprFpConst__##counter : public APrinter::DoubleConstantExpr<APrinterExprFpConst__##counter> { \
    static constexpr double value () { return (the_value); } \
}

#define APRINTER_FP_CONST_EXPR__HELPER2(the_value, counter) APRINTER_FP_CONST_EXPR__HELPER1(the_value, counter)

#define APRINTER_FP_CONST_EXPR(the_value) APRINTER_FP_CONST_EXPR__HELPER2(the_value, __COUNTER__)

#define APRINTER_DEFINE_UNARY_EXPR_FUNC_CLASS(Name, EvalExpr) \
struct ExprFunc__##Name { \
    template <typename Arg1> \
    static constexpr auto call (Arg1 arg1) -> RemoveReference<decltype(EvalExpr)> \
    { \
        return (EvalExpr); \
    } \
};

#define APRINTER_DEFINE_BINARY_EXPR_FUNC_CLASS(Name, EvalExpr) \
struct ExprFunc__##Name { \
    template <typename Arg1, typename Arg2> \
    static constexpr auto call (Arg1 arg1, Arg2 arg2) -> RemoveReference<decltype(EvalExpr)> \
    { \
        return (EvalExpr); \
    } \
};

#define APRINTER_DEFINE_TERNARY_EXPR_FUNC_CLASS(Name, EvalExpr) \
struct ExprFunc__##Name { \
    template <typename Arg1, typename Arg2, typename Arg3> \
    static constexpr auto call (Arg1 arg1, Arg2 arg2, Arg3 arg3) -> RemoveReference<decltype(EvalExpr)> \
    { \
        return (EvalExpr); \
    } \
};

#define APRINTER_DEFINE_UNARY_EXPR_OPERATOR(Operator, Name) \
APRINTER_DEFINE_UNARY_EXPR_FUNC_CLASS(Name, Operator arg1) \
template <typename Op1> \
NaryExpr<ExprFunc__##Name, Op1> operator Operator (Expr<Op1>);

#define APRINTER_DEFINE_BINARY_EXPR_OPERATOR(Operator, Name) \
APRINTER_DEFINE_BINARY_EXPR_FUNC_CLASS(Name, arg1 Operator arg2) \
template <typename Op1, typename Op2> \
NaryExpr<ExprFunc__##Name, Op1, Op2> operator Operator (Expr<Op1>, Expr<Op2>);

#define APRINTER_DEFINE_UNARY_EXPR_FUNC(Name, EvalExpr) \
APRINTER_DEFINE_UNARY_EXPR_FUNC_CLASS(Name, EvalExpr) \
template <typename Op1> \
NaryExpr<ExprFunc__##Name, Op1> Expr##Name (Op1);

#define APRINTER_DEFINE_BINARY_EXPR_FUNC(Name, EvalExpr) \
APRINTER_DEFINE_BINARY_EXPR_FUNC_CLASS(Name, EvalExpr) \
template <typename Op1, typename Op2> \
NaryExpr<ExprFunc__##Name, Op1, Op2> Expr##Name (Op1, Op2);

#define APRINTER_DEFINE_TERNARY_EXPR_FUNC(Name, EvalExpr) \
APRINTER_DEFINE_TERNARY_EXPR_FUNC_CLASS(Name, EvalExpr) \
template <typename Op1, typename Op2, typename Op3> \
NaryExpr<ExprFunc__##Name, Op1, Op2, Op3> Expr##Name (Op1, Op2, Op3);

template <typename Type, Type Value>
SimpleConstantExpr<Type, Value> ExprConst ();

template <bool Value>
SimpleConstantExpr<bool, Value> ExprBoolConst ();

APRINTER_DEFINE_UNARY_EXPR_OPERATOR(+, UnaryPlus)
APRINTER_DEFINE_UNARY_EXPR_OPERATOR(-, UnaryMinus)
APRINTER_DEFINE_UNARY_EXPR_OPERATOR(!, LogicalNegation)
APRINTER_DEFINE_UNARY_EXPR_OPERATOR(~, BitwiseNot)

template <typename TargetType>
APRINTER_DEFINE_UNARY_EXPR_FUNC_CLASS(Cast, (TargetType)arg1)
template <typename TargetType, typename Op1>
NaryExpr<ExprFunc__Cast<TargetType>, Op1> ExprCast (Op1);

APRINTER_DEFINE_BINARY_EXPR_OPERATOR(+,  Addition)
APRINTER_DEFINE_BINARY_EXPR_OPERATOR(-,  Subtraction)
APRINTER_DEFINE_BINARY_EXPR_OPERATOR(*,  Multiplication)
APRINTER_DEFINE_BINARY_EXPR_OPERATOR(/,  Division)
APRINTER_DEFINE_BINARY_EXPR_OPERATOR(%,  Modulo)
APRINTER_DEFINE_BINARY_EXPR_OPERATOR(==, EqualTo)
APRINTER_DEFINE_BINARY_EXPR_OPERATOR(!=, NotEqualTo)
APRINTER_DEFINE_BINARY_EXPR_OPERATOR(>,  GreaterThan)
APRINTER_DEFINE_BINARY_EXPR_OPERATOR(<,  LessThan)
APRINTER_DEFINE_BINARY_EXPR_OPERATOR(>=, GreaterThenOrEqualTo)
APRINTER_DEFINE_BINARY_EXPR_OPERATOR(<=, LessThanOrEqualTo)
APRINTER_DEFINE_BINARY_EXPR_OPERATOR(&&, LogicalAnd)
APRINTER_DEFINE_BINARY_EXPR_OPERATOR(||, LogicalOr)
APRINTER_DEFINE_BINARY_EXPR_OPERATOR(&,  BitwiseAnd)
APRINTER_DEFINE_BINARY_EXPR_OPERATOR(|,  BitwiseOr)
APRINTER_DEFINE_BINARY_EXPR_OPERATOR(^,  BitwiseXor)
APRINTER_DEFINE_BINARY_EXPR_OPERATOR(<<, BitwiseLeftShift)
APRINTER_DEFINE_BINARY_EXPR_OPERATOR(>>, BitwiseRightShift)

APRINTER_DEFINE_BINARY_EXPR_FUNC(Fmin, ConstexprFmin(arg1, arg2))
APRINTER_DEFINE_BINARY_EXPR_FUNC(Fmax, ConstexprFmax(arg1, arg2))

APRINTER_DEFINE_TERNARY_EXPR_FUNC(If, arg1 ? arg2 : arg3)

#include <aprinter/EndNamespace.h>

#endif
