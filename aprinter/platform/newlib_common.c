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

#include <stddef.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

void *__dso_handle;

__attribute__((weak))
void aprinter_platform_debug_write (char const *ptr, size_t len)
{
}

__attribute__((used))
void _init (void)
{
}

#ifndef APRINTER_NO_SBRK

__attribute__((used))
__attribute__((aligned(sizeof(void *))))
char aprinter_heap[HEAP_SIZE];

char *aprinter_heap_end = aprinter_heap;

__attribute__((used))
void * _sbrk (ptrdiff_t incr)
{
    if (incr > aprinter_heap + HEAP_SIZE - aprinter_heap_end) {
        errno = ENOMEM;
        return (void *)-1;
    }
    char *prev_heap_end = aprinter_heap_end;
    aprinter_heap_end += incr;
    return prev_heap_end;
}

size_t aprinter_get_heap_usage()
{
    return aprinter_heap_end - aprinter_heap;
}

#endif

__attribute__((used))
int _read (int file, void *ptr, size_t len)
{
    return -1;
}

__attribute__((used))
int _write (int file, void const *ptr, size_t len)
{
    aprinter_platform_debug_write(ptr, len);
    return len;
}

__attribute__((used))
int _close (int file)
{
    return -1;
}

__attribute__((used))
int _fstat (int file, struct stat *st)
{
    return -1;
}

__attribute__((used))
int _isatty (int fd)
{
    return 1;
}

__attribute__((used))
_off_t _lseek (int file, _off_t ptr, int dir)
{
    return -1;
}
