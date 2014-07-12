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

#ifndef AMBROLIB_FLASH_CONFIG_STORE_H
#define AMBROLIB_FLASH_CONFIG_STORE_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include <aprinter/meta/Object.h>
#include <aprinter/meta/WrapFunction.h>
#include <aprinter/meta/TypeListGet.h>
#include <aprinter/meta/IndexElemList.h>
#include <aprinter/meta/ListForEach.h>
#include <aprinter/meta/WrapValue.h>
#include <aprinter/meta/TypeListLength.h>
#include <aprinter/base/Assert.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, typename ConfigManager, typename Handler, typename Params>
class FlashConfigStore {
public:
    struct Object;
    
private:
    AMBRO_DECLARE_LIST_FOREACH_HELPER(Foreach_read, read)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(Foreach_write, write)
    
    struct FlashHandler;
    using Loop = typename Context::EventLoop;
    using TheFlash = typename Params::FlashService::template Flash<Context, Object, FlashHandler>;
    using OptionSpecList = typename ConfigManager::RuntimeConfigOptionsList;
    
    static_assert(Params::StartBlock < Params::EndBlock, "");
    static_assert(Params::EndBlock <= TheFlash::NumBlocks, "");
    
    static uint32_t const HeaderMagic = UINT32_C(0xAE83108C);
    
    struct Header {
        uint32_t magic;
    };
    
    template <int OptionIndex, typename Dummy = void>
    struct OptionHelper {
        using Option = TypeListGet<OptionSpecList, OptionIndex>;
        using Type = typename Option::Type;
        using PrevOption = OptionHelper<(OptionIndex - 1)>;
        static_assert(sizeof(Type) <= TheFlash::BlockSize, "");
        
        static bool const UseNextBlock = PrevOption::BlockEndOffset + sizeof(Type) > TheFlash::BlockSize;
        static size_t const BlockNumber = PrevOption::BlockNumber + UseNextBlock;
        static size_t const BlockStartOffset = UseNextBlock ? 0 : PrevOption::BlockEndOffset;
        static size_t const BlockEndOffset = BlockStartOffset + sizeof(Type);
        static size_t const FlashOffset = BlockNumber * TheFlash::BlockSize + BlockStartOffset;
        
        static_assert(BlockNumber < Params::EndBlock, "");
        static_assert(BlockEndOffset <= TheFlash::BlockSize, "");
        
        static void read (Context c)
        {
            Type value;
            read_flash(FlashOffset, sizeof(value), &value);
            ConfigManager::setOptionValue(c, Option(), value);
        }
        
        template <int WriteBlockNumber>
        static void write (Context c, WrapInt<WriteBlockNumber>)
        {
            auto *o = Object::self(c);
            if (BlockNumber == WriteBlockNumber) {
                Type value = ConfigManager::getOptionValue(c, Option());
                memcpy((uint8_t *)o->write_buffer + BlockStartOffset, &value, sizeof(Type));
            }
        }
    };
    
    template <typename Dummy>
    struct OptionHelper<(-1), Dummy> {
        static size_t const BlockNumber = Params::StartBlock + 1;
        static size_t const BlockEndOffset = 0;
    };
    
    template <int OptionIndex>
    using OptionHelperOneArg = OptionHelper<OptionIndex>;
    using OptionHelperList = IndexElemList<OptionSpecList, OptionHelperOneArg>;
    using LastOption = OptionHelper<(TypeListLength<OptionHelperList>::Value - 1)>;
    static size_t const NumOptionBlocks = LastOption::BlockNumber - OptionHelper<(-1)>::BlockNumber + 1;
    
    template <int WriteBlockNumber, typename Dummy = void>
    struct BlockWriteHelper {
        static_assert(WriteBlockNumber >= 1, "");
        static_assert(WriteBlockNumber < 1 + NumOptionBlocks, "");
        static int const BlockNumber = Params::StartBlock + WriteBlockNumber;
        
        static size_t write (Context c)
        {
            ListForEachForward<OptionHelperList>(Foreach_write(), c, WrapInt<BlockNumber>());
            return BlockNumber;
        }
    };
    
