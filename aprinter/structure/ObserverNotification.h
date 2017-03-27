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

#ifndef APRINTER_OBSERVER_NOTIFICATION_H
#define APRINTER_OBSERVER_NOTIFICATION_H

#include <aprinter/base/Assert.h>

#include <aprinter/BeginNamespace.h>

class ObserverNotification
{
    struct ListNode {
        ListNode **m_prev;
        ListNode *m_next;
    };
    
public:
    class Observer;
    
    class Observable
    {
        friend ObserverNotification;
        
    private:
        ListNode *m_first;
        
    public:
        inline void init ()
        {
            m_first = nullptr;
        }
        
        inline bool hasObservers ()
        {
            return m_first != nullptr;
        }
        
        void removeObservers ()
        {
            for (ListNode *node = m_first; node != nullptr; node = node->m_next) {
                AMBRO_ASSERT(node->m_prev != nullptr)
                node->m_prev = nullptr;
            }
            m_first = nullptr;
        }
        
        class NotificationIterator
        {
        private:
            Observer *m_observer;
            ListNode m_temp_node;
            
        public:
            inline NotificationIterator (Observable &observable)
            {
                m_observer = static_cast<Observer *>(observable.m_first);
            }
            
            template <bool RemoveNotified>
            Observer * beginNotify ()
            {
                if (m_observer == nullptr) {
                    return nullptr;
                }
                
                m_temp_node.m_next = m_observer->m_next;
                
                if (RemoveNotified) {
                    m_temp_node.m_prev = m_observer->m_prev;
                    
                    m_observer->m_prev = nullptr;
                    
                    AMBRO_ASSERT(*m_temp_node.m_prev == m_observer)
                    *m_temp_node.m_prev = &m_temp_node;
                } else {
                    m_temp_node.m_prev = &m_observer->m_next;
                    
                    m_observer->m_next = &m_temp_node;
                }
                
                if (m_temp_node.m_next != nullptr) {
                    AMBRO_ASSERT(m_temp_node.m_next->m_prev == &m_observer->m_next)
                    m_temp_node.m_next->m_prev = &m_temp_node.m_next;
                }
                
                return m_observer;
            }
            
            void endNotify ()
            {
                m_observer = static_cast<Observer *>(m_temp_node.m_next);
                
                remove_node(m_temp_node);
            }
        };
        
        template <bool RemoveNotified, typename NotifyFunc>
        void notifyObservers (NotifyFunc notify)
        {
            NotificationIterator iter(*this);
            
            while (Observer *observer = iter.template beginNotify<RemoveNotified>()) {
                notify(*observer);
                iter.endNotify();
            }
        }
        
        template <typename EnumerateFunc>
        void enumerateObservers (EnumerateFunc enumerate)
        {
            for (ListNode *node = m_first; node != nullptr; node = node->m_next) {
                enumerate(static_cast<Observer &>(*node));
            }
        }
        
    private:
        inline void prepend_node (ListNode &node)
        {
            node.m_prev = &m_first;
            node.m_next = m_first;
            if (node.m_next != nullptr) {
                AMBRO_ASSERT(node.m_next->m_prev == &m_first)
                node.m_next->m_prev = &node.m_next;
            }
            m_first = &node;
        }
        
        inline static void remove_node (ListNode &node)
        {
            AMBRO_ASSERT(*node.m_prev == &node)
            *node.m_prev = node.m_next;
            if (node.m_next != nullptr) {
                AMBRO_ASSERT(node.m_next->m_prev == &node.m_next)
                node.m_next->m_prev = node.m_prev;
            }
        }
    };

    class Observer :
        private ListNode
    {
        friend ObserverNotification;
        
    public:
        inline void init ()
        {
            m_prev = nullptr;
        }
        
        inline void deinit ()
        {
            reset();
        }
        
        inline bool isObserving ()
        {
            return m_prev != nullptr;
        }
        
        void reset ()
        {
            if (m_prev != nullptr) {
                Observable::remove_node(*this);
                m_prev = nullptr;
            }
        }
        
        void observe (Observable &observable)
        {
            AMBRO_ASSERT(!isObserving())
            
            observable.prepend_node(*this);
        }
    };
};

#include <aprinter/EndNamespace.h>

#endif
