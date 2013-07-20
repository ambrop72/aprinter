/*
 * Copyright (c) 2013 Ambroz Bizjak
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

#ifndef AMBROLIB_AXIS_SHARER_H
#define AMBROLIB_AXIS_SHARER_H

#include <stddef.h>

#include <aprinter/meta/WrapCallback.h>
#include <aprinter/meta/ForwardHandler.h>
#include <aprinter/base/OffsetCallback.h>
#include <aprinter/base/DebugObject.h>
#include <aprinter/base/Assert.h>
#include <aprinter/stepper/AxisSplitter.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename AxisStepperParams, typename Stepper, typename GetStepperHandler>
class AxisSharerUser;

template <typename Context, typename AxisStepperParams, typename Stepper, typename GetStepperHandler>
class AxisSharer
: private DebugObject<Context, void>
{
private:
    struct AxisGetStepperHandler;
    struct AxisPullCmdHandler;
    struct AxisBufferFullHandler;
    struct AxisBufferEmptyHandler;
    
public:
    using Axis = AxisSplitter<Context, AxisStepperParams, Stepper, AxisGetStepperHandler, AxisPullCmdHandler, AxisBufferFullHandler, AxisBufferEmptyHandler>;
    using User = AxisSharerUser<Context, AxisStepperParams, Stepper, GetStepperHandler>;
    
    void init (Context c)
    {
        m_axis.init(c);
        m_user = NULL;
        
        this->debugInit(c);
    }
    
    void deinit (Context c)
    {
        this->debugDeinit(c);
        AMBRO_ASSERT(!m_user)
        
        m_axis.deinit(c);
    }
    
    typename Axis::TimerInstance * getTimer ()
    {
        return m_axis.getTimer();
    }
    
private:
    friend User;
    
    void axisPullCmdHandler (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_user)
        
        return m_user->m_pull_cmd_handler(m_user, c);
    }
    
    void axisBufferFullHandler (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_user)
        
        return m_user->m_buffer_full_handler(m_user, c);
    }
    
    void axisBufferEmptyHandler (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_user)
        
        return m_user->m_buffer_empty_handler(m_user, c);
    }
    
    Axis m_axis;
    User *m_user;
    
    struct AxisGetStepperHandler : public AMBRO_FHANDLER_TD(&AxisSharer::m_axis, GetStepperHandler) {};
    struct AxisPullCmdHandler : public AMBRO_WCALLBACK_TD(&AxisSharer::axisPullCmdHandler, &AxisSharer::m_axis) {};
    struct AxisBufferFullHandler : public AMBRO_WCALLBACK_TD(&AxisSharer::axisBufferFullHandler, &AxisSharer::m_axis) {};
    struct AxisBufferEmptyHandler : public AMBRO_WCALLBACK_TD(&AxisSharer::axisBufferEmptyHandler, &AxisSharer::m_axis) {};
};

template <typename Context, typename AxisStepperParams, typename Stepper, typename GetStepperHandler>
class AxisSharerUser
: private DebugObject<Context, void>
{
public:
    using Sharer = AxisSharer<Context, AxisStepperParams, Stepper, GetStepperHandler>;
    using Axis = typename Sharer::Axis;
    
    using PullCmdHandler = void (*) (AxisSharerUser *, Context);
    using BufferFullHandler = void (*) (AxisSharerUser *, Context);
    using BufferEmptyHandler = void (*) (AxisSharerUser *, Context);
    
    void init (Context c, Sharer *sharer, PullCmdHandler pull_cmd_handler, BufferFullHandler buffer_full_handler, BufferEmptyHandler buffer_empty_handler)
    {
        m_sharer = sharer;
        m_pull_cmd_handler = pull_cmd_handler;
        m_buffer_full_handler = buffer_full_handler;
        m_buffer_empty_handler = buffer_empty_handler;
        m_active = false;
        
        this->debugInit(c);
    }
    
    void deinit (Context c)
    {
        this->debugDeinit(c);
        AMBRO_ASSERT(!m_active)
    }
    
    void activate (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(!m_active)
        AMBRO_ASSERT(!m_sharer->m_user)
        AMBRO_ASSERT(!m_sharer->m_axis.isRunning(c))
        
        m_active = true;
        m_sharer->m_user = this;
    }
    
    void deactivate (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_active)
        AMBRO_ASSERT(!m_sharer->m_axis.isRunning(c))
        AMBRO_ASSERT(m_sharer->m_user == this)
        
        m_active = false;
        m_sharer->m_user = NULL;
    }
    
    Axis * getAxis (Context c)
    {
        this->debugAccess(c);
        AMBRO_ASSERT(m_active)
        
        return &m_sharer->m_axis;
    }
    
    bool isActive (Context c)
    {
        this->debugAccess(c);
        
        return m_active;
    }
    
private:
    friend Sharer;
    
    Sharer *m_sharer;
    PullCmdHandler m_pull_cmd_handler;
    BufferFullHandler m_buffer_full_handler;
    BufferEmptyHandler m_buffer_empty_handler;
    bool m_active;
};

#include <aprinter/EndNamespace.h>

#endif