    template <typename Dummy>
    struct BlockWriteHelper<0, Dummy> {
        static size_t write (Context c)
        {
            return Params::StartBlock;
        }
    };
    
    template <typename Dummy>
    struct BlockWriteHelper<(1 + NumOptionBlocks), Dummy> {
        static size_t write (Context c)
        {
            auto *o = Object::self(c);
            Header header;
            header.magic = HeaderMagic;
            memcpy(o->write_buffer, &header, sizeof(header));
            return Params::StartBlock;
        }
    };
    
    static size_t const NumWriteBlocks = 1 + NumOptionBlocks + 1;
    template <int WriteBlockNumber>
    using BlockWriteHelperOneArg = BlockWriteHelper<WriteBlockNumber>;
    using BlockWriteHelperList = IndexElemListCount<NumWriteBlocks, BlockWriteHelperOneArg>;
    
    enum State {STATE_IDLE, STATE_START_READING, STATE_START_WRITING, STATE_WRITING};
    
public:
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        TheFlash::init(c);
        o->event.init(c, FlashConfigStore::event_handler);
        o->state = STATE_IDLE;
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        
        o->event.deinit(c);
        TheFlash::deinit(c);
    }
    
    static void startReading (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->state == STATE_IDLE)
        
        o->state = STATE_START_READING;
        o->event.prependNowNotAlready(c);
    }
    
    static void startWriting (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->state == STATE_IDLE)
        
        o->write_current_block = -1;
        o->state = STATE_START_WRITING;
        o->event.prependNowNotAlready(c);
    }
    
    using GetFlash = TheFlash;
    
private:
    static void read_flash (size_t offset, size_t size, void *dst)
    {
        for (size_t i = 0; i < size; i++) {
            ((uint8_t *)dst)[i] = TheFlash::getReadPointer()[offset + i];
        }
    }
    
    static void flash_handler (Context c, bool success)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->state == STATE_WRITING)
        
        if (!success) {
            o->state = STATE_IDLE;
            return Handler::call(c, false);
        }
        o->write_current_block++;
        if (o->write_current_block == NumWriteBlocks) {
            o->state = STATE_IDLE;
            return Handler::call(c, true);
        }
        memset(o->write_buffer, 0, sizeof(o->write_buffer));
        size_t block_number = ListForOneOffset<BlockWriteHelperList, 0, size_t>(o->write_current_block, Foreach_write(), c);
        for (size_t i = 0; i < (TheFlash::BlockSize / 4); i++) {
            TheFlash::getBlockWritePointer(c, block_number)[i] = o->write_buffer[i];
        }
        TheFlash::startBlockWrite(c, block_number);
    }
    
    static void event_handler (typename Loop::QueuedEvent *, Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->state == STATE_START_READING || o->state == STATE_START_WRITING)
        
        if (o->state == STATE_START_WRITING) {
            o->state = STATE_WRITING;
            return flash_handler(c, true);
        }
        
        bool success = false;
        Header header;
        read_flash(Params::StartBlock * TheFlash::BlockSize, sizeof(header), &header);
        if (header.magic != HeaderMagic) {
            goto fail;
        }
        ListForEachForward<OptionHelperList>(Foreach_read(), c);
        success = true;
    fail:
        o->state = STATE_IDLE;
        return Handler::call(c, success);
    }
    
    struct FlashHandler : public AMBRO_WFUNC_TD(&FlashConfigStore::flash_handler) {};
    
public:
    struct Object : public ObjBase<FlashConfigStore, ParentObject, MakeTypeList<
        TheFlash
    >> {
        typename Loop::QueuedEvent event;
        State state;
        size_t write_current_block;
        uint32_t write_buffer[TheFlash::BlockSize / 4];
    };
};

template <typename TFlashService, int TStartBlock, int TEndBlock>
struct FlashConfigStoreService {
    using FlashService = TFlashService;
    static int const StartBlock = TStartBlock;
    static int const EndBlock = TEndBlock;
    
    template <typename Context, typename ParentObject, typename ConfigManager, typename Handler>
    using Store = FlashConfigStore<Context, ParentObject, ConfigManager, Handler, FlashConfigStoreService>;
};

#include <aprinter/EndNamespace.h>

#endif
