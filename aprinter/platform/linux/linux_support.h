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

#ifndef APRINTER_LINUX_SUPPORT_H
#define APRINTER_LINUX_SUPPORT_H

#include <stdlib.h>
#include <pthread.h>

#include <aprinter/base/Callback.h>
#include <aprinter/system/InterruptLockCommon.h>

// This is a stub F_CPU value of 1Hz so that the "max steps per cycle"
// setting effectively configures "max steps per second".
#define F_CPU (1.0)

#define APRINTER_INTERRUPT_LOCK_MODE APRINTER_INTERRUPT_LOCK_MODE_SIMPLE

#define AMBROLIB_EMERGENCY_ACTION
#define AMBROLIB_ABORT_ACTION { ::abort(); }

#define APRINTER_DONT_DEFINE_CXA_PURE_VIRTUAL
#define APRINTER_MAIN_WITH_ARGS

struct LinuxCmdlineOptions {
    bool lock_mem;
    int rt_class;
    int rt_priority;
    int rt_affinity;
    int main_affinity;
};

extern LinuxCmdlineOptions cmdline_options;

extern pthread_mutex_t interrupt_mutex;

void platform_init (int argc, char *argv[]);

inline static void cli (void)
{
    int res = pthread_mutex_lock(&interrupt_mutex);
    if (res != 0) {
        abort();
    }
}

inline static void sei (void)
{
    int res = pthread_mutex_unlock(&interrupt_mutex);
    if (res != 0) {
        abort();
    }
}

class LinuxRtThread {
public:
    using FuncType = APrinter::Callback<void()>;
    
    void start (FuncType start_func);
    void start (FuncType start_func, int rt_class, int rt_priority, int rt_affinity);
    void join ();
    
private:
    static void * thread_trampoline (void *arg);
    
    FuncType m_start_func;
    void *m_stack;
    pthread_t m_thread;
};

#endif
