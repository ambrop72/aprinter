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

template <int Rows, int Cols, typename MX, typename MY, typename MBeta>
void LinearLeastSquaresKnownSize (MX mx, MY my, MBeta mbeta)
{
    AMBRO_ASSERT(Rows >= Cols)
    AMBRO_ASSERT(mx.rows() == Rows)
    AMBRO_ASSERT(mx.cols() == Cols)
    AMBRO_ASSERT(my.rows() == Rows)
    AMBRO_ASSERT(my.cols() == 1)
    AMBRO_ASSERT(mbeta.rows() == Cols)
    AMBRO_ASSERT(mbeta.cols() == 1)
    
    Matrix<typename MX::T, Rows, Rows> mqt;
    MatrixQrHouseholderKnownSize<Rows, Cols>(mx--, mqt--);
    
    auto mrn = mx.range(0, 0, Cols, Cols);
    
    Matrix<typename MX::T, Cols, 1> mqtyn;
    MatrixMultiply(mqtyn--, mqt++.range(0, 0, Cols, Rows), my++);
    
    MatrixSolveUpperTriangular(mrn++, mqtyn++, mbeta--);
}

#include <aprinter/EndNamespace.h>

#endif
