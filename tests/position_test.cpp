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

#include <stdio.h>

#include <aprinter/meta/Position.h>

using namespace APrinter;

template <typename Position, typename ABPosition>
struct A {
    void callAB ()
    {
        // I can call AB from here!
        auto *ab = PositionTraverse<Position, ABPosition>::call(this);
        ab->speak();
    }
    
    void speak ()
    {
        printf("A speaking\n");
    }
};

template <typename Position, typename APosition>
struct B {
    void callA ()
    {
        // I can call A from here!
        auto *a = PositionTraverse<Position, APosition>::call(this);
        a->speak();
    }
    
    void speak ()
    {
        printf("B speaking\n");
    }
};

template <typename Position>
struct AB {
    // boilerplate
    struct APosition;
    struct BPosition;
    
    using TheA = A<APosition, Position>;
    using TheB = B<BPosition, APosition>;
    
    TheA m_a;
    TheB m_b;
    
    // boilerplate
    struct APosition : public MemberPosition<Position, TheA, &AB::m_a> {};
    struct BPosition : public MemberPosition<Position, TheB, &AB::m_b> {};
    
    void speak ()
    {
        printf("AB speaking\n");
    }
};

// boilerplate
struct TheABPosition;
using TheAB = AB<TheABPosition>;
TheAB ab;

// boilerplate
struct TheABPosition : public RootPosition<TheAB> {};

int main ()
{
    // Make B call A (its sibling).
    ab.m_b.callA();
    
    // Make A call AB (its parent).
    ab.m_a.callAB();
}
