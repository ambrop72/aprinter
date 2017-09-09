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

#ifndef AIPSTACK_PREPROCESSOR_H
#define AIPSTACK_PREPROCESSOR_H

#define AIPSTACK_JOIN_HELPER(x, y) x##y
#define AIPSTACK_JOIN(x, y) AIPSTACK_JOIN_HELPER(x, y)

#define AIPSTACK_STRINGIFY_HELPER(x) #x
#define AIPSTACK_STRINGIFY(x) AIPSTACK_STRINGIFY_HELPER(x)

#define AIPSTACK_REMOVE_PARENS(...) __VA_ARGS__

#define AIPSTACK_ARRAY_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))

#include "Preprocessor_MacroMap.h"

#define AIPSTACK_USE_TYPE1(namespace, type_name) using type_name = typename namespace::type_name;
#define AIPSTACK_USE_TYPE2(namespace, type_name) using type_name = namespace::type_name;
#define AIPSTACK_USE_VAL(namespace, value_name) static constexpr auto value_name = namespace::value_name;

#define AIPSTACK_USE_TYPES1(namespace, type_names) AIPSTACK_AS_MAP(AIPSTACK_USE_TYPE1, AIPSTACK_AS_MAP_DELIMITER_NONE, namespace, type_names)
#define AIPSTACK_USE_TYPES2(namespace, type_names) AIPSTACK_AS_MAP(AIPSTACK_USE_TYPE2, AIPSTACK_AS_MAP_DELIMITER_NONE, namespace, type_names)
#define AIPSTACK_USE_VALS(namespace,   type_names) AIPSTACK_AS_MAP(AIPSTACK_USE_VAL,   AIPSTACK_AS_MAP_DELIMITER_NONE, namespace, type_names)

#endif
