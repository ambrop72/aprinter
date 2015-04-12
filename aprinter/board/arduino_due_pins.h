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

#ifndef AMBROLIB_ARDUINO_DUE_PINS_H
#define AMBROLIB_ARDUINO_DUE_PINS_H

#include <aprinter/system/At91SamPins.h>

#include <aprinter/BeginNamespace.h>

using DuePin0 = At91SamPin<At91SamPioA, 8>;
using DuePin1 = At91SamPin<At91SamPioA, 9>;
using DuePin2 = At91SamPin<At91SamPioB, 25>;
using DuePin3 = At91SamPin<At91SamPioC, 28>;
using DuePin4 = At91SamPin<At91SamPioC, 26>;
using DuePin5 = At91SamPin<At91SamPioC, 25>;
using DuePin6 = At91SamPin<At91SamPioC, 24>;
using DuePin7 = At91SamPin<At91SamPioC, 23>;
using DuePin8 = At91SamPin<At91SamPioC, 22>;
using DuePin9 = At91SamPin<At91SamPioC, 21>;
using DuePin10 = At91SamPin<At91SamPioC, 29>;
using DuePin11 = At91SamPin<At91SamPioD, 7>;
using DuePin12 = At91SamPin<At91SamPioD, 8>;
using DuePin13 = At91SamPin<At91SamPioB, 27>;
using DuePin14 = At91SamPin<At91SamPioD, 4>;
using DuePin15 = At91SamPin<At91SamPioD, 5>;
using DuePin16 = At91SamPin<At91SamPioA, 13>;
using DuePin17 = At91SamPin<At91SamPioA, 12>;
using DuePin18 = At91SamPin<At91SamPioA, 11>;
using DuePin19 = At91SamPin<At91SamPioA, 10>;
using DuePin20 = At91SamPin<At91SamPioB, 12>;
using DuePin21 = At91SamPin<At91SamPioB, 13>;
using DuePin22 = At91SamPin<At91SamPioB, 26>;
using DuePin23 = At91SamPin<At91SamPioA, 14>;
using DuePin24 = At91SamPin<At91SamPioA, 15>;
using DuePin25 = At91SamPin<At91SamPioD, 0>;
using DuePin26 = At91SamPin<At91SamPioD, 1>;
using DuePin27 = At91SamPin<At91SamPioD, 2>;
using DuePin28 = At91SamPin<At91SamPioD, 3>;
using DuePin29 = At91SamPin<At91SamPioD, 6>;
using DuePin30 = At91SamPin<At91SamPioD, 9>;
using DuePin31 = At91SamPin<At91SamPioA, 7>;
using DuePin32 = At91SamPin<At91SamPioD, 10>;
using DuePin33 = At91SamPin<At91SamPioC, 1>;
using DuePin34 = At91SamPin<At91SamPioC, 2>;
using DuePin35 = At91SamPin<At91SamPioC, 3>;
using DuePin36 = At91SamPin<At91SamPioC, 4>;
using DuePin37 = At91SamPin<At91SamPioC, 5>;
using DuePin38 = At91SamPin<At91SamPioC, 6>;
using DuePin39 = At91SamPin<At91SamPioC, 7>;
using DuePin40 = At91SamPin<At91SamPioC, 8>;
using DuePin41 = At91SamPin<At91SamPioC, 9>;
using DuePin42 = At91SamPin<At91SamPioA, 19>;
using DuePin43 = At91SamPin<At91SamPioA, 20>;
using DuePin44 = At91SamPin<At91SamPioC, 19>;
using DuePin45 = At91SamPin<At91SamPioC, 18>;
using DuePin46 = At91SamPin<At91SamPioC, 17>;
using DuePin47 = At91SamPin<At91SamPioC, 16>;
using DuePin48 = At91SamPin<At91SamPioC, 15>;
using DuePin49 = At91SamPin<At91SamPioC, 14>;
using DuePin50 = At91SamPin<At91SamPioC, 13>;
using DuePin51 = At91SamPin<At91SamPioC, 12>;
using DuePin52 = At91SamPin<At91SamPioB, 21>;
using DuePin53 = At91SamPin<At91SamPioB, 14>;
using DuePin54 = At91SamPin<At91SamPioA, 16>;
using DuePin55 = At91SamPin<At91SamPioA, 24>;
using DuePin56 = At91SamPin<At91SamPioA, 23>;
using DuePin57 = At91SamPin<At91SamPioA, 22>;
using DuePin58 = At91SamPin<At91SamPioA, 6>;
using DuePin59 = At91SamPin<At91SamPioA, 4>;
using DuePin60 = At91SamPin<At91SamPioA, 3>;
using DuePin61 = At91SamPin<At91SamPioA, 2>;
using DuePin62 = At91SamPin<At91SamPioB, 17>;
using DuePin63 = At91SamPin<At91SamPioB, 18>;
using DuePin64 = At91SamPin<At91SamPioB, 19>;
using DuePin65 = At91SamPin<At91SamPioB, 20>;
using DuePin66 = At91SamPin<At91SamPioB, 15>;
using DuePin67 = At91SamPin<At91SamPioB, 16>;
using DuePin68 = At91SamPin<At91SamPioA, 1>;
using DuePin69 = At91SamPin<At91SamPioA, 0>;
using DuePin70 = At91SamPin<At91SamPioA, 17>;
using DuePin71 = At91SamPin<At91SamPioA, 18>;
using DuePin72 = At91SamPin<At91SamPioC, 30>;
using DuePin73 = At91SamPin<At91SamPioA, 21>;
using DuePin74 = At91SamPin<At91SamPioA, 25>;
using DuePin75 = At91SamPin<At91SamPioA, 26>;
using DuePin76 = At91SamPin<At91SamPioA, 27>;
using DuePin77 = At91SamPin<At91SamPioA, 28>;
using DuePin78 = At91SamPin<At91SamPioB, 23>;

using DuePinA0 = DuePin54;
using DuePinA1 = DuePin55;
using DuePinA2 = DuePin56;
using DuePinA3 = DuePin57;
using DuePinA4 = DuePin58;
using DuePinA5 = DuePin59;
using DuePinA6 = DuePin60;
using DuePinA7 = DuePin61;
using DuePinA8 = DuePin62;
using DuePinA9 = DuePin63;
using DuePinA10 = DuePin64;
using DuePinA11 = DuePin65;
using DuePinA12 = DuePin66;
using DuePinA13 = DuePin67;
using DuePinA14 = DuePin68;
using DuePinA15 = DuePin69;

#include <aprinter/EndNamespace.h>

#endif
