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

#ifndef APRINTER_IPSTACK_IP_ETH_HW_H
#define APRINTER_IPSTACK_IP_ETH_HW_H

#include <aprinter/base/Preprocessor.h>
#include <aprinter/base/Assert.h>
#include <aprinter/structure/ObserverNotification.h>

#include <aipstack/misc/Err.h>
#include <aipstack/proto/EthernetProto.h>
#include <aipstack/proto/IpAddr.h>

#include <aipstack/BeginNamespace.h>

/**
 * A collection of abstract interfaces specific to Ethernet-based
 * network interfaced.
 *
 * These are used by higher level network protocols which require
 * the lower-level functionality (e.g. DHCP).
 */
class IpEthHw
{
    APRINTER_USE_TYPES2(APrinter, (Observer, Observable))
    
public:
    class ArpObserver;
    
    /**
     * Interface provided through IpStack::Iface::getHwIface.
     * The associated IpHwType is IpHwType::Ethernet.
     */
    class HwIface
    {
        friend IpEthHw;
        
    public:
        /**
        * Get the MAC address of the interface.
        */
        virtual MacAddr getMacAddr () = 0;
        
        /**
        * Get a reference to the Ethernet header of the current
        * frame being processed.
        * 
        * This MUST NOT be called outside of processing of received frames.
        */
        virtual EthHeader::Ref getRxEthHeader () = 0;
        
        /**
         * Send a broadcast ARP query for the given IP address.
         */
        virtual IpErr sendArpQuery (Ip4Addr ip_addr) = 0;
        
    private:
        /**
         * Return a reference to an ObserverNotification::Observable
         * which provides notification of received ARP updates.
         * To notify observers, the implementation should use
         * Observable::notifyKeepObservers and use notifyArpObserver
         * in its callback.
         */
        virtual Observable & getArpObservable () = 0;
        
    protected:
        /**
         * Nofity one ARP observer; see getArpObservable.
         */
        inline static void notifyArpObserver (Observer &observer, Ip4Addr ip_addr, MacAddr mac_addr)
        {
            ArpObserver &arp_observer = static_cast<ArpObserver &>(observer);
            AMBRO_ASSERT(arp_observer.isActive())
            
            arp_observer.arpInfoReceived(ip_addr, mac_addr);
        }
    };
    
    /**
     * Allows receiving information about ARP updates received
     * on the interface.
     */
    class ArpObserver : private Observer
    {
        friend IpEthHw;
        
    public:
        /**
         * Initialize.
         */
        using Observer::init;
        
        /**
         * Deinitialize.
         */
        using Observer::deinit;
        
        /**
         * Reset (unsubscribe if subscribed).
         */
        using Observer::reset;
        
        /**
         * Check if subscribed.
         */
        using Observer::isActive;
        
        /**
         * Subscribe ARP updates on an interface.
         * 
         * Must not be subscribed already.
         * Updates will be reported using arpInfoReceived.
         * NOTE: You must unsubscribe before deinit/reset of the interface.
         */
        inline void observe (HwIface &hw)
        {
            Observer::observe(hw.getArpObservable());
        }
        
    private:
        /**
         * Reports a single ARP update.
         */
        virtual void arpInfoReceived (Ip4Addr ip_addr, MacAddr mac_addr) = 0;
    };
};

#include <aipstack/EndNamespace.h>

#endif
