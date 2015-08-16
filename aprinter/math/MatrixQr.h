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

#ifndef APRINTER_MATRIX_QR_H
#define APRINTER_MATRIX_QR_H

#include <aprinter/base/Assert.h>
#include <aprinter/math/Matrix.h>
#include <aprinter/math/MatrixSolveUpperTriangular.h>
#include <aprinter/math/FloatTools.h>

#include <aprinter/BeginNamespace.h>

template <typename MA, typename MQT, typename MTempV, typename MTempQ, typename MTempQ2>
void MatrixQrHouseholder (MA ma, MQT mqt, MTempV tempv, MTempQ tempq, MTempQ2 tempq2)
{
    AMBRO_ASSERT(ma.rows() >= ma.cols())
    AMBRO_ASSERT(mqt.rows() == ma.rows())
    AMBRO_ASSERT(mqt.cols() == ma.rows())
    AMBRO_ASSERT(tempv.rows() == ma.rows())
    AMBRO_ASSERT(tempv.cols() == 1)
    AMBRO_ASSERT(tempq.rows() == ma.rows())
    AMBRO_ASSERT(tempq.cols() == ma.rows())
    AMBRO_ASSERT(tempq2.rows() == ma.rows())
    AMBRO_ASSERT(tempq2.cols() == ma.rows())
    
    int rows = ma.rows();
    int cols = ma.cols();
    int iterations = (cols < rows - 1) ? cols : (rows - 1);
    MatrixWriteIdentity(mqt--);
    for (int k = 0; k < iterations; k++) {
        auto x = ma.range(k, k, rows - k, 1);
        auto alpha = FloatSqrt(MatrixSquareNorm(x)) * ((x(0, 0) < 0) ? 1 : -1);
        auto v = tempv.range(0, 0, rows - k, 1);
        MatrixWriteZero(v);
        v(0, 0) = -alpha;
        MatrixElemOpInPlace<MatrixElemOpAdd>(v--, x++);
        auto beta = FloatSqrt(MatrixSquareNorm(v));
        MatrixElemOpScalarInPlace<MatrixElemOpDivide>(v--, beta);
        auto small_q = tempq.range(k, k, rows - k, rows - k);
        MatrixMultiplyMMT(small_q, v);
        MatrixElemOpScalarInPlace<MatrixElemOpMultiply>(small_q--, -2);
        MatrixElemOpScalarInPlace<MatrixElemOpAdd>(small_q--, 1, MatrixElemPredicateDiagonal());
        auto a_temp = tempq2.range(0, 0, rows, cols);
        MatrixMultiply(a_temp--, tempq++, ma++);
        MatrixCopy(ma--, a_temp++);
        MatrixMultiply(tempq2--, tempq++, mqt++);
        MatrixCopy(mqt--, tempq2++);
        tempq(k, k) = 1;
        MatrixWriteZero(tempq--.range(k, k + 1, 1, rows - (k + 1)));
        MatrixWriteZero(tempq--.range(k + 1, k, rows - (k + 1), 1));
    }
}

template <int Rows, int Cols, typename MA, typename MQT>
void MatrixQrHouseholderKnownSize (MA ma, MQT mqt)
{
    AMBRO_ASSERT(ma.rows() == Rows)
    AMBRO_ASSERT(ma.cols() == Cols)
    AMBRO_ASSERT(mqt.rows() == Rows)
    AMBRO_ASSERT(mqt.cols() == Rows)
    
    Matrix<typename MA::T, Rows, 1> tempv;
    Matrix<typename MA::T, Rows, Rows> tempq;
    Matrix<typename MA::T, Rows, Rows> tempq2;
    
    MatrixQrHouseholder(ma--, mqt--, tempv--, tempq--, tempq2--);
}

#include <aprinter/EndNamespace.h>

#endif
