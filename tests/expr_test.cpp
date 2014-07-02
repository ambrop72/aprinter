#include <stdio.h>
#include <inttypes.h>

#include <aprinter/meta/Expr.h>
#include <aprinter/meta/ExprFixedPoint.h>
#include <aprinter/meta/TypesAreEqual.h>
#include <aprinter/meta/WrapDouble.h>

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
