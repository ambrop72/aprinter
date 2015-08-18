#include <stdio.h>

#include <aprinter/math/LinearLeastSquares.h>

using namespace APrinter;

template <typename M>
static void print_matrix (M m)
{
    for (int i = 0; i < m.rows(); i++) {
        for (int j = 0; j < m.cols(); j++) {
            printf("%f ", (double)m(i, j));
        }
        printf("\n");
    }
    printf("\n");
}

int main ()
{
    static int const Rows = 7;
    static int const Cols = 3;
    
    Matrix<double, Rows, Cols> x;
    Matrix<double, Rows, 1> y;
    
    x--(0, 0) = -6;  x--(0, 1) = -4;  x--(0, 2) = 1.4; y--(0, 0) = 0.5;
    x--(1, 0) = 7;   x--(1, 1) = -5;  x--(1, 2) = 1.7; y--(1, 0) = -0.2;
    x--(2, 0) = 4;   x--(2, 1) = 12;  x--(2, 2) = 1.2; y--(2, 0) = 2.1;
    x--(3, 0) = 3;   x--(3, 1) = 6;   x--(3, 2) = 1.5; y--(3, 0) = 7.4;
    x--(4, 0) = 5;   x--(4, 1) = -1;  x--(4, 2) = 1.1; y--(4, 0) = 6;
    x--(5, 0) = 8.4; x--(5, 1) = 4.5; x--(5, 2) = 1.6; y--(5, 0) = 6.7;
    x--(6, 0) = 7.4; x--(6, 1) = 4.7; x--(6, 2) = -3;  y--(6, 0) = -5.2;
    
    printf("X=\n");
    print_matrix(x++);
    printf("Y=\n");
    print_matrix(y++);
    
    auto x_copy = x;
    Matrix<double, Cols, 1> beta;
    LinearLeastSquaresMaxSize<Rows, Cols>(x_copy--, y++, beta--);
    
    printf("Beta=\n");
    print_matrix(beta++);
    
    Matrix<double, 1, 1> result;
    
    for (int i = 0; i < Rows; i++) {
        MatrixMultiply(result--, x++.range(i, 0, 1, Cols), beta++);
        printf("Result%d=\n", i);
        print_matrix(result++);
    }
    
    return 0;
}
