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

#ifndef AMBROLIB_STM32F4_USB_H
#define AMBROLIB_STM32F4_USB_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include <usb_regs.h>
#include <usb_defines.h>

#include <aprinter/meta/Object.h>
#include <aprinter/meta/MakeTypeList.h>
#include <aprinter/meta/MinMax.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Lock.h>
#include <aprinter/system/InterruptLock.h>
#include <aprinter/usb/usb_proto.h>

#include <aprinter/BeginNamespace.h>

template <
    uint32_t TOtgAddress,
    uint8_t TTrdtim,
    uint8_t TToutcal,
    enum IRQn TIrq
>
struct Stm32f4UsbInfo {
    static uint32_t const OtgAddress = TOtgAddress;
    static uint8_t const Trdtim = TTrdtim;
    static uint8_t const Toutcal = TToutcal;
    static enum IRQn const Irq = TIrq;
};

using Stm32F4UsbInfoFS = Stm32f4UsbInfo<
    USB_OTG_FS_BASE_ADDR,
    5,
    7,
    OTG_FS_IRQn
>;

template <typename Context, typename ParentObject, typename Info>
class Stm32f4Usb {
private:
    using FastEvent = typename Context::EventLoop::template FastEventSpec<Stm32f4Usb>;
    
public:
    struct Object;
    
    enum State {
        STATE_WAITING_RESET = 0,
        STATE_WAITING_ENUM = 1,
        STATE_ENUM_DONE = 2,
        STATE_TEST = 3
    };
    
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        memset(&o->device_desc, 0, sizeof(o->device_desc));
        o->device_desc.bLength = sizeof(o->device_desc);
        o->device_desc.bDescriptorType = USB_DESSCRIPTOR_TYPE_DEVICE;
        o->device_desc.bcdUSB = UINT16_C(0x0200);
        o->device_desc.bMaxPacketSize = 64;
        o->device_desc.idVendor = UINT16_C(0x0483);
        o->device_desc.idProduct = UINT16_C(0x5710);
        o->device_desc.bNumConfigurations = 1;
        
        Context::EventLoop::template initFastEvent<FastEvent>(c, Stm32f4Usb::event_handler);
        
        RCC_AHB2PeriphClockCmd(RCC_AHB2Periph_OTG_FS, ENABLE);
        
        //reset_otg(c);
        
        USB_OTG_GINTSTS_TypeDef intsts;
        USB_OTG_GUSBCFG_TypeDef usbcfg;
        USB_OTG_GCCFG_TypeDef gccfg;
        USB_OTG_GAHBCFG_TypeDef ahbcfg;
        USB_OTG_GINTMSK_TypeDef intmsk;
        USB_OTG_DCFG_TypeDef dcfg;
        
        // OTG init
        
        ahbcfg.d32 = 0;
        ahbcfg.b.glblintrmsk = 1;
        ahbcfg.b.nptxfemplvl_txfemplvl = 0;
        core()->GAHBCFG = ahbcfg.d32;
        
        usbcfg.d32 = 0;
        usbcfg.b.physel = 1;
        usbcfg.b.force_dev = 1;
        usbcfg.b.toutcal = Info::Toutcal;
        usbcfg.b.usbtrdtim = Info::Trdtim;
        usbcfg.b.srpcap = 1;
        usbcfg.b.hnpcap = 1;
        core()->GUSBCFG = usbcfg.d32;
        
        intmsk.d32 = 0;
        intmsk.b.otgintr = 1;
        core()->GINTMSK = intmsk.d32;
        
        // device init
        
        dcfg.d32 = device()->DCFG;
        dcfg.b.devspd = 3;
        dcfg.b.nzstsouthshk = 1;
        device()->DCFG = dcfg.d32;
        
        intmsk.d32 = core()->GINTMSK;
        intmsk.b.usbreset = 1;
        intmsk.b.enumdone = 1;
        intmsk.b.sofintr = 1;
        intmsk.b.inepintr = 1;
        intmsk.b.outepintr = 1;
        intmsk.b.rxstsqlvl = 1;
        core()->GINTMSK = intmsk.d32;
        
        USB_OTG_DAINT_TypeDef daintmsk;
        daintmsk.d32 = 0;
        daintmsk.ep.out = (1 << 0);
        daintmsk.ep.in = (1 << 0);
        device()->DAINTMSK = daintmsk.d32;
        
        USB_OTG_DOEPMSK_TypeDef doepmsk;
        doepmsk.d32 = device()->DOEPMSK;
        doepmsk.b.setup = 1;
        device()->DOEPMSK = doepmsk.d32;
        
        gccfg.d32 = 0;
        gccfg.b.vbussensingB = 1;
        gccfg.b.pwdn = 1;
        core()->GCCFG = gccfg.d32;
        
        o->state = STATE_WAITING_RESET;
        o->tx_ptr = NULL;
        
        NVIC_ClearPendingIRQ(Info::Irq);
        NVIC_SetPriority(Info::Irq, INTERRUPT_PRIORITY);
        NVIC_EnableIRQ(Info::Irq);
        
