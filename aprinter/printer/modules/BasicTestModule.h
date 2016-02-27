/*
 * Copyright (c) 2016 Ambroz Bizjak
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

#ifndef APRINTER_BASIC_TEST_MODULE_H
#define APRINTER_BASIC_TEST_MODULE_H

#include <stdint.h>

#include <aprinter/meta/AliasStruct.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/ProgramMemory.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, typename ThePrinterMain, typename Params>
class BasicTestModule {
public:
    static void init (Context c)
    {
        auto *o = Object::self(c);
        o->underrun_count = 0;
    }
    
    static bool check_command (Context c, typename ThePrinterMain::TheCommand *cmd)
    {
        auto *o = Object::self(c);
        
        switch (cmd->getCmdNumber(c)) {
#ifdef EVENTLOOP_BENCHMARK
            case 916: { // reset benchmark time
                if (!cmd->tryUnplannedCommand(c)) {
                    break;
                }
                Context::EventLoop::resetBenchTime(c);
                cmd->finishCommand(c);
            } break;
            
            case 917: { // print benchmark time
                if (!cmd->tryUnplannedCommand(c)) {
                    break;
                }
                cmd->reply_append_uint32(c, Context::EventLoop::getBenchTime(c));
                cmd->reply_append_ch(c, '\n');
                cmd->finishCommand(c);
            } break;
#endif
            
            case 918: { // test assertions
                uint32_t magic = cmd->get_command_param_uint32(c, 'M', 0);
                if (magic != UINT32_C(122345)) {
                    cmd->reportError(c, AMBRO_PSTR("BadMagic"));
                } else {
                    if (cmd->find_command_param(c, 'F', nullptr)) {
                        AMBRO_ASSERT_FORCE(0)
                    } else {
                        AMBRO_ASSERT(0)
                    }
                }
                cmd->finishCommand(c);
            } break;
            
            case 920: { // get underrun count
                cmd->reply_append_uint32(c, o->underrun_count);
                cmd->reply_append_ch(c, '\n');
                cmd->finishCommand(c);
            } break;
            
            default:
                return true;
        }
        
        return false;
    }
    
    static void planner_underrun (Context c)
    {
        auto *o = Object::self(c);
        
        o->underrun_count++;
        
#ifdef AXISDRIVER_DETECT_OVERLOAD
        if (ThePrinterMain::ThePlanner::axisOverloadOccurred(c)) {
            ThePrinterMain::print_pgm_string(c, AMBRO_PSTR("//AxisOverload\n"));
        } else {
            ThePrinterMain::print_pgm_string(c, AMBRO_PSTR("//NoOverload\n"));
        }
#endif
    }
    
public:
    struct Object : public ObjBase<BasicTestModule, ParentObject, EmptyTypeList> {
        uint32_t underrun_count;
    };
};

struct BasicTestModuleService {
    APRINTER_MODULE_TEMPLATE(BasicTestModuleService, BasicTestModule)
};

#include <aprinter/EndNamespace.h>

#endif
