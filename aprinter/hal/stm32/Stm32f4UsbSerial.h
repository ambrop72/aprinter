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

#ifndef APRINTER_STM32F4_USB_SERIAL_H
#define APRINTER_STM32F4_USB_SERIAL_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include <usbd_core.h>
#include <usbd_cdc.h>

#include <aprinter/meta/BoundedInt.h>
#include <aprinter/meta/TypeListUtils.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Callback.h>
#include <aprinter/system/InterruptLock.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, int RecvBufferBits, int SendBufferBits, typename RecvHandler, typename SendHandler, typename Params>
class Stm32f4UsbSerial {
private:
    using RecvFastEvent = typename Context::EventLoop::template FastEventSpec<Stm32f4UsbSerial>;
    using SendFastEvent = typename Context::EventLoop::template FastEventSpec<RecvFastEvent>;
    
public:
    struct Object;
    using RecvSizeType = BoundedInt<RecvBufferBits, false>;
    using SendSizeType = BoundedInt<SendBufferBits, false>;
    
private:
    using TheDebugObject = DebugObject<Context, Object>;
    
    static size_t const UsbRxBufferSize = 1024;
    static_assert(UsbRxBufferSize >= CDC_DATA_HS_OUT_PACKET_SIZE, "");
    static_assert(UsbRxBufferSize >= CDC_DATA_FS_OUT_PACKET_SIZE, "");
    
    // TBD: Is this too much, what is the right limit on tx size?
    static size_t const UsbMaxTxSize = 1024;
    
public:
    static void init (Context c, uint32_t baud)
    {
        auto *o = Object::self(c);
        
        Context::EventLoop::template initFastEvent<RecvFastEvent>(c, Stm32f4UsbSerial::recv_event_handler);
        o->m_recv_start = RecvSizeType::import(0);
        o->m_recv_end = RecvSizeType::import(0);
        o->m_recv_force = false;
        o->m_recv_rx_active = true;
        
        Context::EventLoop::template initFastEvent<SendFastEvent>(c, Stm32f4UsbSerial::send_event_handler);
        o->m_send_start = SendSizeType::import(0);
        o->m_send_end = SendSizeType::import(0);
        o->m_send_event = SendSizeType::import(0);
        o->m_send_tx_active = false;
        
        o->m_line_coding.bitrate = 115200;
        o->m_line_coding.format = 0;
        o->m_line_coding.paritytype = 0;
        o->m_line_coding.datatype = 8;
        
        o->m_cdc_ops.Init =        Stm32f4UsbSerial::cdc_cb_init;
        o->m_cdc_ops.DeInit =      Stm32f4UsbSerial::cdc_cb_deinit;
        o->m_cdc_ops.Control =     Stm32f4UsbSerial::cdc_cb_control;
        o->m_cdc_ops.Receive =     Stm32f4UsbSerial::cdc_cb_receive;
        o->m_cdc_ops.TxCompleted = Stm32f4UsbSerial::cdc_cb_tx_completed;
        
        if (USBD_RegisterClass(&USBD_Device, USBD_CDC_CLASS) != USBD_OK) {
            AMBRO_ASSERT_ABORT("USBD_RegisterClass failed");
        }
        
        if (USBD_CDC_RegisterInterface(&USBD_Device, &o->m_cdc_ops) != USBD_OK) {
            AMBRO_ASSERT_ABORT("USBD_CDC_RegisterInterface failed");
        }
        
        TheDebugObject::init(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::deinit(c);
        
        Context::EventLoop::template resetFastEvent<SendFastEvent>(c);
        Context::EventLoop::template resetFastEvent<RecvFastEvent>(c);
    }
    
    static RecvSizeType recvQuery (Context c, bool *out_overrun)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(out_overrun)
        
        *out_overrun = false;
        return recv_avail(o->m_recv_start, o->m_recv_end);
    }
    
    static char * recvGetChunkPtr (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        return (o->m_recv_buffer + o->m_recv_start.value());
    }
    
