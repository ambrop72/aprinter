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

#ifndef APRINTER_MATRIX_H
#define APRINTER_MATRIX_H

#include <stddef.h>

#include <aprinter/base/Assert.h>
#include <aprinter/meta/TypesAreEqual.h>
#include <aprinter/meta/RemoveConst.h>

#include <aprinter/BeginNamespace.h>

template <typename TTMaybeConst>
class MatrixRange {
public:
    using TMaybeConst = TTMaybeConst;
    using T = RemoveConst<TMaybeConst>;
    
    static MatrixRange Make (TMaybeConst *data, int rows, int cols, int stride)
    {
        AMBRO_ASSERT(rows >= 0)
        AMBRO_ASSERT(cols >= 0)
        AMBRO_ASSERT(stride >= cols)
        
        MatrixRange res;
        res.m_data = data;
        res.m_rows = rows;
        res.m_cols = cols;
        res.m_stride = stride;
        return res;
    }
    
    int rows () const
    {
        return m_rows;
    }
    
    int cols () const
    {
        return m_cols;
    }
    
    TMaybeConst & operator() (int row, int col) const
    {
        AMBRO_ASSERT(row >= 0)
        AMBRO_ASSERT(row < m_rows)
        AMBRO_ASSERT(col >= 0)
        AMBRO_ASSERT(col < m_cols)
        
        return *index_data(row, col);
    }
    
    MatrixRange range (int row, int col, int rows, int cols) const
    {
        AMBRO_ASSERT(row >= 0)
        AMBRO_ASSERT(row <= m_rows)
        AMBRO_ASSERT(rows <= m_rows - row)
        AMBRO_ASSERT(col >= 0)
        AMBRO_ASSERT(col <= m_cols)
        AMBRO_ASSERT(cols <= m_cols - col)
        
        return Make(index_data(row, col), rows, cols, m_stride);
    }
    
    MatrixRange operator-- (int) const
    {
        return *this;
    }
    
    MatrixRange<T const> operator++ (int) const
    {
        return MatrixRange<T const>::Make(m_data, m_rows, m_cols, m_stride);
    }
    
    operator MatrixRange<T const> () const
    {
        return (*this)++;
    }
    
private:
    TMaybeConst * index_data (int row, int col) const
    {
        return m_data + ((size_t)row * m_stride + col);
    }
    
    TMaybeConst *m_data;
    int m_rows;
    int m_cols;
    int m_stride;
};

template <typename TT, int TRows, int TCols>
class Matrix {
    static_assert(TypesAreEqual<TT, float>::Value || TypesAreEqual<TT, double>::Value, "");
    static_assert(TRows > 0, "");
    static_assert(TCols > 0, "");
    
public:
    using T = TT;
    static const int Rows = TRows;
    static const int Cols = TCols;
    
    MatrixRange<T> operator-- (int)
    {
        return MatrixRange<T>::Make(m_data, Rows, Cols, Cols);
    }
    
    MatrixRange<T const> operator-- (int) const
    {
        return MatrixRange<T const>::Make(m_data, Rows, Cols, Cols);
    }
    
    MatrixRange<T const> operator++ (int) const
    {
        return MatrixRange<T const>::Make(m_data, Rows, Cols, Cols);
    }
    
private:
    T m_data[(size_t)Rows * Cols];
};

template <typename MR, typename M1>
void MatrixCopy (MR mr, M1 m1)
{
    AMBRO_ASSERT(mr.rows() == m1.rows())
    AMBRO_ASSERT(mr.cols() == m1.cols())
    
    int rows = mr.rows();
    int cols = mr.cols();
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            mr(i, j) = m1(i, j);
        }
    }
}

template <typename MR>
void MatrixWriteIdentity (MR mr)
{
    int rows = mr.rows();
    int cols = mr.cols();
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            mr(i, j) = (i == j);
        }
    }
}

template <typename MR>
void MatrixWriteZero (MR mr)
{
    int rows = mr.rows();
    int cols = mr.cols();
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            mr(i, j) = 0;
        }
    }
}

