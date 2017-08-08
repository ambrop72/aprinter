#include <stdio.h>

#include <cassert>
#include <utility>
#include <type_traits>

struct FacadeEmptyState {};

template <typename Impl>
struct FacadePtrState {
    inline FacadePtrState (Impl *impl) :
        impl(impl)
    {}
    
    Impl *impl;
};

template <typename Impl>
using FacadeState = typename std::conditional<
    Impl::ImplIsStatic,
    FacadeEmptyState,
    FacadePtrState<Impl>
>::type;

template <typename Func>
struct GetReturnTypeHelper;

template <typename Ret, typename... Args>
struct GetReturnTypeHelper<Ret(Args...)> {
    using Result = Ret;
};

template <typename Func>
using GetReturnType = typename GetReturnTypeHelper<Func>::Result;

template <typename Impl>
class FacadeClass :
    private FacadeState<Impl>
{
public:
    static bool const ImplIsStatic = Impl::ImplIsStatic;
    
    using State = FacadeState<Impl>;
    
private:
    template <typename Func, typename... Args>
    inline GetReturnType<Func> callImpl (Func Impl::*func_ptr, Args && ... args) const
    {
        static_assert(!ImplIsStatic, "");
        Impl *impl = static_cast<FacadePtrState<Impl> const &>(*this).impl;
        return (impl->*func_ptr)(args...);
    }
    
    template <typename Func, typename... Args>
    inline GetReturnType<Func> callImpl (Func *func_ptr, Args && ... args) const
    {
        return (*func_ptr)(args...);
    }
    
    template <typename Object, typename Func, typename... Args>
    inline static GetReturnType<Func> callObject (Func Object::*func_ptr, Object &obj, Args && ... args)
    {
        return (obj.*func_ptr)(args...);
    }
    
    template <typename Object, typename Func, typename... Args>
    inline static GetReturnType<Func> callObject (Func *func_ptr, Object &obj, Args && ... args)
    {
        (void)obj;
        return (*func_ptr)(args...);
    }
    
public:
    inline FacadeClass (State state = State()) :
        State(state)
    {
    }
    
    inline State state () const
    {
        return static_cast<State>(*this);
    }
    
    inline int foo () const
    {
        return callImpl<int()>(&Impl::foo);
    }
    
    inline float bar (int arg1, int arg2) const
    {
        return callImpl<float(int, int)>(&Impl::bar, arg1, arg2);
    }
    
    class Cat :
        private Impl::Cat
    {
        using ImplCat = typename Impl::Cat;
        
    public:
        inline Cat (FacadeClass facade, int arg) :
            ImplCat(facade.state(), arg)
        {
        }
        
        inline bool meow (bool loud)
        {
            return callObject<ImplCat, bool(bool)>(&Impl::Cat::meow, *this, loud);
        }
        
        inline bool bark (bool loud)
        {
            return callObject<ImplCat, bool(bool)>(&Impl::Cat::bark, *this, loud);
        }
    };
};

template <typename Impl, typename Derived>
class FacadeUser :
    private FacadeState<Impl>
{
public:
    using Facade = FacadeClass<Impl>;
    
    inline FacadeUser (FacadeState<Impl> state = FacadeState<Impl>()) :
        FacadeState<Impl>(state)
    {
    }
    
    inline Facade facade () const
    {
        return Facade(static_cast<FacadeState<Impl>>(*this));
    }
};

class NonstaticImpl {
    int m_state;
    
public:
    static bool const ImplIsStatic = false;
    
    NonstaticImpl (int state) :
        m_state(state)
    {}
    
    int foo ()
    {
        return 5 + m_state;
    }
    
    static float bar (int arg1, int arg2)
    {
        return arg1 * 3.0 + arg2;
    }
    
    class Cat
    {
        NonstaticImpl *m_impl;
        int m_arg;
        
    public:
        Cat (FacadePtrState<NonstaticImpl> state, int arg) :
            m_impl(state.impl),
            m_arg(arg)
        {
        }
        
        bool meow (bool loud)
        {
            return m_arg;
        }
        
        static bool bark (bool loud)
        {
            return !loud;
        }
    };
};

class StaticImpl {
public:
    static bool const ImplIsStatic = true;
    
    static int foo ()
    {
        return 1;
    }
    
    static float bar (int arg1, int arg2)
    {
        return arg1 + arg2;
    }
    
    class Cat
    {
        int m_arg;
        
    public:
        Cat (FacadeEmptyState, int arg) :
            m_arg(arg)
        {
        }
        
        static bool meow (bool loud)
        {
            return loud;
        }
        
        bool bark (bool loud)
        {
            return !loud;
        }
    };
};

template <typename Impl>
class User :
    private FacadeUser<Impl, User<Impl>>
{
    using FacadeUser<Impl, User<Impl>>::facade;
    
public:
    User (FacadeState<Impl> fs) :
        FacadeUser<Impl, User<Impl>>(fs)
    {
        facade().foo();
        facade().bar(1, 2);
    }
};

template <typename Impl>
class User2 :
    private FacadeUser<Impl, User2<Impl>>,
    private User<Impl>
{
    using FacadeUser<Impl, User2<Impl>>::facade;
    using Facade = typename FacadeUser<Impl, User2<Impl>>::Facade;
    
    User<Impl> m_user;
    
public:
    User2 (FacadeState<Impl> fs) :
        FacadeUser<Impl, User2<Impl>>(fs),
        User<Impl>(fs),
        m_user(fs)
    {
        int a = facade().foo();
        float b = facade().bar(1, 2);
        printf("%d %f\n", a, b);
        
        typename Facade::Cat cat(facade(), 0);
        bool c = cat.meow(false);
        bool d = cat.bark(false);
        printf("%d %d\n", c, d);
    }
};

int main()
{
    {
        NonstaticImpl impl(3);
        User2<NonstaticImpl> user2(&impl);
    }
    
    {
        User2<StaticImpl> user2{FacadeEmptyState()};
    }
    
    return 0;
}
