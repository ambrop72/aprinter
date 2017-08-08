#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>

#include <aipstack/platform/PlatformFacade.h>

class StaticImpl
{
    using PlatformRef = AIpStack::PlatformRef<StaticImpl>;
    
public:
    static bool const ImplIsStatic = true;
    
    using TimeType = uint32_t;
    
    static constexpr double TimeFreq = 1e3;
    
    static TimeType getTime ()
    {
        return 10;
    }
    
    class Timer :
        public PlatformRef
    {
    private:
        bool m_set;
        TimeType m_time;
        
    public:
        Timer (PlatformRef ref) :
            PlatformRef(ref),
            m_set(false),
            m_time(0)
        {
        }
        
        inline bool isSet ()
        {
            return m_set;
        }
        
        inline TimeType getSetTime ()
        {
            return m_time;
        }
        
        void unset ()
        {
            m_set = false;
        }
        
        void setAt (TimeType abs_time)
        {
            m_set = true;
            m_time = abs_time;
        }
        
        void testCallExpired ()
        {
            handleTimerExpired();
        }
        
    protected:
        virtual void handleTimerExpired () = 0;
    };
};

class NonstaticImpl
{
    using PlatformRef = AIpStack::PlatformRef<NonstaticImpl>;
    
public:
    static bool const ImplIsStatic = false;
    
    using TimeType = uint32_t;
    
    static constexpr double TimeFreq = 1e3;
    
private:
    TimeType m_time;
    
public:
    NonstaticImpl (TimeType time) :
        m_time(time)
    {
    }
    
    TimeType getTime ()
    {
        return m_time;
    }
    
    class Timer :
        public PlatformRef
    {
    private:
        bool m_set;
        TimeType m_time;
        
    public:
        Timer (PlatformRef ref) :
            PlatformRef(ref),
            m_set(false),
            m_time(0)
        {
        }
        
        inline bool isSet ()
        {
            return m_set;
        }
        
        inline TimeType getSetTime ()
        {
            return m_time;
        }
        
        void unset ()
        {
            m_set = false;
        }
        
        void setAt (TimeType abs_time)
        {
            m_set = true;
            m_time = abs_time;
        }
        
        void testCallExpired ()
        {
            handleTimerExpired();
        }
        
    protected:
        virtual void handleTimerExpired () = 0;
    };
};

template <typename PlatformImpl>
class UserScope
{
public:
    class User1;
    
private:
    using Platform = AIpStack::PlatformFacade<PlatformImpl>;
    using Timer = typename Platform::template TimerWrapper<User1>;
    using PlatformRef = typename Platform::template RefWrapper<User1>;
    
public:
    class User1 :
        private Timer,
        private PlatformRef
    {
    public:
        User1 (Platform platform) :
            Timer(platform),
            PlatformRef(platform)
        {
        }
        
        void test ()
        {
            Timer::setAt(123);
            Timer::impl().testCallExpired();
            
            Timer::unset();
            Timer::impl().testCallExpired();
            
            Timer::setAfter(30);
            Timer::impl().testCallExpired();
            
            PlatformRef::platform();
        }
        
    private:
        void handleTimerExpired () override final
        {
            ::printf("handleTimerExpired %d %" PRIu32 "\n",
                Platform::Timer::isSet(), Platform::Timer::getSetTime());
        }
    };
};

int main ()
{
    {
        using Platform = AIpStack::PlatformFacade<StaticImpl>;    
        UserScope<StaticImpl>::User1 user{Platform()};
        user.test();
    }
    
    {
        NonstaticImpl impl{6};
        using Platform = AIpStack::PlatformFacade<NonstaticImpl>;
        UserScope<NonstaticImpl>::User1 user{Platform(&impl)};
        user.test();
    }
    
    return 0;
}
