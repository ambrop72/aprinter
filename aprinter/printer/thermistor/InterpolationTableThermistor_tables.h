/*
 * Copyright (c) 2017 Ambroz Bizjak
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

#ifndef APRINTER_INTERPOLATION_TABLE_THERMISTOR_TABLES_H
#define APRINTER_INTERPOLATION_TABLE_THERMISTOR_TABLES_H

#include "InterpolationTableThermistor.h"

namespace APrinter {

// NOTE: Tables must always be sorted by voltage regardless of
// positive/negative slope.

APRINTER_DEFINE_INTERPOLATION_TABLE(InterpolationTableE3dPt100, false, 5.0, ({
    {1.11,  1.0},
    {1.15,  10.0},
    {1.20,  20.0},
    {1.24,  30.0},
    {1.28,  40.0},
    {1.32,  50.0},
    {1.36,  60.0},
    {1.40,  70.0},
    {1.44,  80.0},
    {1.48,  90.0},
    {1.52,  100.0},
    {1.56,  110.0},
    {1.61,  120.0},
    {1.65,  130.0},
    {1.68,  140.0},
    {1.72,  150.0},
    {1.76,  160.0},
    {1.80,  170.0},
    {1.84,  180.0},
    {1.88,  190.0},
    {1.92,  200.0},
    {1.96,  210.0},
    {2.00,  220.0},
    {2.04,  230.0},
    {2.07,  240.0},
    {2.11,  250.0},
    {2.15,  260.0},
    {2.18,  270.0},
    {2.22,  280.0},
    {2.26,  290.0},
    {2.29,  300.0},
    {2.33,  310.0},
    {2.37,  320.0},
    {2.41,  330.0},
    {2.44,  340.0},
    {2.48,  350.0},
    {2.51,  360.0},
    {2.55,  370.0},
    {2.58,  380.0},
    {2.62,  390.0},
    {2.66,  400.0},
    {3.00,  500.0},
    {3.33,  600.0},
    {3.63,  700.0},
    {3.93,  800.0},
    {4.21,  900.0},
    {4.48,  1000.0},
    {4.73,  1100.0},
}))

}

#endif