template <typename MR, typename M1, typename M2>
void MatrixMultiply (MR mr, M1 m1, M2 m2)
{
    AMBRO_ASSERT(m1.cols() == m2.rows())
    AMBRO_ASSERT(mr.rows() == m1.rows())
    AMBRO_ASSERT(mr.cols() == m2.cols())
    
    int rows = m1.rows();
    int cols = m2.cols();
    int mid = m1.cols();
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            mr(i, j) = 0;
            for (int k = 0; k < mid; k++) {
                mr(i, j) += m1(i, k) * m2(k, j);
            }
        }
    }
}

template <typename MR, typename M1>
void MatrixMultiplyMMT (MR mr, M1 m1)
{
    AMBRO_ASSERT(mr.rows() == m1.rows())
    AMBRO_ASSERT(mr.cols() == m1.rows())
    
    int rows = m1.rows();
    int mid = m1.cols();
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < rows; j++) {
            mr(i, j) = 0;
            for (int k = 0; k < mid; k++) {
                mr(i, j) += m1(i, k) * m1(j, k);
            }
        }
    }
}

template <typename M1>
typename M1::T MatrixSquareNorm (M1 m1)
{
    typename M1::T res = 0;
    int rows = m1.rows();
    int cols = m1.cols();
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            res += m1(i, j) * m1(i, j);
        }
    }
    return res;
}


struct MatrixElemOpAdd {
    template <typename T>
    static T op (T v1, T v2)
    {
        return v1 + v2;
    }
};

struct MatrixElemOpSubtract {
    template <typename T>
    static T op (T v1, T v2)
    {
        return v1 - v2;
    }
};

struct MatrixElemOpMultiply {
    template <typename T>
    static T op (T v1, T v2)
    {
        return v1 * v2;
    }
};

struct MatrixElemOpDivide {
    template <typename T>
    static T op (T v1, T v2)
    {
        return v1 / v2;
    }
};

struct MatrixElemOpV2 {
    template <typename T>
    static T op (T v1, T v2)
    {
        return v2;
    }
};

template <typename Op, typename M1, typename M2, typename MR>
void MatrixElemOp (M1 m1, M2 m2, MR mr)
{
    AMBRO_ASSERT(m1.rows() == m2.rows())
    AMBRO_ASSERT(m1.cols() == m2.cols())
    AMBRO_ASSERT(mr.rows() == m1.rows())
    AMBRO_ASSERT(mr.cols() == m1.cols())
    
    int rows = m1.rows();
    int cols = m1.cols();
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            mr(i, j) = Op::op(m1(i, j), m2(i, j));
        }
    }
}

template <typename Op, typename M1, typename M2>
void MatrixElemOpInPlace (M1 m1, M2 m2)
{
    AMBRO_ASSERT(m1.rows() == m2.rows())
    AMBRO_ASSERT(m1.cols() == m2.cols())
    
    int rows = m1.rows();
    int cols = m1.cols();
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            m1(i, j) = Op::op(m1(i, j), m2(i, j));
        }
    }
}

template <typename Op, typename M1, typename MR>
void MatrixElemOpScalar (M1 m1, typename M1::T s, MR mr)
{
    AMBRO_ASSERT(mr.rows() == m1.rows())
    AMBRO_ASSERT(mr.cols() == m1.cols())
    
    int rows = m1.rows();
    int cols = m1.cols();
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            mr(i, j) = Op::op(m1(i, j), s);
        }
    }
}

struct MatrixElemPredicateTrue {
    static const bool check (int i, int j) { return true; }
};

struct MatrixElemPredicateDiagonal {
    static const bool check (int i, int j) { return (i == j); }
};

template <typename Op, typename M1, typename Predicate = MatrixElemPredicateTrue>
void MatrixElemOpScalarInPlace (M1 m1, typename M1::T s, Predicate p = Predicate())
{
    int rows = m1.rows();
    int cols = m1.cols();
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            if (p.check(i, j)) {
                m1(i, j) = Op::op(m1(i, j), s);
            }
        }
    }
}

#include <aprinter/EndNamespace.h>

#endif
