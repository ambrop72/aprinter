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

#ifndef AMBROLIB_EEPROM_CONFIG_STORE_H
#define AMBROLIB_EEPROM_CONFIG_STORE_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include <aprinter/meta/WrapFunction.h>
#include <aprinter/meta/TypeListUtils.h>
#include <aprinter/meta/IndexElemList.h>
#include <aprinter/meta/ListForEach.h>
#include <aprinter/meta/WrapValue.h>
#include <aprinter/meta/DedummyIndexTemplate.h>
#include <aprinter/meta/TypeListReverse.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/Callback.h>
#include <aprinter/base/Assert.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, typename ConfigManager, typename Handler, typename Params>
class EepromConfigStore {
public:
    struct Object;
    
private:
    AMBRO_DECLARE_LIST_FOREACH_HELPER(Foreach_read, read)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(Foreach_write, write)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(Foreach_get_block_number, get_block_number)
    
    struct EepromHandler;
    using Loop = typename Context::EventLoop;
    using TheEeprom = typename Params::EepromService::template Eeprom<Context, Object, EepromHandler>;
    using OptionSpecList = typename ConfigManager::RuntimeConfigOptionsList;
    enum State {STATE_IDLE, STATE_START_READING, STATE_START_WRITING, STATE_READING, STATE_WRITING};
    
    static uint32_t const HeaderMagic = UINT32_C(0xB3CF9267);
    
    struct Header {
        uint32_t magic;
        uint32_t format_hash;
    };
    
    static_assert(Params::StartBlock < Params::EndBlock, "");
    static_assert(Params::EndBlock <= TheEeprom::NumBlocks, "");
    static_assert(sizeof(Header) <= TheEeprom::BlockSize, "");
    
    template <int OptionIndex, typename Dummy=void>
    struct OptionHelper {
        using Option = TypeListGet<OptionSpecList, OptionIndex>;
        using Type = typename Option::Type;
        using PrevOption = OptionHelper<(OptionIndex - 1)>;
        static_assert(sizeof(Type) <= TheEeprom::BlockSize, "");
        
        static bool const UseNextBlock = PrevOption::BlockEndOffset + sizeof(Type) > TheEeprom::BlockSize;
        static int const RelBlockNumber = PrevOption::RelBlockNumber + UseNextBlock;
        static int const BlockNumber = (Params::StartBlock + 1) + RelBlockNumber;
        static int const BlockStartOffset = UseNextBlock ? 0 : PrevOption::BlockEndOffset;
        static int const BlockEndOffset = BlockStartOffset + sizeof(Type);
        
        static_assert(BlockNumber < Params::EndBlock, "");
        static_assert(BlockEndOffset <= TheEeprom::BlockSize, "");
        
        using PrevOptionLists = If<UseNextBlock, ConsTypeList<typename PrevOption::BlockOptionList, typename PrevOption::PrevOptionLists>, typename PrevOption::PrevOptionLists>;
        using BlockOptionList = ConsTypeList<OptionHelper<OptionIndex>, If<UseNextBlock, EmptyTypeList, typename PrevOption::BlockOptionList>>;
        
        static void write (Context c)
        {
            auto *o = Object::self(c);
            Type value = ConfigManager::getOptionValue(c, Option());
            memcpy(o->buffer + BlockStartOffset, &value, sizeof(Type));
        }
        
        static void read (Context c)
        {
            auto *o = Object::self(c);
            Type value;
            memcpy(&value, o->buffer + BlockStartOffset, sizeof(Type));
            ConfigManager::setOptionValue(c, Option(), value);
        }
    };
    
    template <typename Dummy>
    struct OptionHelper<(-1), Dummy> {
        static int const RelBlockNumber = 0;
        static int const BlockEndOffset = 0;
        
        using PrevOptionLists = EmptyTypeList;
        using BlockOptionList = EmptyTypeList;
    };
    
    using OptionHelperList = IndexElemList<OptionSpecList, DedummyIndexTemplate<OptionHelper>::template Result>;
    using LastOption = OptionHelper<(TypeListLength<OptionHelperList>::Value - 1)>;
    static int const NumOptionBlocks = LastOption::RelBlockNumber + 1;
    
    using BlockOptionLists = ConsTypeList<typename LastOption::BlockOptionList, typename LastOption::PrevOptionLists>;
    
    template <int RelBlockNumber>
    using OptionsForBlock = TypeListReverse<TypeListGet<BlockOptionLists, (NumOptionBlocks - 1 - RelBlockNumber)>>;
    
    template <int WriteBlockNumber, typename Dummy=void>
    struct BlockWriteHelper {
        static_assert(WriteBlockNumber >= 1, "");
        static_assert(WriteBlockNumber < 1 + NumOptionBlocks, "");
        
        static int write (Context c)
        {
            ListForEachForward<OptionsForBlock<(WriteBlockNumber - 1)>>(Foreach_write(), c);
            return Params::StartBlock + WriteBlockNumber;
        }
    };
    
    template <typename Dummy>
    struct BlockWriteHelper<0, Dummy> {
        static int write (Context c)
        {
            return Params::StartBlock;
        }
    };
    
