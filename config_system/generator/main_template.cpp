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

/*
 * $${GENERATED_WARNING}
 * 
 * Board for build: $${BoardForBuild}
 */

#include <stdint.h>
#include <stdio.h>
#include <math.h>

$${PLATFORM_INCLUDES}
static void emergency (void);
static void emergency_abort (void);

#define AMBROLIB_EMERGENCY_ACTION { emergency(); }
#define AMBROLIB_ABORT_ACTION { emergency_abort(); }

#include <aprinter/meta/BasicMetaUtils.h>
#include <aprinter/meta/TypeListUtils.h>
#include <aprinter/meta/MemberType.h>
#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/PlacementNew.h>
#include <aprinter/base/Hints.h>

$${AprinterIncludes}
using namespace APrinter;

$${EXTRA_CONSTANTS}
APRINTER_CONFIG_START

$${ConfigOptions}
APRINTER_CONFIG_END

struct Context;
struct Program;

using MyDebugObjectGroup = DebugObjectGroup<Context, Program>;
$${GlobalResourceExprs}
struct Context {
    using DebugGroup = MyDebugObjectGroup;
$${GlobalResourceContextAliases}
    void check () const;
};

$${CodeBeforeProgram}
struct Program : public ObjBase<void, void, MakeTypeList<
    MyDebugObjectGroup,
$${GlobalResourceProgramChildren}
>> {
    static Program * self (Context c);
};

union ProgramMemory {
    ProgramMemory () {}
    ~ProgramMemory () {}
    
    Program program;
} program_memory;

Program * Program::self (Context c) { return &program_memory.program; }
void Context::check () const {}

$${GlobalCode}
static void emergency (void)
{
    // Disable interrupts to guarantee that adjustments made by
    // emergency handling code are complete and no overridden.
#ifndef APRINTER_EMERGENCY_NO_CLI
    cli();
#endif
    // PrinterMain will perform specific emergency actions.
    $${EmergencyProvider}::emergency();
}

APRINTER_NO_RETURN
static void emergency_abort()
{
    $${EmergencyProvider}::emergency_abort();
}

#ifndef APRINTER_DONT_DEFINE_CXA_PURE_VIRTUAL
extern "C" __attribute__((used)) void __cxa_pure_virtual(void)
{
    AMBRO_ASSERT_ABORT("pure virtual function call");
}
#endif

#ifdef APRINTER_MAIN_WITH_ARGS
int main (int argc, char *argv[])
#else
int main ()
#endif
{
$${InitCalls}
    Context c;
    
    new(&program_memory.program) Program();
    
    MyDebugObjectGroup::init(c);
    
$${GlobalResourceInit}
$${FinalInitCalls}
}
