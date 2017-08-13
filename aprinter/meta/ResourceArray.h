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

#ifndef APRINTER_RESOURCE_ARRAY_H
#define APRINTER_RESOURCE_ARRAY_H

#include <stddef.h>

#include <type_traits>
#include <utility>

#include <aprinter/base/ExceptionUtils.h>

namespace APrinter {

struct ResourceArrayInitSame {};

template <typename Elem, size_t Size>
class ResourceArray;

namespace ResourceArrayPrivate {
    struct DefaultConstructMixinArg {};
    
    template <bool Has>
    struct DefaultConstructMixin
    {
        DefaultConstructMixin (DefaultConstructMixinArg) {}
    };

    template <>
    struct DefaultConstructMixin<false>
    {
        DefaultConstructMixin (DefaultConstructMixinArg) {}
        DefaultConstructMixin () = delete;
        DefaultConstructMixin (DefaultConstructMixin const &) = default;
        DefaultConstructMixin (DefaultConstructMixin &&) = default;
        DefaultConstructMixin & operator= (DefaultConstructMixin const &) = default;
        DefaultConstructMixin & operator= (DefaultConstructMixin &&) = default;
    };
    
    template <bool Has>
    struct CopyConstructMixin {};

    template <>
    struct CopyConstructMixin<false>
    {
        CopyConstructMixin () = default;
        CopyConstructMixin (CopyConstructMixin const &) = delete;
        CopyConstructMixin (CopyConstructMixin &&) = default;
        CopyConstructMixin & operator= (CopyConstructMixin const &) = default;
        CopyConstructMixin & operator= (CopyConstructMixin &&) = default;
    };
    
    template <bool Has>
    struct MoveConstructMixin {};

    template <>
    struct MoveConstructMixin<false>
    {
        MoveConstructMixin () = default;
        MoveConstructMixin (MoveConstructMixin const &) = default;
        MoveConstructMixin (MoveConstructMixin &&) = delete;
        MoveConstructMixin & operator= (MoveConstructMixin const &) = default;
        MoveConstructMixin & operator= (MoveConstructMixin &&) = default;
    };
    
    template <bool Has>
    struct CopyAssignMixin {};

    template <>
    struct CopyAssignMixin<false>
    {
        CopyAssignMixin () = default;
        CopyAssignMixin (CopyAssignMixin const &) = default;
        CopyAssignMixin (CopyAssignMixin &&) = default;
        CopyAssignMixin & operator= (CopyAssignMixin const &) = delete;
        CopyAssignMixin & operator= (CopyAssignMixin &&) = default;
    };
    
    template <bool Has>
    struct MoveAssignMixin {};

    template <>
    struct MoveAssignMixin<false>
    {
        MoveAssignMixin () = default;
        MoveAssignMixin (MoveAssignMixin const &) = default;
        MoveAssignMixin (MoveAssignMixin &&) = default;
        MoveAssignMixin & operator= (MoveAssignMixin const &) = default;
        MoveAssignMixin & operator= (MoveAssignMixin &&) = delete;
    };
    
    template <typename Elem, size_t Size>
    class ArrayBase
    {
        friend ResourceArray<Elem, Size>;
        
        using Storage = std::aligned_storage_t<sizeof(Elem), alignof(Elem)>;
        
        static_assert(sizeof(Storage) == sizeof(Elem), "");
        
    private:
        Storage m_arr[Size];
        
    public:
        ArrayBase ()
        {
            size_t i;
            APRINTER_TRY {
                for (i = 0; i < Size; i++) {
                    new(elem_ptr(i)) Elem();
                }
            }
            APRINTER_CATCH(..., {
                destruct_elems(i);
                throw;
            })
        }
        
        ArrayBase (ArrayBase const &other)
        {
            size_t i;
            APRINTER_TRY {
                for (i = 0; i < Size; i++) {
                    new(elem_ptr(i)) Elem(*other.elem_ptr(i));
                }
            }
            APRINTER_CATCH(..., {
                destruct_elems(i);
                throw;
            })
        }
        