    static void recvConsume (Context c, RecvSizeType amount)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        AMBRO_ASSERT(amount <= recv_avail(o->m_recv_start, o->m_recv_end))
        o->m_recv_start = BoundedModuloAdd(o->m_recv_start, amount);
        Context::EventLoop::template triggerFastEvent<RecvFastEvent>(c);
    }
    
    static void recvClearOverrun (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        AMBRO_ASSERT(false)
    }
    
    static void recvForceEvent (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        o->m_recv_force = true;
        Context::EventLoop::template triggerFastEvent<RecvFastEvent>(c);
    }
    
    static SendSizeType sendQuery (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        return send_avail(o->m_send_start, o->m_send_end);
    }
    
    static SendSizeType sendGetChunkLen (Context c, SendSizeType rem_length)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        if (o->m_send_end.value() > 0 && rem_length > BoundedModuloNegative(o->m_send_end)) {
            rem_length = BoundedModuloNegative(o->m_send_end);
        }
        
        return rem_length;
    }
    
    static char * sendGetChunkPtr (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        return (o->m_send_buffer + o->m_send_end.value());
    }
    
    static void sendProvide (Context c, SendSizeType amount)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        AMBRO_ASSERT(amount <= send_avail(o->m_send_start, o->m_send_end))
        o->m_send_end = BoundedModuloAdd(o->m_send_end, amount);
    }
    
    static void sendPoke (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        Context::EventLoop::template triggerFastEvent<SendFastEvent>(c);
    }
    
    static void sendRequestEvent (Context c, SendSizeType min_amount)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        o->m_send_event = min_amount;
        Context::EventLoop::template triggerFastEvent<SendFastEvent>(c);
    }
    
    using EventLoopFastEvents = MakeTypeList<RecvFastEvent, SendFastEvent>;
    
