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

#ifndef APRINTER_ALIAS_STRUCT_H
#define APRINTER_ALIAS_STRUCT_H

#include <aprinter/base/Preprocessor.h>

#define APRINTER_AS_GET_ARG_PAR_NAME(par_type, decl_type, name) Param_##name

#define APRINTER_AS_MAKE_TEMPLATE_PART_VA(...) APRINTER_AS_GET_1(__VA_ARGS__) APRINTER_AS_GET_ARG_PAR_NAME(__VA_ARGS__)
#define APRINTER_AS_MAKE_TEMPLATE_PART(dummy_arg, param_def) APRINTER_AS_MAKE_TEMPLATE_PART_VA param_def

#define APRINTER_AS_MAKE_STRUCT_PART_VA(...) APRINTER_AS_GET_2(__VA_ARGS__) APRINTER_AS_GET_3(__VA_ARGS__) = APRINTER_AS_GET_ARG_PAR_NAME(__VA_ARGS__);
#define APRINTER_AS_MAKE_STRUCT_PART(dummy_arg, param_def) APRINTER_AS_MAKE_STRUCT_PART_VA param_def

#define APRINTER_AS_TYPE(name) (typename, using, name)
#define APRINTER_AS_VALUE(type, name) (type, static type const, name)

#define APRINTER_AS_REMOVE_PARENS(...) __VA_ARGS__

/**
 * Define a template struct which exposes its own template parameters.
 * The argument extra specifies any additional definitions inside
 * the struct. From 1 up to 22 template parameters are supported.
 * 
 * \code
 * APRINTER_ALIAS_STRUCT(MyStruct, (
 *     APRINTER_AS_TYPE(Par1),
 *     APRINTER_AS_VALUE(bool, Par2)
 * ), (
 *     using Extra = void;
 * ))
 * // Expands to
 * template <
 *     typename Param_Par1,
 *     bool Param_Par2
 * >
 * struct MyStruct {
 *     using Par1 = Param_Par1;
 *     static bool const Par2 = Param_Par2;
 *     using Extra = void;
 * };
 * \endcode
 */
#define APRINTER_ALIAS_STRUCT_EXT(name, pars, extra) \
template < APRINTER_AS_MAP(APRINTER_AS_MAKE_TEMPLATE_PART, APRINTER_AS_MAP_DELIMITER_COMMA, 0, pars) > \
struct name { \
    APRINTER_AS_MAP(APRINTER_AS_MAKE_STRUCT_PART, APRINTER_AS_MAP_DELIMITER_NONE, 0, pars) \
    APRINTER_AS_REMOVE_PARENS extra \
};

/**
 * This is just like APRINTER_ALIAS_STRUCT_EXT but without the extra parameter.
 */
#define APRINTER_ALIAS_STRUCT(name, pars) APRINTER_ALIAS_STRUCT_EXT(name, pars, ())

#endif
