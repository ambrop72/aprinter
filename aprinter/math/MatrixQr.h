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

template <typename MR, typename MV, typename MA, typename MRowBuf>
void MatrixTransformHouseholder (MR mr, MV mv, MA ma, MRowBuf row_buf)
{
    AMBRO_ASSERT(mr.rows() == ma.rows())
    AMBRO_ASSERT(mr.cols() == ma.cols())
    AMBRO_ASSERT(mv.cols() == 1)
    AMBRO_ASSERT(ma.rows() == mv.rows())
    AMBRO_ASSERT(row_buf.rows() == 1)
    AMBRO_ASSERT(row_buf.cols() == mv.rows())
    
    int rows = ma.rows();
    int cols = ma.cols();
    
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < rows; j++) {
            row_buf(0, j) = -2.0f * (mv(i, 0) * mv(j, 0));
        }
        row_buf(0, i) += 1.0f;
        
        MatrixMultiply(mr--.range(i, 0, 1, cols), row_buf++, ma++);
    }
}

template <typename MA, typename MQVectors, typename MRowBuf, typename MMatrixBuf>
MQVectors MatrixQrHouseholder (MA ma, MQVectors q_vectors, MRowBuf row_buf, MMatrixBuf matrix_buf)
{
    AMBRO_ASSERT(ma.rows() >= ma.cols())
    AMBRO_ASSERT(q_vectors.rows() >= ma.rows())
    AMBRO_ASSERT(q_vectors.cols() >= MatrixQrNumIterations(ma.rows(), ma.cols()))
    AMBRO_ASSERT(row_buf.rows() == 1)
    AMBRO_ASSERT(row_buf.cols() == ma.rows())
    AMBRO_ASSERT(matrix_buf.rows() == ma.rows())
    AMBRO_ASSERT(matrix_buf.cols() == ma.cols())
    
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
        
        auto ma_for_transform = ma--.range(k, 0, rows - k, cols);
        auto a_temp = matrix_buf.range(0, 0, rows - k, cols);
        MatrixTransformHouseholder(a_temp--, v++, ma_for_transform++, row_buf--.range(0, 0, 1, rows - k));
        MatrixCopy(ma_for_transform--, a_temp++);
    }
    
    return q_vectors.range(0, 0, rows, iterations);
}

template <typename MMatrix, typename MQVectors, typename MRowBuf, typename MMatrixBuf>
void MatrixQrHouseholderQMultiply (MMatrix matrix, MQVectors q_vectors, MRowBuf row_buf, MMatrixBuf matrix_buf)
{
    AMBRO_ASSERT(matrix.rows() == q_vectors.rows())
    AMBRO_ASSERT(q_vectors.rows() > q_vectors.cols())
    AMBRO_ASSERT(row_buf.rows() == 1)
    AMBRO_ASSERT(row_buf.cols() == matrix.rows())
    AMBRO_ASSERT(matrix_buf.rows() == matrix.rows())
    AMBRO_ASSERT(matrix_buf.cols() == matrix.cols())
    
    int rows = matrix.rows();
    int cols = matrix.cols();
    int iterations = q_vectors.cols();
    
    for (int k = iterations - 1; k >= 0; k--) {
        auto v = q_vectors.range(k, k, rows - k, 1);
        
        auto matrix_for_transform = matrix.range(k, 0, rows - k, cols);
        auto result_temp = matrix_buf.range(0, 0, rows - k, cols);
        MatrixTransformHouseholder(result_temp--, v++, matrix_for_transform++, row_buf--.range(0, 0, 1, rows - k));
        MatrixCopy(matrix_for_transform--, result_temp++);
    }
}

#include <aprinter/EndNamespace.h>

#endif
