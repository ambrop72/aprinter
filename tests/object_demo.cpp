/*
 * Copyright (c) 2015 Ambroz Bizjak
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

#include <aprinter/base/Object.h>

using namespace APrinter;

/*
 * The Object framework lets you define classes which are aware of their position in
 * the class hierarchy.
 * 
 * The class definition is actually a template, whose ParentObject template parameter
 * allows the class to know its position. If the root is at a constant location, this
 * will result in faster code which does not pass instance pointers to functions -
 * each function knows the address of its instance data. This comes at the cost that
 * all instances need to be known at compile time, and each instance will have its own
 * code generated.
 */

/*
 * Example class with no child classes.
 */

template <typename Context, typename ParentObject>
class Example1 {
public:
    // Object is the struct which contains the instance data. We just declare it here
    // and complete the definition at the bottom of the class.
    struct Object;
    
    static void init (Context c)
    {
        puts("Example1::init");
        
        // get the pointer to the instance data
        auto *o = Object::self(c);
        
        // set some variables
        o->x = 0;
        o->y = 4;
    }
    
public:
    /*
     * Finally define the Object.
     * We need to inherit from ObjBase, which provides the self() function.
     * We give ObjBase the name of this class, the parent object type, and
     * a list of child classes (none here).
     * The ParentObject given to us is most likely the Object type in another
     * class written in this way (or the root Program class, see at the bottom).
     */
    struct Object : public ObjBase<Example1, ParentObject, EmptyTypeList> {
        int x;
        int y;
    };
};

/*
 * Example class with one child class.
 */

template <typename Context, typename ParentObject>
class Example2 {
public:
    struct Object;
    
    // we need to instantiate our child class. Pass out Object as the ParentClass.
    using TheExample1 = Example1<Context, Object>;
    
    static void init (Context c)
    {
        puts("Example2::init");
        auto *o = Object::self(c);
        TheExample1::init(c); // Call a function on our Example1 child class!
        o->z = 0;
    }
    
public:
    struct Object : public ObjBase<Example2, ParentObject, MakeTypeList<
        /*
         * Specify our child classes here. the ObjBase will inherit a tuple
         * of their respective Object classes.
         */
        TheExample1 
    >> {
        int z;
    };
};

/*
 * Example root object (simplified, for full example see the aprinter main files).
 */

struct MyContext {};
struct Program;

// Define instances of children of the root object (Program).
using MyExample2 = Example2<MyContext, Program>;

struct Program : public ObjBase<void, void, MakeTypeList<
    // list children here
    MyExample2
>> {
    static Program * self (MyContext c);
};

Program p;

Program * Program::self (MyContext c) { return &p; }

/*
 * At this point all the Object structs of our classes are contained within
 * the Program. When calling Object::self() on come object, it will chain
 * down, until it gets to our Program and calls our overloaded self() function.
 */

int main ()
{
    puts("main");
    MyContext c;
    // call functions on out classes
    MyExample2::init(c);
}


