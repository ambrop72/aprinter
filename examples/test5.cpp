#include <stdio.h>
#include <inttypes.h>

#include <aprinter/meta/Fixed.h>
#include <aprinter/meta/BoundedInt.h>
#include <aprinter/meta/FixedPoint.h>

using namespace APrinter;

template <typename F>
static void print_fixed (F op)
{
    printf("%f num_bits=%d exp=%d bits=%" PRIiMAX "\n", op.toDouble(), F::num_bits, F::exp, (intmax_t)op.m_bits);
}

template <typename B>
static void print_bounded (B op)
{
    printf("%" PRIiMAX " bits=%d\n", (intmax_t)op.value(), B::num_bits);
}

template <typename B>
static void print_fixedpoint (B op)
{
    printf("%f num_bits=%d exp=%d bits=%" PRIiMAX "\n", op.doubleValue(), B::num_bits, B::exp, (intmax_t)op.bitsValue().value());
}

int main ()
{
    auto one = Fixed<int32_t, -4, 8>(1.0);
    auto two = Fixed<int32_t, -4, 8>(2.0);
    auto a = one * two;
    auto b = a * two;
    auto c = b * two;
    auto d = c * two;
    auto e = one + two;
    auto f = two + a;
    
    print_fixed(one);
    print_fixed(two);
    print_fixed(a);
    print_fixed(b);
    print_fixed(c);
    print_fixed(d);
    print_fixed(e);
    print_fixed(f);
    
    auto q0 = BoundedInt<4>::import(-15);
    auto q1 = q0.convert<4>();
    auto q2 = q0 + q1;
    auto q3 = q1 * q2;
    auto q4 = q3.shift<-54>();
    auto q5 = q3.shift<8>();
    auto q6 = q3 / BoundedInt<8>::import(4);
    auto q7 = q6.squareRoot();
    
    print_bounded(q0);
    print_bounded(q1);
    print_bounded(q2);
    print_bounded(q3);
    print_bounded(q4);
    print_bounded(q5);
    print_bounded(q6);
    print_bounded(q7);
    
    auto f_half = FixedPoint<8, -4>::importDouble(0.5);
    auto f_one = FixedPoint<8, -4>::importDouble(1.0);
    auto f_two = FixedPoint<8, -4>::importDouble(2.0);
    auto f_1 = f_one * f_one;
    auto f_2 = f_1.bitsDown<8>();
    auto f_3 = f_half + f_two;
    auto f_4 = FixedPoint<8, -6>::importDouble(1.0);
    auto f_5 = f_4 + f_one;
    auto f_6 = f_two.bitsUp<15>();
    auto f_7 = f_6.squareRoot();
    
    print_fixedpoint(f_half);
    print_fixedpoint(f_one);
    print_fixedpoint(f_two);
    print_fixedpoint(f_1);
    print_fixedpoint(f_2);
    print_fixedpoint(f_3);
    print_fixedpoint(f_4);
    print_fixedpoint(f_5);
    print_fixedpoint(f_6);
    print_fixedpoint(f_7);
}
