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

#ifndef APRINTER_MATRIX_LEAST_SQUARES_H
#define APRINTER_MATRIX_LEAST_SQUARES_H

#include <aprinter/base/Assert.h>
#include <aprinter/math/Matrix.h>
#include <aprinter/math/MatrixQr.h>
#include <aprinter/math/MatrixSolveUpperTriangular.h>

#include <aprinter/BeginNamespace.h>

template <int MaxRows, int MaxCols, typename MX, typename MY, typename MBeta>
void LinearLeastSquaresMaxSize (MX mx, MY my, MBeta mbeta)
{
    AMBRO_ASSERT(mx.rows() >= mx.cols())
    AMBRO_ASSERT(mx.rows() <= MaxRows)
    AMBRO_ASSERT(mx.cols() <= MaxCols)
    AMBRO_ASSERT(my.rows() == mx.rows())
    AMBRO_ASSERT(my.cols() == 1)
    AMBRO_ASSERT(mbeta.rows() == mx.cols())
    AMBRO_ASSERT(mbeta.cols() == 1)
    
    int rows = mx.rows();
    int cols = mx.cols();
    
    Matrix<typename MX::T, MaxCols, MaxCols> r_buf;
    Matrix<typename MX::T, MaxRows, 1> col_buf;
    Matrix<typename MX::T, 1, MaxCols> row_buf;
    
    MatrixQrHouseholder(mx--, r_buf--.range(0, 0, cols, cols), col_buf--.range(0, 0, rows, 1), row_buf--.range(0, 0, 1, cols));
    
    auto mqtyn = col_buf--.range(0, 0, cols, 1);
    MatrixMultiply(mqtyn--, mx.transposed(), my++);
    
    MatrixSolveUpperTriangular(r_buf++.range(0, 0, cols, cols), mqtyn++, mbeta--);
}

#include <aprinter/EndNamespace.h>

#endif
