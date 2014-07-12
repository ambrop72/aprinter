/*
 * Copyright (c) 2014 Ambroz Bizjak
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

#ifndef AMBROLIB_AT91SAM3X_FLASH_H
#define AMBROLIB_AT91SAM3X_FLASH_H

#include <stdint.h>
#include <stddef.h>

#include <aprinter/meta/Object.h>
#include <aprinter/meta/MakeTypeList.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Lock.h>
#include <aprinter/system/InterruptLock.h>

#include <aprinter/BeginNamespace.h>

#define APRINTER_AT91SAM3X_FLASH_DEFINE_DEVICE(Index) \
struct At91Sam3xFlashDevice##Index { \
    static int const DeviceIndex = Index; \
    static size_t const Size = IFLASH##Index##_SIZE; \
    static size_t const PageSize = IFLASH##Index##_PAGE_SIZE; \
    static size_t const LockRegionSize = IFLASH##Index##_LOCK_REGION_SIZE; \
    static size_t const NumPages = IFLASH##Index##_NB_OF_PAGES; \
    static uint8_t volatile * dataPtr () { return (uint8_t volatile *)IFLASH##Index##_ADDR; } \
    static IRQn const Irq = EFC##Index##_IRQn; \
    static Efc volatile * efc () { return EFC##Index; } \
};

#ifdef IFLASH0_SIZE
APRINTER_AT91SAM3X_FLASH_DEFINE_DEVICE(0)
#endif
#ifdef IFLASH1_SIZE
APRINTER_AT91SAM3X_FLASH_DEFINE_DEVICE(1)
#endif

template <typename Context, typename ParentObject, typename Handler, typename Params>
class At91Sam3xFlash {
private:
    using FastEvent = typename Context::EventLoop::template FastEventSpec<At91Sam3xFlash>;
    using Device = typename Params::Device;
    
public:
    struct Object;
    
    static int const DeviceIndex = Device::DeviceIndex;
    
    static size_t const BlockSize = Device::PageSize;
    static size_t const NumBlocks = Device::NumPages;
    
    static uint8_t const volatile * getReadPointer () { return Device::dataPtr(); }
    
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        Context::EventLoop::template initFastEvent<FastEvent>(c, At91Sam3xFlash::event_handler);
        o->writing = false;
        
        NVIC_ClearPendingIRQ(Device::Irq);
        NVIC_SetPriority(Device::Irq, INTERRUPT_PRIORITY);
        NVIC_EnableIRQ(Device::Irq);
        
        o->debugInit(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        o->debugDeinit(c);
        
        NVIC_DisableIRQ(Device::Irq);
        Device::efc()->EEFC_FMR &= ~EEFC_FMR_FRDY;
        NVIC_ClearPendingIRQ(Device::Irq);
        
        Context::EventLoop::template resetFastEvent<FastEvent>(c);
    }
    
    static uint32_t volatile * getBlockWritePointer (Context c, size_t block_index)
    {
        auto *o = Object::self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(block_index < NumBlocks)
        
        return (uint32_t volatile *)Device::dataPtr();
    }
    
    static void startBlockWrite (Context c, size_t block_index)
    {
        auto *o = Object::self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(block_index < NumBlocks)
        AMBRO_ASSERT(!o->writing)
        
        AMBRO_LOCK_T(InterruptTempLock(), c, lock_c) {
            o->writing = true;
        }
        Device::efc()->EEFC_FCR = EEFC_FCR_FCMD(0x03) | EEFC_FCR_FARG(block_index) | EEFC_FCR_FKEY(0x5A);
        Device::efc()->EEFC_FMR |= EEFC_FMR_FRDY;
    }
    
    static void efc_irq (InterruptContext<Context> c)
    {
        auto *o = Object::self(c);
        
        uint32_t fsr = Device::efc()->EEFC_FSR;
        if (fsr & EEFC_FSR_FRDY) {
            AMBRO_ASSERT(o->writing)
            Device::efc()->EEFC_FMR &= ~EEFC_FMR_FRDY;
            o->success = !(fsr & EEFC_FSR_FCMDE) && !(fsr & EEFC_FSR_FLOCKE);
            Context::EventLoop::template triggerFastEvent<FastEvent>(c);
        }
    }
    
    using EventLoopFastEvents = MakeTypeList<FastEvent>;
    
private:
    static void event_handler (Context c)
    {
        auto *o = Object::self(c);
        o->debugAccess(c);
        AMBRO_ASSERT(o->writing)
        
        o->writing = false;
        return Handler::call(c, o->success);
    }
    
public:
    struct Object : public ObjBase<At91Sam3xFlash, ParentObject, EmptyTypeList>,
        public DebugObject<Context, void>
    {
        bool writing;
        bool success;
    };
};

template <typename TDevice>
struct At91Sam3xFlashService {
    using Device = TDevice;
    
    template <typename Context, typename ParentObject, typename Handler>
    using Flash = At91Sam3xFlash<Context, ParentObject, Handler, At91Sam3xFlashService>;
};

#define AMBRO_AT91SAM3X_FLASH_GLOBAL(TheDeviceIndex, TheFlash, context) \
extern "C" \
__attribute__((used)) \
void EFC##TheDeviceIndex##_Handler (void) \
{ \
    static_assert(TheDeviceIndex == TheFlash::DeviceIndex, "Invalid flash device index."); \
    TheFlash::efc_irq(MakeInterruptContext((context))); \
}

#include <aprinter/EndNamespace.h>

#endif
