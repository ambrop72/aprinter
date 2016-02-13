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

#ifndef APRINTER_SDIO_INTERFACE_H
#define APRINTER_SDIO_INTERFACE_H

#include <stdint.h>
#include <stddef.h>

#include <aprinter/BeginNamespace.h>

namespace SdioIface {
    struct InterfaceParams {
        bool clock_full_speed;
        bool wide_data_bus;
    };
    
    enum ResponseType {
        RESPONSE_NONE,
        RESPONSE_SHORT,
        RESPONSE_LONG
    };
    
    enum {
        CMD_FLAG_NO_CRC_CHECK = 1 << 0,
        CMD_FLAG_NO_CMDNUM_CHECK = 1 << 1
    };
    
    enum DataDirection {
        DATA_DIR_NONE,
        DATA_DIR_READ,
        DATA_DIR_WRITE
    };
    
    struct CommandParams {
        uint8_t cmd_index;
        uint32_t argument;
        ResponseType response_type;
        uint8_t flags;
        DataDirection direction;
        size_t num_blocks;
        uint32_t *data_ptr;
    };
    
    enum CommandErrorCode {
        CMD_ERROR_NONE,
        CMD_ERROR_RESPONSE_TIMEOUT,
        CMD_ERROR_RESPONSE_CHECKSUM,
        CMD_ERROR_BAD_RESPONSE_CMD,
        CMD_ERROR_OTHER
    };
    
    enum DataErrorCode {
        DATA_ERROR_NONE,
        DATA_ERROR_CHECKSUM,
        DATA_ERROR_TIMEOUT,
        DATA_ERROR_RX_OVERRUN,
        DATA_ERROR_TX_OVERRUN,
        DATA_ERROR_STBITER,
        DATA_ERROR_DMA
    };
    
    struct CommandResults {
        CommandErrorCode error_code;
        uint32_t response[4];
    };
}

#include <aprinter/EndNamespace.h>

#endif
