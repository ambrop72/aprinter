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

inline constexpr int MatrixQrNumIterations (int rows, int cols)
{
    return (cols < rows - 1) ? cols : (rows - 1);
}

template <typename MV, typename MA, typename MColBuf>
void MatrixTransformHouseholder (MV mv, MA ma, MColBuf col_buf)
{
    AMBRO_ASSERT(mv.cols() == 1)
    AMBRO_ASSERT(ma.rows() == mv.rows())
    AMBRO_ASSERT(col_buf.rows() == ma.rows())
    AMBRO_ASSERT(col_buf.cols() == 1)
    
    int rows = ma.rows();
    int cols = ma.cols();
    
    for (int j = 0; j < cols; j++) {
        MatrixCopy(col_buf--, ma++.range(0, j, rows, 1));
        
        for (int i = 0; i < rows; i++) {
            typename MA::T value = 0.0f;
            for (int k = 0; k < rows; k++) {
                value += (-2.0f * (mv(i, 0) * mv(k, 0)) + (i == k)) * col_buf(k, 0);
            }
            
            ma(i, j) = value;
        }
    }
}

template <typename MA, typename MQVectors, typename MRowBuf>
MQVectors MatrixQrHouseholder (MA ma, MQVectors q_vectors, MRowBuf col_buf)
{
    AMBRO_ASSERT(ma.rows() >= ma.cols())
    AMBRO_ASSERT(q_vectors.rows() >= ma.rows())
    AMBRO_ASSERT(q_vectors.cols() >= MatrixQrNumIterations(ma.rows(), ma.cols()))
    AMBRO_ASSERT(col_buf.rows() == ma.rows())
    AMBRO_ASSERT(col_buf.cols() == 1)
    
    int rows = ma.rows();
    int cols = ma.cols();
    int iterations = MatrixQrNumIterations(rows, cols);
    
    for (int k = 0; k < iterations; k++) {
        auto x = ma.range(k, k, rows - k, 1);
        auto alpha = FloatSqrt(MatrixSquareNorm(x)) * ((x(0, 0) < 0) ? 1 : -1);
        
        auto v = q_vectors.range(k, k, rows - k, 1);
        MatrixWriteZero(v);
        v(0, 0) = -alpha;
        MatrixElemOpInPlace<MatrixElemOpAdd>(v--, x++);
        auto beta = FloatSqrt(MatrixSquareNorm(v));
        MatrixElemOpScalarInPlace<MatrixElemOpDivide>(v--, beta);
        
        auto ma_for_transform = ma--.range(k, k, rows - k, cols - k);
        MatrixTransformHouseholder(v++, ma_for_transform--, col_buf--.range(0, 0, rows - k, 1));
    }
    
    return q_vectors.range(0, 0, rows, iterations);
}

template <typename MMatrix, typename MQVectors, typename MRowBuf>
void MatrixQrHouseholderQMultiply (MMatrix matrix, MQVectors q_vectors, MRowBuf col_buf)
{
    AMBRO_ASSERT(matrix.rows() == q_vectors.rows())
    AMBRO_ASSERT(q_vectors.rows() > q_vectors.cols())
    AMBRO_ASSERT(col_buf.rows() == matrix.rows())
    AMBRO_ASSERT(col_buf.cols() == 1)
    
    int rows = matrix.rows();
    int cols = matrix.cols();
    int iterations = q_vectors.cols();
    
    for (int k = iterations - 1; k >= 0; k--) {
        auto v = q_vectors.range(k, k, rows - k, 1);
        
        auto matrix_for_transform = matrix.range(k, 0, rows - k, cols);
        MatrixTransformHouseholder(v++, matrix_for_transform--, col_buf--.range(0, 0, rows - k, 1));
    }
}

#include <aprinter/EndNamespace.h>

#endif
