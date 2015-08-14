#include <stdio.h>


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

#include <aprinter/math/MatrixQr.h>

using namespace APrinter;

int main ()
{
    Matrix<double, 5, 3> x;
    Matrix<double, 5, 1> y;
    /*
    x--(0, 0) = -6; x--(0, 1) = -4; x--(0, 2) = 1; y--(0, 0) = 0.5;
    x--(1, 0) = 7;  x--(1, 1) = -5; x--(1, 2) = 1; y--(1, 0) = -0.2;
    x--(2, 0) = 0;  x--(2, 1) = 0; x--(2, 2) = 0; y--(2, 0) = 0;
    x--(3, 0) = 4;  x--(3, 1) = 12; x--(3, 2) = 1; y--(3, 0) = 2.1;
    */
    x--(0, 0) = -6; x--(0, 1) = -4; x--(0, 2) = 1; y--(0, 0) = 0.5;
    x--(1, 0) = 7;  x--(1, 1) = -5; x--(1, 2) = 1; y--(1, 0) = -0.2;
    x--(2, 0) = 4;  x--(2, 1) = 12; x--(2, 2) = 1; y--(2, 0) = 2.1;
    x--(3, 0) = 0;  x--(3, 1) = 0;  x--(3, 2) = 0; y--(3, 0) = 0;
    x--(4, 0) = 0;  x--(4, 1) = 0;  x--(4, 2) = 0; y--(4, 0) = 0;
    
    printf("X=\n");
    print_matrix(x++);
    printf("Y=\n");
    print_matrix(y++);
    
    auto x_copy = x;
    Matrix<double, 3, 1> beta;
    LinearLeastSquaresKnownSize<5, 3>(x_copy--, y++, beta--);
    
    printf("Beta=\n");
    print_matrix(beta++);
    
    Matrix<double, 1, 1> result;
    
    MatrixMultiply(result--, x++.range(0, 0, 1, 3), beta++);
    printf("Result1=\n");
    print_matrix(result++);
    
    MatrixMultiply(result--, x++.range(1, 0, 1, 3), beta++);
    printf("Result2=\n");
    print_matrix(result++);
    
    MatrixMultiply(result--, x++.range(2, 0, 1, 3), beta++);
    printf("Result3=\n");
    print_matrix(result++);
    
    MatrixMultiply(result--, x++.range(3, 0, 1, 3), beta++);
    printf("Result4=\n");
    print_matrix(result++);
    
    MatrixMultiply(result--, x++.range(4, 0, 1, 3), beta++);
    printf("Result5=\n");
    print_matrix(result++);
    
    return 0;
}