        ArrayBase (ArrayBase &&other)
        {
            size_t i;
            APRINTER_TRY {
                for (i = 0; i < Size; i++) {
                    new(elem_ptr(i)) Elem(std::move(*other.elem_ptr(i)));
                }
            }
            APRINTER_CATCH(..., {
                destruct_elems(i);
                throw;
            })
        }
        
        template <typename... Args>
        ArrayBase (ResourceArrayInitSame, Args const & ... args)
        {
            size_t i;
            APRINTER_TRY {
                for (i = 0; i < Size; i++) {
                    new(elem_ptr(i)) Elem(args...);
                }
            }
            APRINTER_CATCH(..., {
                destruct_elems(i);
                throw;
            })
        }
        
        ~ArrayBase ()
        {
            destruct_elems(Size);
        }
        
        ArrayBase & operator= (ArrayBase const &other)
        {
            for (size_t i = 0; i < Size; i++) {
                elem_ptr(i)->operator=(*other.elem_ptr(i));
            }
            return *this;
        }
        
        ArrayBase & operator= (ArrayBase &&other)
        {
            for (size_t i = 0; i < Size; i++) {
                elem_ptr(i)->operator=(std::move(*other.elem_ptr(i)));
            }
            return *this;
        }
        
    private:
        inline Elem * elem_ptr (size_t index)
        {
            return reinterpret_cast<Elem *>(&m_arr) + index;
        }
        
        inline Elem const * elem_ptr (size_t index) const
        {
            return reinterpret_cast<Elem const *>(&m_arr) + index;
        }
        
        void destruct_elems (size_t count)
        {
            for (size_t i = count; i > 0; i--) {
                elem_ptr(i - 1)->~Elem();
            }
        }
    };
}

template <typename Elem, size_t Size>
class ResourceArray :
    private ResourceArrayPrivate::ArrayBase<Elem, Size>,
    private ResourceArrayPrivate::DefaultConstructMixin<std::is_default_constructible<Elem>::value>,
    private ResourceArrayPrivate::CopyConstructMixin<std::is_copy_constructible<Elem>::value>,
    private ResourceArrayPrivate::MoveConstructMixin<std::is_move_constructible<Elem>::value>,
    private ResourceArrayPrivate::CopyAssignMixin<std::is_copy_assignable<Elem>::value>,
    private ResourceArrayPrivate::MoveAssignMixin<std::is_move_assignable<Elem>::value>
{
public:
    ResourceArray () = default;
    
    template <typename... Args>
    ResourceArray (ResourceArrayInitSame, Args const & ... args) :
        ResourceArrayPrivate::ArrayBase<Elem, Size>(ResourceArrayInitSame(), args...),
        ResourceArrayPrivate::DefaultConstructMixin<std::is_default_constructible<Elem>::value>(ResourceArrayPrivate::DefaultConstructMixinArg())
    {
    }
    
    inline Elem & operator[] (size_t index)
    {
        return *this->elem_ptr(index);
    }
    
    inline Elem const & operator[] (size_t index) const
    {
        return *this->elem_ptr(index);
    }
    
    inline Elem * data ()
    {
        return this->elem_ptr(0);
    }
    
    inline Elem const * data () const
    {
        return this->elem_ptr(0);
    }
    
    inline static size_t size ()
    {
        return Size;
    }
    
    using iterator = Elem *;
    
    using const_iterator = Elem const *;
    
    inline iterator begin ()
    {
        return this->elem_ptr(0);
    }
    
    inline const_iterator begin () const
    {
        return this->elem_ptr(0);
    }
    
    inline iterator end ()
    {
        return this->elem_ptr(Size);
    }
    
    inline const_iterator end () const
    {
        return this->elem_ptr(Size);
    }
};

}

#endif