        o->debugInit(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        o->debugDeinit(c);
        
        NVIC_DisableIRQ(Info::Irq);
        
        device()->DCFG = 0;
        core()->GCCFG = 0;
        core()->GINTMSK = 0;
        core()->GUSBCFG = 0;
        core()->GAHBCFG = 0;
        
        NVIC_ClearPendingIRQ(Info::Irq);
        
        RCC_AHB2PeriphClockCmd(RCC_AHB2Periph_OTG_FS, DISABLE);
        
        Context::EventLoop::template resetFastEvent<FastEvent>(c);
    }
    
    static State getState (Context c)
    {
        auto *o = Object::self(c);
        
        State state;
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            state = o->state;
        }
        return state;
    }
    
    static void usb_irq (InterruptContext<Context> c)
    {
        auto *o = Object::self(c);
        
        USB_OTG_GINTSTS_TypeDef intsts;
        intsts.d32 = core()->GINTSTS;
        
        USB_OTG_GINTSTS_TypeDef intsts_clear;
        intsts_clear.d32 = 0;
        
        if (intsts.b.otgintr) {
            USB_OTG_GOTGINT_TypeDef otgint;
            otgint.d32 = core()->GOTGINT;
            core()->GOTGINT = otgint.d32;
            if (otgint.b.sesenddet) {
                o->state = STATE_WAITING_RESET;
            }
        }
        
        if (intsts.b.usbreset) {
            intsts_clear.b.usbreset = 1;
            o->state = STATE_WAITING_ENUM;
        }
        
        if (intsts.b.enumdone) {
            intsts_clear.b.enumdone = 1;
            
            if (o->state == STATE_WAITING_ENUM) {
                o->state = STATE_ENUM_DONE;
                
                USB_OTG_DSTS_TypeDef dsts;
                dsts.d32 = device()->DSTS;
                
                USB_OTG_DEPCTL_TypeDef diepctl;
                diepctl.d32 = inep()[0].DIEPCTL;
                diepctl.b.mps = DEP0CTL_MPS_64;
                inep()[0].DIEPCTL = diepctl.d32;
            }
        }
        
        if (intsts.b.rxstsqlvl) {
            USB_OTG_DRXSTS_TypeDef rxsts;
            rxsts.d32 = core()->GRXSTSP;
            
            switch (rxsts.b.pktsts) {
                case STS_SETUP_UPDT: {
                    read_rx_fifo(0, (uint8_t *)&o->setup_packet, sizeof(o->setup_packet));
                } break;
                
                case STS_DATA_UPDT: {
                    uint16_t size = min((uint16_t)rxsts.b.bcnt, (uint16_t)sizeof(o->data));
                    read_rx_fifo(0, o->data, size);
                } break;
            }
        }
        
        if (intsts.b.sofintr) {
            intsts_clear.b.sofintr = 1;
        }
        
        if (intsts.b.inepint) {
            USB_OTG_DAINT_TypeDef daint;
            daint.d32 = device()->DAINT;
            
            if (daint.ep.in & (1 << 0)) {
                USB_OTG_DIEPINTn_TypeDef diepint;
                diepint.d32 = inep()[0].DIEPINT;
                
                if (diepint.b.emptyintr && o->tx_ptr) {
                    uint16_t packet_len = min((uint16_t)64, o->tx_len);
                    write_tx_fifo(0, o->tx_ptr, packet_len);
                    o->tx_ptr += packet_len;
                    o->tx_len -= packet_len;
                    
                    if (o->tx_len == 0) {
                        device()->DIEPEMPMSK &= ~((uint32_t)1 << 0);
                        o->tx_ptr = NULL;
                    }
                }
            }
        }
        
        if (intsts.b.outepintr) {
            USB_OTG_DAINT_TypeDef daint;
            daint.d32 = device()->DAINT;
            
            if (daint.ep.out & (1 << 0)) {
                USB_OTG_DOEPINTn_TypeDef doepint;
                doepint.d32 = outep()[0].DOEPINT;
                
                if (doepint.b.setup) {
                    USB_OTG_DOEPINTn_TypeDef doepint_clear;
                    doepint_clear.d32 = 0;
                    doepint_clear.b.setup = 1;
                    outep()[0].DOEPINT = doepint_clear.d32;
                    
                    if (o->state >= STATE_ENUM_DONE) {
                        handle_setup(c);
                    }
                }
            }
        }
        
        core()->GINTSTS = intsts_clear.d32;
        
        Context::EventLoop::template triggerFastEvent<FastEvent>(c);
    }
    
    using EventLoopFastEvents = MakeTypeList<FastEvent>;
    
