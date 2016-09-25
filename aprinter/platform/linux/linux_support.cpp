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

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include <alloca.h>
#include <pthread.h>
#include <signal.h>
#include <sched.h>
#include <getopt.h>
#include <sys/mman.h>

#include "linux_support.h"

#include <aprinter/base/Assert.h>

LinuxCmdlineOptions cmdline_options;
pthread_mutex_t interrupt_mutex;

static bool parse_options (int argc, char *argv[]);
static void make_cpuset (int affinity, cpu_set_t *cpuset);

static size_t const MainStackPrefaultSize = 16384;
static size_t const RtThreadStackSize = PTHREAD_STACK_MIN;

template <size_t Size>
__attribute__((noinline))
static void prefault_stack ()
{
    void *ptr = alloca(Size);
    auto const volatile memset_ptr = memset;
    memset_ptr(ptr, 0, Size);
}

void platform_init (int argc, char *argv[])
{
    int res;
    
    // Parse command-line options.
    if (!parse_options(argc, argv)) {
        exit(1);
    }
    
    // Lock memory if requested.
    if (cmdline_options.lock_mem) {
        res = mlockall(MCL_CURRENT|MCL_FUTURE);
        AMBRO_ASSERT_FORCE_MSG(res == 0, "mlockall failed")
    }
    
    // Set main CPU affinity if requested.
    if (cmdline_options.main_affinity != 0) {
        cpu_set_t cpuset;
        make_cpuset(cmdline_options.main_affinity, &cpuset);
        
        res = pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
        AMBRO_ASSERT_FORCE_MSG(res == 0, "pthread_setaffinity_np failed")
    }
    
    // Pre-fault the stack of main.
    prefault_stack<MainStackPrefaultSize>();
    
    // Initialize the interrupt_mutex.
    pthread_mutexattr_t attr;
    res = pthread_mutexattr_init(&attr);
    AMBRO_ASSERT_FORCE(res == 0)
    res = pthread_mutexattr_setprotocol(&attr, PTHREAD_PRIO_INHERIT);
    AMBRO_ASSERT_FORCE(res == 0)
    res = pthread_mutex_init(&interrupt_mutex, &attr);
    AMBRO_ASSERT_FORCE(res == 0)
    res = pthread_mutexattr_destroy(&attr);
    AMBRO_ASSERT_FORCE(res == 0)
    
    // Configure SIGPIPE to SIG_IGN.
    struct sigaction act = {};
    act.sa_handler = SIG_IGN;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    res = sigaction(SIGPIPE, &act, NULL);
    AMBRO_ASSERT_FORCE(res == 0)
}

static bool parse_options (int argc, char *argv[])
{
    cmdline_options.lock_mem = false;
    cmdline_options.rt_class = -1;
    cmdline_options.rt_priority = -1;
    cmdline_options.rt_affinity = 0;
    cmdline_options.main_affinity = 0;
    
    static struct option const long_options[] = {
        {"lock-mem",      no_argument,       nullptr, 'l'},
        {"rt-class",      required_argument, nullptr, 'c'},
        {"rt-priority",   required_argument, nullptr, 'p'},
        {"rt-affinity",   required_argument, nullptr, 'a'},
        {"main-affinity", required_argument, nullptr, 'f'},
        {}
    };
    
    while (true) {
        int option_index = 0;
        int opt = getopt_long(argc, argv, "lc:p:a:f:", long_options, &option_index);
        if (opt == -1) {
            break;
        }
        
        switch (opt) {
            case 'l': {
                cmdline_options.lock_mem = true;
            } break;
            
            case 'c': {
                if (!strcmp(optarg, "FIFO")) {
                    cmdline_options.rt_class = SCHED_FIFO;
                }
                else if (!strcmp(optarg, "RR")) {
                    cmdline_options.rt_class = SCHED_RR;
                }
                else if (!strcmp(optarg, "OTHER")) {
                    cmdline_options.rt_class = SCHED_OTHER;
                }
                else {
                    fprintf(stderr, "Invalid RT class\n");
                    return false;
                }
            } break;
            
            case 'p': {
                int val = atoi(optarg);
                if (val < 0) {
                    fprintf(stderr, "Invalid RT priority\n");
                    return false;
                }
                cmdline_options.rt_priority = val;
            } break;
            
            case 'a': {
                int val = atoi(optarg);
                if (val == 0) {
                    fprintf(stderr, "Invalid RT affinity\n");
                    return false;
                }
                cmdline_options.rt_affinity = val;
            } break;
            
            case 'f': {
                int val = atoi(optarg);
                if (val == 0) {
                    fprintf(stderr, "Invalid main affinity\n");
                    return false;
                }
                cmdline_options.main_affinity = val;
            } break;
            
            default: {
                return false;
            } break;
        }
    }
    
    if (cmdline_options.rt_class >= 0 && cmdline_options.rt_priority < 0) {
        fprintf(stderr, "Error: RT class specified without RT priority\n");
        return false;
    }
    
    return true;
}

