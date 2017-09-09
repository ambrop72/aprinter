/*
 * Copyright (c) 2017 Ambroz Bizjak
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

#ifndef AIPSTACK_STRUCTURE_RAII_WRAPPER_H
#define AIPSTACK_STRUCTURE_RAII_WRAPPER_H

#include <type_traits>

#include <aipstack/misc/Assert.h>

#include <aipstack/misc/NonCopyable.h>

namespace AIpStack {

enum class StructureDestructAction {None, AssertEmpty};

template <
    typename StructureType,
    StructureDestructAction DestructAction = StructureDestructAction::None
>
class StructureRaiiWrapper :
    public StructureType,
    private NonCopyable<StructureRaiiWrapper<StructureType, DestructAction>>
{
    using ActionType = StructureDestructAction;
    
public:
    inline StructureRaiiWrapper ()
    {
        StructureType::init();
    }
    
    inline ~StructureRaiiWrapper ()
    {
        destructAction(std::integral_constant<ActionType, DestructAction>());
    }
    
private:
    inline void destructAction (std::integral_constant<ActionType, ActionType::None>)
    {}
    
    inline void destructAction (std::integral_constant<ActionType, ActionType::AssertEmpty>)
    {
        AIPSTACK_ASSERT(StructureType::isEmpty())
    }
};

}

#endif
