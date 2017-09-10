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

#ifndef AIPSTACK_FUNCTION_IF_H
#define AIPSTACK_FUNCTION_IF_H

#include <type_traits>

namespace AIpStack {

#define AIPSTACK_FUNCTION_IF(condition, return_type, remaining_declaration) \
AIPSTACK_FUNCTION_IF_EXT(condition, , return_type, remaining_declaration)

#define AIPSTACK_FUNCTION_IF_OR_EMPTY(condition, return_type, remaining_declaration) \
AIPSTACK_FUNCTION_IF_OR_EMPTY_EXT(condition, , return_type, remaining_declaration)

#define AIPSTACK_FUNCTION_IF_ELSE(condition, return_type, remaining_declaration, if_code, else_code) \
AIPSTACK_FUNCTION_IF_ELSE_EXT(condition, , return_type, remaining_declaration, if_code, else_code)

#define AIPSTACK_FUNCTION_IF_EXT(condition, qualifiers, return_type, remaining_declaration) \
template <typename FunctionIfReturnType=return_type> qualifiers std::template enable_if_t<(condition), FunctionIfReturnType> remaining_declaration

#define AIPSTACK_FUNCTION_IF_OR_EMPTY_EXT(condition, qualifiers, return_type, remaining_declaration) \
template <typename FunctionIfReturnType=return_type> qualifiers std::template enable_if_t<!(condition), FunctionIfReturnType> remaining_declaration {} \
template <typename FunctionIfReturnType=return_type> qualifiers std::template enable_if_t<(condition), FunctionIfReturnType> remaining_declaration

#define AIPSTACK_FUNCTION_IF_ELSE_EXT(condition, qualifiers, return_type, remaining_declaration, if_code, else_code) \
template <typename FunctionIfReturnType=return_type> qualifiers std::template enable_if_t<(condition), FunctionIfReturnType> remaining_declaration if_code \
template <typename FunctionIfReturnType=return_type> qualifiers std::template enable_if_t<!(condition), FunctionIfReturnType> remaining_declaration else_code

}

#endif
