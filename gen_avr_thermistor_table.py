"""
/*
 * Copyright (c) 2013 Ambroz Bizjak
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
"""

import sys
import math

if len(sys.argv) != 7:
    print("Usage: <Name> <ResistorR> <ThermistorR0> <ThermistorBeta> <StartTemp> <EndTemp>")
    exit(1)

Name = sys.argv[1]
ResistorR = float(sys.argv[2])
ThermistorR0 = float(sys.argv[3])
ThermistorBeta = float(sys.argv[4])
StartTemp = float(sys.argv[5])
EndTemp = float(sys.argv[6])

ScaleFactorExp = 7

table_data = []
starting = True
stopped = False
for i in range(1024):
    voltage_frac = i / 1024.0
    r_thermistor = ResistorR * (voltage_frac / (1.0 - voltage_frac))
    r_inf = ThermistorR0 * math.exp(-ThermistorBeta / 298.15)
    if r_thermistor / r_inf <= 0.0:
        assert starting
        continue
    t = (ThermistorBeta / math.log(r_thermistor / r_inf)) - 273.15
    if t > EndTemp:
        assert starting
        continue
    if starting:
        starting = False
        table_offset = i
    if t < StartTemp:
        assert not starting
        stopped = True
        table_end = i
        break
    value = int(t * 2**ScaleFactorExp)
    assert value >= 0
    assert value < 2**16
    table_data.append("UINT16_C(%s), " % (value))
    if i % 8 == 0:
        table_data.append("\n")

assert not starting
assert stopped
table_size = table_end - table_offset

print("""/*
 * Copyright (c) 2013 Ambroz Bizjak
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

/* 
 * This file was automatically generated using gen_avr_thermistor_table.py.
 * The following parameters were used for generation:
 * 
 * Name = %(Name)s
 * ResistorR = %(ResistorR)s
 * ThermistorR0 = %(ThermistorR0)s
 * ThermistorBeta = %(ThermistorBeta)s
 * StartTemp = %(StartTemp)s
 * EndTemp = %(EndTemp)s
 * 
 * The file can be regenerated with the following command:
 * 
 * python gen_avr_thermistor_table.py "%(Name)s" %(ResistorR)s %(ThermistorR0)s %(ThermistorBeta)s %(StartTemp)s %(EndTemp)s
 */

#ifndef AMBROLIB_AVR_THERMISTOR_%(Name)s_H
#define AMBROLIB_AVR_THERMISTOR_%(Name)s_H

#include <stdint.h>
#include <avr/pgmspace.h>

#include <aprinter/meta/FixedPoint.h>
#include <aprinter/base/Likely.h>

#include <aprinter/BeginNamespace.h>

class AvrThermistorTable_%(Name)s {
public:
    using ValueFixedType = FixedPoint<16, false, -%(ScaleFactorExp)s>;
    
    static ValueFixedType call (uint16_t adc_value)
    {
        if (AMBRO_UNLIKELY(adc_value < %(TableOffset)s)) {
            return ValueFixedType::maxValue();
        }
        if (AMBRO_UNLIKELY(adc_value > %(TableEnd)s - 1)) {
            return ValueFixedType::minValue();
        }
        return ValueFixedType::importBits(pgm_read_word(&table[(adc_value - %(TableOffset)s)]));
    }
    
private:
    static uint16_t const table[%(TableSize)s] PROGMEM;
};

uint16_t const AvrThermistorTable_%(Name)s::table[%(TableSize)s] PROGMEM = {
%(TableData)s
};

#include <aprinter/EndNamespace.h>

#endif """
% {
    'Name':Name, 'TableData':''.join(table_data), 'TableSize':table_size, 'TableOffset':table_offset,
    'TableEnd':table_end, 'ResistorR':ResistorR, 'ThermistorBeta':ThermistorBeta,
    'ThermistorR0':ThermistorR0, 'StartTemp':StartTemp, 'EndTemp':EndTemp, 'ScaleFactorExp':ScaleFactorExp
})