private:
    static RecvSizeType recv_avail (RecvSizeType start, RecvSizeType end)
    {
        return BoundedModuloSubtract(end, start);
    }
    
    static SendSizeType send_avail (SendSizeType start, SendSizeType end)
    {
        return BoundedModuloDec(BoundedModuloSubtract(start, end));
    }
    
    static void recv_event_handler (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        bool rx_active;
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            rx_active = o->m_recv_rx_active;
        }
        
        if (!rx_active) {
            RecvSizeType virtual_start = BoundedModuloDec(o->m_recv_start);
            while (o->m_recv_rx_buffer_pos < o->m_recv_rx_buffer_len && o->m_recv_end != virtual_start) {
                RecvSizeType amount = (o->m_recv_end > virtual_start) ? BoundedModuloNegative(o->m_recv_end) : BoundedUnsafeSubtract(virtual_start, o->m_recv_end);
                size_t remaining = o->m_recv_rx_buffer_len - o->m_recv_rx_buffer_pos;
                if (amount.m_int > remaining) {
                    amount.m_int = remaining;
                }
                char const *src = o->m_recv_rx_buffer + o->m_recv_rx_buffer_pos;
                char *dst = o->m_recv_buffer + o->m_recv_end.value();
                memcpy(dst, src, amount.value());
                memcpy(dst + ((size_t)RecvSizeType::maxIntValue() + 1), src, amount.value());
                o->m_recv_rx_buffer_pos += amount.value();
                o->m_recv_end = BoundedModuloAdd(o->m_recv_end, amount);
                o->m_recv_force = true;
            }
            
            if (o->m_recv_rx_buffer_pos == o->m_recv_rx_buffer_len) {
                o->m_recv_rx_active = true;
                USBD_CDC_ReceivePacket(&USBD_Device);
            }
        }
        
        if (o->m_recv_force) {
            o->m_recv_force = false;
            RecvHandler::call(c);
        }
    }
    
    static void send_event_handler (Context c)
    {
        auto *o = Object::self(c);
        TheDebugObject::access(c);
        
        if (o->m_send_tx_active && USBD_CDC_CheckTxBusy(&USBD_Device) != USBD_BUSY) {
            o->m_send_tx_active = false;
            o->m_send_start = BoundedModuloAdd(o->m_send_start, o->m_send_tx_len);
        }
        
        if (!o->m_send_tx_active && o->m_send_start != o->m_send_end) {
            SendSizeType amount = (o->m_send_end < o->m_send_start) ? BoundedModuloNegative(o->m_send_start) : BoundedUnsafeSubtract(o->m_send_end, o->m_send_start);
            if (amount.m_int > UsbMaxTxSize) {
                amount.m_int = UsbMaxTxSize;
            }
            USBD_CDC_SetTxBuffer(&USBD_Device, (uint8_t *)(o->m_send_buffer + o->m_send_start.value()), amount.value());
            if (USBD_CDC_TransmitPacket(&USBD_Device) == USBD_OK) {
                o->m_send_tx_active = true;
                o->m_send_tx_len = amount;
            }
        }
        
        if (o->m_send_event != SendSizeType::import(0) && send_avail(o->m_send_start, o->m_send_end) >= o->m_send_event) {
            o->m_send_event = SendSizeType::import(0);
            SendHandler::call(c);
        }
    }
    
    static int8_t cdc_cb_init (void)
    {
        auto c = Context();
        auto *o = Object::self(c);
        
        USBD_CDC_SetRxBuffer(&USBD_Device, (uint8_t *)o->m_recv_rx_buffer);
        return USBD_OK;
    }
    
    static int8_t cdc_cb_deinit (void)
    {
        return USBD_OK;
    }
    
    static int8_t cdc_cb_control (uint8_t cmd, uint8_t *pbuf, uint16_t length)
    {
        auto c = MakeInterruptContext(Context());
        auto *o = Object::self(c);
        
        switch (cmd) {
            case CDC_SET_LINE_CODING: {
                o->m_line_coding.bitrate = ((uint32_t)pbuf[0] << 0) | ((uint32_t)pbuf[1] << 8) | ((uint32_t)pbuf[2] << 16) | ((uint32_t)pbuf[3] << 24);
                o->m_line_coding.format = pbuf[4];
                o->m_line_coding.paritytype = pbuf[5];
                o->m_line_coding.datatype = pbuf[6];
            } break;
            case CDC_GET_LINE_CODING: {
                pbuf[0] = o->m_line_coding.bitrate >> 0;
                pbuf[1] = o->m_line_coding.bitrate >> 8;
                pbuf[2] = o->m_line_coding.bitrate >> 16;
                pbuf[3] = o->m_line_coding.bitrate >> 24;
                pbuf[4] = o->m_line_coding.format;
                pbuf[5] = o->m_line_coding.paritytype;
                pbuf[6] = o->m_line_coding.datatype;
            } break;
        }
        return USBD_OK;
    }
    
    static int8_t cdc_cb_receive (uint8_t *pbuf, uint32_t *len)
    {
        auto c = MakeInterruptContext(Context());
        auto *o = Object::self(c);
        AMBRO_ASSERT_FORCE(o->m_recv_rx_active)
        AMBRO_ASSERT_FORCE(*len <= UsbRxBufferSize)
        
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            o->m_recv_rx_active = false;
            o->m_recv_rx_buffer_len = *len;
            o->m_recv_rx_buffer_pos = 0;
        }
        Context::EventLoop::template triggerFastEvent<RecvFastEvent>(c);
        return USBD_OK;
    }
    
    static void cdc_cb_tx_completed ()
    {
        auto c = MakeInterruptContext(Context());
        
        Context::EventLoop::template triggerFastEvent<SendFastEvent>(c);
    }
    
public:
    struct Object : public ObjBase<Stm32f4UsbSerial, ParentObject, MakeTypeList<TheDebugObject>> {
        RecvSizeType m_recv_start;
        RecvSizeType m_recv_end;
        bool m_recv_force;
        bool m_recv_rx_active;
        size_t m_recv_rx_buffer_len;
        size_t m_recv_rx_buffer_pos;
        char m_recv_buffer[2 * ((size_t)RecvSizeType::maxIntValue() + 1)];
        char m_recv_rx_buffer[UsbRxBufferSize];
        SendSizeType m_send_start;
        SendSizeType m_send_end;
        SendSizeType m_send_event;
        bool m_send_tx_active;
        SendSizeType m_send_tx_len;
        char m_send_buffer[(size_t)SendSizeType::maxIntValue() + 1];
        USBD_CDC_LineCodingTypeDef m_line_coding;
        USBD_CDC_ItfTypeDef m_cdc_ops;
    };
};

struct Stm32f4UsbSerialService {
    template <typename Context, typename ParentObject, int RecvBufferBits, int SendBufferBits, typename RecvHandler, typename SendHandler>
    using Serial = Stm32f4UsbSerial<Context, ParentObject, RecvBufferBits, SendBufferBits, RecvHandler, SendHandler, Stm32f4UsbSerialService>;
};

#include <aprinter/EndNamespace.h>

#endif
