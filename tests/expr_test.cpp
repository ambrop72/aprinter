#include <stdio.h>

#include <aprinter/meta/Expr.h>
#include <aprinter/meta/TypesAreEqual.h>
#include <aprinter/meta/WrapDouble.h>

using namespace APrinter;

// Functions returning variable values.
struct VarFuncC { static bool call () { return 1; } };
struct VarFuncD { static char call () { return 0; } };

// Define floating point constants.
using FpValE = AMBRO_WRAP_DOUBLE(1.2);
using FpValF = AMBRO_WRAP_DOUBLE(3.5);

// Define constant expressions.
using ConstantA = SimpleConstantExpr<int, 3>;
using ConstantB = SimpleConstantExpr<int, 2>;
using ConstantE = ConstantExpr<double, FpValE>;
using ConstantF = ConstantExpr<double, FpValF>;

// Define variable expressions.
using VariableC = VariableExpr<bool, VarFuncC>;
using VariableD = VariableExpr<char, VarFuncD>;

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
    
    // Try some evaluations.
    // If an expression is constexpr, its eval function must be constexpr.
    constexpr int aplusb = APlusB::eval();
    constexpr double eplusf = EplusF::eval();
    double test1 = Test1::eval();
    
    printf("%d\n", aplusb);
    printf("%f\n", eplusf);
    printf("%f\n", test1);
}
