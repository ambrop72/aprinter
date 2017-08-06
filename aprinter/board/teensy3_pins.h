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

#ifndef AMBROLIB_TEENSY3_PINS_H
#define AMBROLIB_TEENSY3_PINS_H

#include <aprinter/hal/teensy3/Mk20Pins.h>

namespace APrinter {

using TeensyPin0 = Mk20Pin<Mk20PortB, 16>;
using TeensyPin1 = Mk20Pin<Mk20PortB, 17>;
using TeensyPin2 = Mk20Pin<Mk20PortD, 0>;
using TeensyPin3 = Mk20Pin<Mk20PortA, 12>;
using TeensyPin4 = Mk20Pin<Mk20PortA, 13>;
using TeensyPin5 = Mk20Pin<Mk20PortD, 7>;
using TeensyPin6 = Mk20Pin<Mk20PortD, 4>;
using TeensyPin7 = Mk20Pin<Mk20PortD, 2>;
using TeensyPin8 = Mk20Pin<Mk20PortD, 3>;
using TeensyPin9 = Mk20Pin<Mk20PortC, 3>;
using TeensyPin10 = Mk20Pin<Mk20PortC, 4>;
using TeensyPin11 = Mk20Pin<Mk20PortC, 6>;
using TeensyPin12 = Mk20Pin<Mk20PortC, 7>;
using TeensyPin13 = Mk20Pin<Mk20PortC, 5>;
using TeensyPin14 = Mk20Pin<Mk20PortD, 1>;
using TeensyPin15 = Mk20Pin<Mk20PortC, 0>;
using TeensyPin16 = Mk20Pin<Mk20PortB, 0>;
using TeensyPin17 = Mk20Pin<Mk20PortB, 1>;
using TeensyPin18 = Mk20Pin<Mk20PortB, 3>;
using TeensyPin19 = Mk20Pin<Mk20PortB, 2>;
using TeensyPin20 = Mk20Pin<Mk20PortD, 5>;
using TeensyPin21 = Mk20Pin<Mk20PortD, 6>;
using TeensyPin22 = Mk20Pin<Mk20PortC, 1>;
using TeensyPin23 = Mk20Pin<Mk20PortC, 2>;
using TeensyPin24 = Mk20Pin<Mk20PortA, 5>;
using TeensyPin25 = Mk20Pin<Mk20PortB, 19>;
using TeensyPin26 = Mk20Pin<Mk20PortE, 1>;
using TeensyPin27 = Mk20Pin<Mk20PortC, 9>;
using TeensyPin28 = Mk20Pin<Mk20PortC, 8>;
using TeensyPin29 = Mk20Pin<Mk20PortC, 10>;
using TeensyPin30 = Mk20Pin<Mk20PortC, 11>;
using TeensyPin31 = Mk20Pin<Mk20PortE, 0>;
using TeensyPin32 = Mk20Pin<Mk20PortB, 18>;
using TeensyPin33 = Mk20Pin<Mk20PortA, 4>;

using TeensyPinA0 = TeensyPin14;
using TeensyPinA1 = TeensyPin15;
using TeensyPinA2 = TeensyPin16;
using TeensyPinA3 = TeensyPin17;
using TeensyPinA4 = TeensyPin18;
using TeensyPinA5 = TeensyPin19;
using TeensyPinA6 = TeensyPin20;
using TeensyPinA7 = TeensyPin21;
using TeensyPinA8 = TeensyPin22;
using TeensyPinA9 = TeensyPin23;

}

#endif