    template <typename Dummy>
    struct BlockWriteHelper<(1 + NumOptionBlocks), Dummy> {
        static int write (Context c)
        {
            auto *o = Object::self(c);
            Header header;
            header.magic = HeaderMagic;
            header.format_hash = ConfigManager::FormatHash;
            memcpy(o->buffer, &header, sizeof(Header));
            return Params::StartBlock;
        }
    };
    
    static int const NumWriteBlocks = 1 + NumOptionBlocks + 1;
    using BlockWriteHelperList = IndexElemListCount<NumWriteBlocks, DedummyIndexTemplate<BlockWriteHelper>::template Result>;
    
    template <int ReadBlockNumber, typename Dummy = void>
    struct BlockReadHelper {
        static_assert(ReadBlockNumber >= 1, "");
        static_assert(ReadBlockNumber < 1 + NumOptionBlocks, "");
        
        static int get_block_number () { return Params::StartBlock + ReadBlockNumber; }
        
        static bool read (Context c)
        {
            ListForEachForward<OptionsForBlock<(ReadBlockNumber - 1)>>(Foreach_read(), c);
            return true;
        }
    };
    
    template <typename Dummy>
    struct BlockReadHelper<0, Dummy> {
        static int get_block_number () { return Params::StartBlock; }
        
        static bool read (Context c)
        {
            auto *o = Object::self(c);
            Header header;
            memcpy(&header, o->buffer, sizeof(Header));
            return (header.magic == HeaderMagic && header.format_hash == ConfigManager::FormatHash);
        }
    };
    
    static int const NumReadBlocks = 1 + NumOptionBlocks;
    using BlockReadHelperList = IndexElemListCount<NumReadBlocks, DedummyIndexTemplate<BlockReadHelper>::template Result>;
    
public:
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        TheEeprom::init(c);
        o->event.init(c, APRINTER_CB_STATFUNC_T(&EepromConfigStore::event_handler));
        o->state = STATE_IDLE;
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        
        o->event.deinit(c);
        TheEeprom::deinit(c);
    }
    
    static void startWriting (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->state == STATE_IDLE)
        
        o->state = STATE_START_WRITING;
        o->current_block = 0;
        o->event.prependNowNotAlready(c);
    }
    
    static void startReading (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->state == STATE_IDLE)
        
        o->state = STATE_START_READING;
        o->current_block = 0;
        o->event.prependNowNotAlready(c);
    }
    
    using GetEeprom = TheEeprom;
    
private:
    static void finish (Context c, bool success)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->state != STATE_IDLE)
        
        o->state = STATE_IDLE;
        return Handler::call(c, success);
    }
    
    static void do_write (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->state == STATE_WRITING)
        
        if (o->current_block == NumWriteBlocks) {
            return finish(c, true);
        }
        memset(o->buffer, 0, sizeof(o->buffer));
        int block_number = ListForOneOffset<BlockWriteHelperList, 0, int>(o->current_block, Foreach_write(), c);
        TheEeprom::startWrite(c, block_number * TheEeprom::BlockSize, o->buffer, TheEeprom::BlockSize);
    }
    
    static void do_read (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->state == STATE_READING)
        
        if (o->current_block == NumReadBlocks) {
            return finish(c, true);
        }
        int block_number = ListForOneOffset<BlockReadHelperList, 0, int>(o->current_block, Foreach_get_block_number());
        TheEeprom::startRead(c, block_number * TheEeprom::BlockSize, o->buffer, TheEeprom::BlockSize);
    }
    
    static void eeprom_handler (Context c, bool success)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->state == STATE_WRITING || o->state == STATE_READING)
        
        if (!success) {
            return finish(c, false);
        }
        
        if (o->state == STATE_WRITING) {
            o->current_block++;
            return do_write(c);
        } else {
            if (!ListForOneOffset<BlockReadHelperList, 0, bool>(o->current_block, Foreach_read(), c)) {
                return finish(c, false);
            }
            o->current_block++;
            return do_read(c);
        }
    }
    struct EepromHandler : public AMBRO_WFUNC_TD(&EepromConfigStore::eeprom_handler) {};
    
    static void event_handler (Context c)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->state == STATE_START_WRITING || o->state == STATE_START_READING)
        
        if (o->state == STATE_START_WRITING) {
            o->state = STATE_WRITING;
            return do_write(c);
        } else {
            o->state = STATE_READING;
            return do_read(c);
        }
    }
    
public:
    struct Object : public ObjBase<EepromConfigStore, ParentObject, MakeTypeList<
        TheEeprom
    >> {
        typename Loop::QueuedEvent event;
        State state;
        int current_block;
        uint8_t buffer[TheEeprom::BlockSize];
    };
};

template <typename TEeepromService, int TStartBlock, int TEndBlock>
struct EepromConfigStoreService {
    using EepromService = TEeepromService;
    static int const StartBlock = TStartBlock;
    static int const EndBlock = TEndBlock;
    
    template <typename Context, typename ParentObject, typename ConfigManager, typename ThePrinterMain, typename Handler>
    using Store = EepromConfigStore<Context, ParentObject, ConfigManager, Handler, EepromConfigStoreService>;
};

#include <aprinter/EndNamespace.h>

#endif
