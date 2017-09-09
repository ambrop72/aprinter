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

#ifndef AIPSTACK_OPTIONS_H
#define AIPSTACK_OPTIONS_H

#include <aprinter/meta/BasicMetaUtils.h>
#include <aprinter/meta/TypeListUtils.h>
#include <aprinter/meta/TypeDict.h>

namespace AIpStack {

namespace OptionsPrivate {
    template <typename Derived, typename DefaultValue, typename... Options>
    using GetValue = APrinter::TypeDictGetOrDefault<
        APrinter::TypeListReverse<APrinter::MakeTypeList<Options...>>, Derived, DefaultValue
    >;
}

/**
 * Represents a configuration option which defines a value (as opposed to a type).
 * 
 * @tparam Derived The derived class identifying this option.
 * @tparam ValueType The type of the option value.
 * @tparam DefaultValue The default value if none is provided.
 */
template <typename Derived, typename ValueType, ValueType DefaultValue>
class ConfigOptionValue {
public:
    /**
     * Provide a value for the option.
     * 
     * Types obtained via this alias are passed as one of the variadic template
     * parameters to various "Config" template classes in order to specify the
     * value of an option.
     * 
     * @tparam Value The desired value for the option.
     */
    template <ValueType Value>
    using Is = APrinter::TypeDictEntry<Derived, APrinter::WrapValue<ValueType, Value>>;
    
#ifndef DOXYGEN_SHOULD_SKIP_THIS
    template <typename... Options>
    struct Config {
        static constexpr ValueType Value = OptionsPrivate::GetValue<
            Derived, APrinter::WrapValue<ValueType, DefaultValue>, Options...
        >::Value;
    };
#endif
};

/**
 * Represents a configuration option which defines a type (as opposed to a value).
 * 
 * @tparam Derived The derived class identifying this option.
 * @tparam DefaultValue The default value if none is provided.
 */
template <typename Derived, typename DefaultValue>
class ConfigOptionType {
public:
    /**
     * Provide a value for the option.
     * 
     * Types obtained via this alias are passed as one of the variadic template
     * parameters to various "Config" template classes in order to specify the
     * value of an option.
     * 
     * @tparam Value The desired value for the option.
     */
    template <typename Value>
    using Is = APrinter::TypeDictEntry<Derived, Value>;
    
#ifndef DOXYGEN_SHOULD_SKIP_THIS
    template <typename... Options>
    struct Config {
        using Value = OptionsPrivate::GetValue<Derived, DefaultValue, Options...>;
    };
#endif
};

#define AIPSTACK_OPTION_DECL_VALUE(name, type, default) \
class name : public AIpStack::ConfigOptionValue<name, type, default> {};

#define AIPSTACK_OPTION_DECL_TYPE(name, default) \
class name : public AIpStack::ConfigOptionType<name, default> {};

#define AIPSTACK_OPTION_CONFIG_VALUE(decls, name) \
static constexpr auto name = decls::name::Config<Options...>::Value;

#define AIPSTACK_OPTION_CONFIG_TYPE(decls, name) \
using name = typename decls::name::Config<Options...>::Value;

}

#endif