void LinuxRtThread::start (int rt_class, int rt_priority, int rt_affinity, void * (*start_func) (void *))
{
    int res;
    pthread_attr_t attr;
    
    m_stack = mmap(nullptr, RtThreadStackSize, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    AMBRO_ASSERT_FORCE_MSG(m_stack != MAP_FAILED, "mmap failed")
    
    auto const volatile memset_ptr = memset;
    memset_ptr(m_stack, 0, RtThreadStackSize);
    
    res = pthread_attr_init(&attr);
    AMBRO_ASSERT_FORCE(res == 0)
    
    res = pthread_attr_setstack(&attr, m_stack, RtThreadStackSize);
    AMBRO_ASSERT_FORCE_MSG(res == 0, "pthread_attr_setstack failed")
    
    if (rt_class >= 0) {
        res = pthread_attr_setschedpolicy(&attr, rt_class);
        AMBRO_ASSERT_FORCE_MSG(res == 0, "pthread_attr_setschedpolicy failed")
        
        struct sched_param param = {};
        param.sched_priority = rt_priority;
        res = pthread_attr_setschedparam(&attr, &param);
        AMBRO_ASSERT_FORCE_MSG(res == 0, "pthread_attr_setschedparam failed")
        
        pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
        AMBRO_ASSERT_FORCE_MSG(res == 0, "pthread_attr_setinheritsched failed")
    }
    
    if (rt_affinity != 0) {
        cpu_set_t cpuset;
        make_cpuset(rt_affinity, &cpuset);
        
        pthread_attr_setaffinity_np(&attr, sizeof(cpuset), &cpuset);
        AMBRO_ASSERT_FORCE_MSG(res == 0, "pthread_attr_setaffinity_np failed")
    }
    
    res = pthread_create(&m_thread, &attr, start_func, nullptr);
    AMBRO_ASSERT_FORCE_MSG(res == 0, "pthread_create failed")
    
    res = pthread_attr_destroy(&attr);
    AMBRO_ASSERT_FORCE(res == 0)
}

void LinuxRtThread::join ()
{
    int res;
    
    res = pthread_join(m_thread, nullptr);
    AMBRO_ASSERT_FORCE(res == 0)
    
    res = munmap(m_stack, RtThreadStackSize);
    AMBRO_ASSERT_FORCE(res == 0)
}

static void make_cpuset (int affinity, cpu_set_t *cpuset)
{
    CPU_ZERO(cpuset);
    if (affinity < 0) {
        int cpu = 0;
        while (affinity < 0) {
            if ((affinity & 1) != 0) {
                CPU_SET(cpu, cpuset);
            }
            cpu++;
            affinity /= 2;
        }
    }
    else if (affinity > 0 && affinity <= CPU_SETSIZE) {
        CPU_SET((affinity - 1), cpuset);
    }
}
