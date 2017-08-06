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

#ifndef APRINTER_HTTP_PATH_PARSER_H
#define APRINTER_HTTP_PATH_PARSER_H

#include <stddef.h>
#include <string.h>

#include <aprinter/base/MemRef.h>
#include <aprinter/base/Assert.h>
#include <aprinter/misc/StringTools.h>

namespace APrinter {

template <int MaxQueryParams>
class HttpPathParser {
    static_assert(MaxQueryParams >= 2 && MaxQueryParams <= 32, "");
    
public:
    /**
     * Parses the given request path into a path string and a list of query parameters.
     * The given memory region must be writable and there must be an extra free byte
     * at the end. This is because the components of the path are percent-decoded and
     * null-terminated in-place.
     */
    void parse (MemRef data)
    {
        // First phase: split the request path into its components.
        // These are the "path" and a list of query parameters, each consisting
        // of a name and value. The input is not modified in this phase.
        
        m_num_params = 0;
        
        char const *qmark = (char const *)memchr(data.ptr, '?', data.len);
        if (!qmark) {
            // No question mark, the whole input is the "path".
            m_path = data;
        } else {
            // Question mark found. The "path" is the part until the question mark.
            size_t path_len = qmark - data.ptr;
            m_path = data.subTo(path_len);
            
            // Skip to just after the question mark and continue with parsing the parameters.
            data = data.subFrom(path_len+1);
            
            while (data.len > 0) {
                // The parameter ends either at the and-sign or at the end of the input.
                char const *andsign = (char *)memchr(data.ptr, '&', data.len);
                size_t param_len = andsign ? (andsign - data.ptr) : data.len;
                
                // Examine the parameter only if it is not empty and we still have space.
                if (param_len > 0 && m_num_params < MaxQueryParams) {
                    char *eqsign = (char *)memchr(data.ptr, '=', param_len);
                    if (!eqsign) {
                        // There is no equal sign, assume that the value is en empty string.
                        // We carefully point the value to the end of the parameter name,
                        // so that nothing bad will happen later in decode_value().
                        m_params[m_num_params++] = QueryParam{data.subTo(param_len), MemRef(data.ptr + param_len, 0)};
                    } else {
                        // There is an equal sign, so extract the name and the value.
                        // We explicitly ignore parameters with empty names, because
                        // this is not implied by param_len>0.
                        size_t name_len = eqsign - data.ptr;
                        if (name_len > 0) {
                            m_params[m_num_params++] = QueryParam{data.subTo(name_len), data.subTo(param_len).subFrom(name_len+1)};
                        }
                    }
                }
                
                // Stop it we reached the end, else continue after the and-sign.
                if (!andsign) {
                    break;
                }
                data = data.subFrom(param_len+1);
            }
        }
        
        // Second phase: percent-decode and null-terminate all the components in-place.
        // Percent-decoding is safe because the result cannot be longer than the input.
        // Null-termination is safe because we overwrite '?', '&' or '=', or make use
        // of an extra free byte at the end of the input which must be provided.
        
        m_path = decode_and_terminate(m_path);
        
        auto num_params = m_num_params;
        for (int i = 0; i < num_params; i++) {
            QueryParam *param = &m_params[i];
            param->name = decode_and_terminate(param->name);
            param->value = decode_and_terminate(param->value);
        }
        
    }
    
    MemRef getPath () const
    {
        return m_path;
    }
    
    int getNumParams () const
    {
        return m_num_params;
    }
    
    void getParam (int idx, MemRef *name, MemRef *value) const
    {
        AMBRO_ASSERT(idx < m_num_params)
        
        *name = m_params[idx].name;
        *value = m_params[idx].value;
    }
    
    bool getParam (MemRef name, MemRef *value=nullptr) const
    {
        auto num_params = m_num_params;
        for (int i = 0; i < num_params; i++) {
            if (m_params[i].name.equalTo(name)) {
                if (value) {
                    *value = m_params[i].value;
                }
                return true;
            }
        }
        return false;
    }
    
private:
    static MemRef decode_and_terminate (MemRef data)
    {
        char const *start_ptr = data.ptr;
        char *end_ptr = (char *)start_ptr;
        
        while (data.len > 0) {
            char c = data.ptr[0];
            
            if (c == '%' && data.len >= 3) {
                int digit1;
                int digit2;
                if (StringDecodeHexDigit(data.ptr[1], &digit1) && StringDecodeHexDigit(data.ptr[2], &digit2)) {
                    *(unsigned char *)end_ptr++ = (digit1 << 4) | digit2;
                    data = data.subFrom(3);
                    continue;
                }
            }
            
            *end_ptr++ = c;
            data = data.subFrom(1);
        }
        
        *end_ptr = '\0';
        
        return MemRef(start_ptr, end_ptr - start_ptr);
    }
    
    struct QueryParam {
        MemRef name;
        MemRef value;
    };
    
    MemRef m_path;
    int m_num_params;
    QueryParam m_params[MaxQueryParams];
};

}

#endif
