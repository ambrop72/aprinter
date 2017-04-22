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

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <vector>

#define APRINTER_LINKED_HEAP_VERIFY 1

#include <aprinter/base/Assert.h>
#include <aprinter/base/Accessor.h>
#include <aprinter/structure/LinkModel.h>
#include <aprinter/structure/TreeCompare.h>
#include <aprinter/structure/LinkedHeap.h>
//#include <aprinter/structure/LinkedHeap_v1.h>
//#include <aprinter/structure/LinkedHeap_v2.h>

using namespace APrinter;

struct Entry;

using LinkModel = PointerLinkModel<Entry>;

struct Entry {
    LinkedHeapNode<LinkModel> node;
    int value;
};

struct KeyFuncs {
    static int GetKeyOfEntry (Entry const &e)
    {
        return e.value;
    }
};

using Compare = TreeCompare<LinkModel, KeyFuncs>;

using Heap = LinkedHeap<APRINTER_MEMBER_ACCESSOR(&Entry::node), Compare, LinkModel>;

static size_t const NumEntries = 10000;

int main ()
{
    srand(time(nullptr));
    printf("%d\n", (int)sizeof(Entry));
    
    std::vector<Entry> entries;
    entries.resize(NumEntries);
    
    Heap heap;
    heap.init();
    
    while (true) {
#if 1
        printf("Inserting\n");
        for (size_t i = 0; i < NumEntries; i++) {
            Entry &e = entries[i];
            e.value = rand();
            //e.value = NumEntries-i;
            heap.insert(e);
        }
        printf("The minimum is %d\n", (*heap.first()).value);
#endif
        
        heap.verifyHeap();
        
#if 1
        printf("Fixuping\n");
        for (size_t i = 0; i < NumEntries; i++) {
            Entry &e = entries[i];
            e.value = rand();
            heap.fixup(e);
        }
        printf("The minimum is %d\n", (*heap.first()).value);
#endif
    
        heap.verifyHeap();
        
#if 1
        printf("Removing\n");
        for (size_t i = 0; i < NumEntries; i++) {
            Entry &e = entries[i];
            heap.remove(e);
        }
        AMBRO_ASSERT_FORCE(heap.first().isNull())
        heap.verifyHeap();
#else
        heap.init();
#endif
    }
    
    return 0;
}