private:
    static USB_OTG_GREGS * core ()
    {
        return (USB_OTG_GREGS *)(Info::OtgAddress + USB_OTG_CORE_GLOBAL_REGS_OFFSET);
    }
    
    static USB_OTG_DREGS * device ()
    {
        return (USB_OTG_DREGS *)(Info::OtgAddress + USB_OTG_DEV_GLOBAL_REG_OFFSET);
    }
    
    static uint32_t volatile * pcgcctl ()
    {
        return (uint32_t volatile *)(Info::OtgAddress + USB_OTG_PCGCCTL_OFFSET);
    }
    
    static USB_OTG_INEPREGS * inep ()
    {
        return (USB_OTG_INEPREGS *)(Info::OtgAddress + USB_OTG_DEV_IN_EP_REG_OFFSET);
    }
    
    static USB_OTG_OUTEPREGS * outep ()
    {
        return (USB_OTG_OUTEPREGS *)(Info::OtgAddress + USB_OTG_DEV_OUT_EP_REG_OFFSET);
    }
    
    static uint32_t volatile * dfifo (uint8_t ep_index)
    {
        return (uint32_t volatile *)(Info::OtgAddress + USB_OTG_DATA_FIFO_OFFSET + ep_index * USB_OTG_DATA_FIFO_SIZE);
    }
    
    static void read_rx_fifo (uint8_t ep_index, uint8_t *data, uint16_t len)
    {
        uint32_t volatile *fifo = dfifo(ep_index);
        
        while (len >= 4) {
            uint32_t word = *fifo;
            memcpy(data, &word, 4);
            data += 4;
            len -= 4;
        }
        
        if (len > 0) {
            uint32_t word = *fifo;
            memcpy(data, &word, len);
        }
    }
    
    static void write_tx_fifo (uint8_t ep_index, uint8_t const *data, uint16_t len)
    {
        uint32_t volatile *fifo = dfifo(ep_index);
        
        while (len >= 4) {
            uint32_t word;
            memcpy(&word, data, 4);
            *fifo = word;
            data += 4;
            len -= 4;
        }
        
        if (len > 0) {
            uint32_t word = 0;
            memcpy(&word, data, len);
            *fifo = word;
        }
    }
    
    static void event_handler (Context c)
    {
        auto *o = Object::self(c);
        o->debugAccess(c);
        
        
    }
    
    static void reset_otg (Context c)
    {
        USB_OTG_GRSTCTL_TypeDef grstctl;
        
        do {
            grstctl.d32 = core()->GRSTCTL;
        } while (!grstctl.b.ahbidle);
        
        grstctl.b.csftrst = 1;
        core()->GRSTCTL = grstctl.d32;
        
        do {
            grstctl.d32 = core()->GRSTCTL;
        } while (grstctl.b.csftrst);
    }
    
    template <typename ThisContext>
    static void handle_setup (ThisContext c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->state >= STATE_ENUM_DONE)
        
        UsbSetupPacket *s = &o->setup_packet;
        
        if (s->bmRequestType == USB_REQUEST_TYPE_D2H_STD_DEV && s->bRequest == USB_REQUEST_ID_GET_DESCRIPTOR) {
            uint8_t desc_type = s->wValue >> 8;
            uint8_t desc_index = s->wValue;
            if (desc_type == USB_DESSCRIPTOR_TYPE_DEVICE && !o->tx_ptr) {
                start_tx(c, (uint8_t *)&o->device_desc, sizeof(o->device_desc));
                if (s->wLength >= 65) {
                    o->state = STATE_TEST;
                }
            }
        }
        else if (s->bmRequestType == USB_REQUEST_TYPE_H2D_STD_DEV && s->bRequest == USB_REQUEST_ID_SET_ADDRESS) {
            
        }
    }
    
    template <typename ThisContext>
    static void start_tx (ThisContext c, uint8_t const *ptr, uint16_t len)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(!o->tx_ptr)
        
        o->tx_ptr = ptr;
        o->tx_len = len;
        
        USB_OTG_DEPXFRSIZ_TypeDef dieptsiz;
        dieptsiz.d32 = 0;
        dieptsiz.b.xfersize = len;
        dieptsiz.b.pktcnt = (len + 63) / 64;
        inep()[0].DIEPTSIZ = dieptsiz.d32;
        
        USB_OTG_DEPCTL_TypeDef diepctl;
        diepctl.d32 = inep()[0].DIEPCTL;
        diepctl.b.cnak = 1;
        diepctl.b.epena = 1;
        inep()[0].DIEPCTL = diepctl.d32;
        
        device()->DIEPEMPMSK |= ((uint32_t)1 << 0);
    }
    
public:
    struct Object : public ObjBase<Stm32f4Usb, ParentObject, EmptyTypeList>,
        public DebugObject<Context, void>
    {
        State state;
        UsbSetupPacket setup_packet;
        uint8_t data[1024];
        UsbDeviceDescriptor device_desc;
        uint8_t const *tx_ptr;
        uint16_t tx_len;
    };
};

#define AMBRO_STM32F4_USB_GLOBAL(the_usb, context) \
extern "C" \
__attribute__((used)) \
void OTG_FS_IRQHandler (void) \
{ \
    the_usb::usb_irq(MakeInterruptContext((context))); \
}

#include <aprinter/EndNamespace.h>

#endif
