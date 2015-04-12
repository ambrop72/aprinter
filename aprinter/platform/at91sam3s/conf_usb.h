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

#ifndef CONF_USB_H
#define CONF_USB_H

#define USB_DEVICE_VENDOR_ID 0x2341
#define USB_DEVICE_PRODUCT_ID 0x003e
#define USB_DEVICE_MAJOR_VERSION 1
#define USB_DEVICE_MINOR_VERSION 0
#define USB_DEVICE_POWER 100
#define USB_DEVICE_ATTR USB_CONFIG_ATTR_BUS_POWERED

#define UDI_CDC_ENABLE_EXT(port) true
#define UDI_CDC_DISABLE_EXT(port)
#define UDI_CDC_SET_DTR_EXT(port, set)
#define UDI_CDC_SET_RTS_EXT(port, set)
#define UDI_CDC_SET_CODING_EXT(port, cfg)
#define UDI_CDC_RX_NOTIFY(port)

#define UDI_CDC_LOW_RATE
#define UDI_CDC_DEFAULT_RATE 1000000
#define UDI_CDC_DEFAULT_STOPBITS CDC_STOP_BITS_1
#define UDI_CDC_DEFAULT_PARITY CDC_PAR_NONE
#define UDI_CDC_DEFAULT_DATABITS 8

#include "udi_cdc_conf.h"

#endif
