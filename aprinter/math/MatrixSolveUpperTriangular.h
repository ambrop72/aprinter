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

#ifndef APRINTER_MATRIX_SOLVE_UPPER_TRIANGULAR_H
#define APRINTER_MATRIX_SOLVE_UPPER_TRIANGULAR_H

#include <aprinter/base/Assert.h>
#include <aprinter/math/Matrix.h>

#include <aprinter/BeginNamespace.h>

template <typename MA, typename MY, typename MX>
void MatrixSolveUpperTriangular (MA ma, MY my, MX mx)
{
    AMBRO_ASSERT(ma.rows() == ma.cols())
    AMBRO_ASSERT(my.rows() == ma.rows())
    AMBRO_ASSERT(mx.rows() == ma.rows())
    AMBRO_ASSERT(my.cols() == mx.cols())
    
    int n = ma.rows();
    int m = my.cols();
    for (int j = n - 1; j >= 0; j--) {
        auto this_x = mx.range(j, 0, 1, m);
        auto this_y = my.range(j, 0, 1, m);
        MatrixMultiply(this_x--, ma++.range(j, j + 1, 1, n - (j + 1)), mx++.range(j + 1, 0, n - (j + 1), m));
        MatrixElemOpScalarInPlace<MatrixElemOpMultiply>(this_x--, -1);
        MatrixElemOpInPlace<MatrixElemOpAdd>(this_x--, this_y++);
        MatrixElemOpScalarInPlace<MatrixElemOpDivide>(this_x--, ma(j, j));
    }
}

#include <aprinter/EndNamespace.h>

#endif
