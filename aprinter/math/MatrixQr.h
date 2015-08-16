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

template <typename MA, typename MQT, typename MTempV, typename MRowBuf, typename MTempQ2>
void MatrixQrHouseholder (MA ma, MQT mqt, MTempV tempv, MRowBuf row_buf, MTempQ2 tempq2)
{
    AMBRO_ASSERT(ma.rows() >= ma.cols())
    AMBRO_ASSERT(mqt.rows() == ma.rows())
    AMBRO_ASSERT(mqt.cols() == ma.rows())
    AMBRO_ASSERT(tempv.rows() == ma.rows())
    AMBRO_ASSERT(tempv.cols() == 1)
    AMBRO_ASSERT(row_buf.rows() == 1)
    AMBRO_ASSERT(row_buf.cols() == ma.rows())
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
        
        auto ma_for_transform = ma--.range(k, 0, rows - k, cols);
        auto a_temp = tempq2.range(0, 0, rows - k, cols);
        MatrixTransformHouseholder(a_temp--, v++, ma_for_transform++, row_buf--.range(0, 0, 1, rows - k));
        MatrixCopy(ma_for_transform--, a_temp++);
        
        auto mqt_for_transform = mqt--.range(k, 0, rows - k, rows);
        auto qt_temp = tempq2.range(0, 0, rows - k, rows);
        MatrixTransformHouseholder(qt_temp--, v++, mqt_for_transform++, row_buf--.range(0, 0, 1, rows - k));
        MatrixCopy(mqt_for_transform--, qt_temp++);
    }
}

template <int MaxRows, int MaxCols, typename MA, typename MQT>
void MatrixQrHouseholderMaxSize (MA ma, MQT mqt)
{
    AMBRO_ASSERT(ma.rows() <= MaxRows)
    AMBRO_ASSERT(ma.cols() <= MaxCols)
    AMBRO_ASSERT(ma.rows() >= ma.cols())
    AMBRO_ASSERT(mqt.rows() == ma.rows())
    AMBRO_ASSERT(mqt.cols() == ma.rows())
    
    Matrix<typename MA::T, MaxRows, 1> tempv;
    Matrix<typename MA::T, 1, MaxRows> row_buf;
    Matrix<typename MA::T, MaxRows, MaxRows> tempq2;
    
    MatrixQrHouseholder(ma--, mqt--, tempv--.range(0, 0, ma.rows(), 1), row_buf--.range(0, 0, 1, ma.rows()), tempq2--.range(0, 0, ma.rows(), ma.rows()));
}

#include <aprinter/EndNamespace.h>

#endif
