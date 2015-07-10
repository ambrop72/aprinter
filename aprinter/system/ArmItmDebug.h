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

#ifndef AMBROLIB_ARM_ITM_DEBUG_H
#define AMBROLIB_ARM_ITM_DEBUG_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, int ItmStimulusPort>
class ArmItmDebug {
public:
    static void write (Context c, char const *data, size_t len)
    {
        while (len > 0) {
            do {
                if ((ITM->TCR & ITM_TCR_ITMENA_Msk) == 0 || (ITM->TER & ((uint32_t)1 << ItmStimulusPort)) == 0) {
                    return;
                }
            } while (ITM->PORT[ItmStimulusPort].u32 == 0);
            
            size_t packet_len;
            if (len >= 4) {
                packet_len = 4;
                uint32_t packet;
                memcpy(&packet, data, packet_len);
                ITM->PORT[ItmStimulusPort].u32 = packet;
            } else if (len >= 2) {
                packet_len = 2;
                uint16_t packet;
                memcpy(&packet, data, packet_len);
                ITM->PORT[ItmStimulusPort].u16 = packet;
            } else {
                packet_len = 1;
                uint8_t packet;
                memcpy(&packet, data, packet_len);
                ITM->PORT[ItmStimulusPort].u8 = packet;
            }
            
            data += packet_len;
            len -= packet_len;
        }
    }
};

#include <aprinter/EndNamespace.h>

#endif
