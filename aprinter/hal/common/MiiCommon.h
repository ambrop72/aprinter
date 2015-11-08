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

#ifndef APRINTER_MII_COMMON_H
#define APRINTER_MII_COMMON_H

#include <stdint.h>

#include <aprinter/BeginNamespace.h>

template <
    typename TActivateHandler,
    typename TPhyMaintHandler,
    typename TReceiveHandler,
    typename TSendBufferType,
    bool TRmii
>
struct MiiClientParams {
    using ActivateHandler = TActivateHandler;
    using PhyMaintHandler = TPhyMaintHandler;
    using ReceiveHandler = TReceiveHandler;
    using SendBufferType = TSendBufferType;
    static bool const Rmii = TRmii;
};

enum class PhyMaintCommandIoType : uint8_t {READ_ONLY, READ_WRITE};

struct MiiPhyMaintCommand {
    uint8_t phy_address;
    uint8_t reg_address;
    PhyMaintCommandIoType io_type;
    uint16_t data;
};

struct MiiPhyMaintResult {
    bool error;
    uint16_t data;
};

namespace MiiSpeed { enum : uint8_t {
    SPEED_10M_HD  = 1 << 0,
    SPEED_10M_FD  = 1 << 1,
    SPEED_100M_HD = 1 << 2,
    SPEED_100M_FD = 1 << 3
}; }

namespace MiiPauseAbility { enum : uint8_t {
    RX_AND_TX = 1 << 0,
    RX_ONLY   = 1 << 1,
    TX_ONLY   = 1 << 2
}; }

namespace MiiPauseConfig { enum : uint8_t {
    RX_ENABLE = 1 << 0,
    TX_ENABLE = 1 << 1
}; }

struct MiiLinkParams {
    uint8_t speed;
    uint8_t pause_config;
};

template <
    typename TPhyRequester
>
struct PhyClientParams {
    using PhyRequester = TPhyRequester;
};

#include <aprinter/EndNamespace.h>

#endif
