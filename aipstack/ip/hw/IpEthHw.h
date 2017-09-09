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

#include <aipstack/misc/Err.h>
#include <aipstack/misc/ObserverNotification.h>
#include <aipstack/proto/EthernetProto.h>
#include <aipstack/proto/IpAddr.h>

namespace AIpStack {

/**
 * A collection of abstract interfaces specific to Ethernet-based
 * network interfaced.
 *
 * These are used by higher level network protocols which require
 * the lower-level functionality (e.g. DHCP).
 * 
 * Note that @ref EthIpIface implements these interfaces, so interface drivers
 * based on @ref EthIpIface do not need to directly implement anything here.
 */
class IpEthHw
{
public:
    class ArpObserver;
    
    /**
     * Observable type for ARP updates, see @ref HwIface::getArpObservable.
     */
    using ArpObservable = Observable<ArpObserver>;
    
    /**
     * Interface provided through IpStack::Iface::getHwIface.
     * 
     * The associated IpHwType is IpHwType::Ethernet.
     */
    class HwIface
    {
        friend IpEthHw;
        
    public:
        /**
         * Get the MAC address of the interface.
         * 
         * @return The MAC address.
         */
        virtual MacAddr getMacAddr () = 0;
        
        /**
         * Get a reference to the Ethernet header of the current
         * frame being processed.
         * 
         * This MUST NOT be called outside of processing of received frames.
         * 
         * @return A reference to the received Ethernet header. It must not
         *         be modified and the reference is valid until only in the
         *         scope of receive processing.
         */
        virtual EthHeader::Ref getRxEthHeader () = 0;
        
        /**
         * Send a broadcast ARP query for the given IP address.
         * 
         * @param ip_addr IP address to send the ARP query to.
         * @return Success or error code.
         */
        virtual IpErr sendArpQuery (Ip4Addr ip_addr) = 0;
        
    protected:
        /**
         * Return a reference to an observable which provides notification of
         * received ARP updates.
         * 
         * To notify observers, the implementation should use
         * @ref Observable::notifyKeepObservers and use
         * @ref notifyArpObserver in its notify callback.
         * 
         * @return A reference to the observable.
         */
        virtual ArpObservable & getArpObservable () = 0;
        
        /**
         * Nofity one ARP observer; see @ref getArpObservable.
         * 
         * @param observer The observer to notify.
         * @param ip_addr IP address to which the ARP update applies.
         * @param mac_addr The MAC address associated with this IP address.
         */
        inline static void notifyArpObserver (
            ArpObserver &observer, Ip4Addr ip_addr, MacAddr mac_addr)
        {
            AMBRO_ASSERT(observer.isActive())
            
            observer.arpInfoReceived(ip_addr, mac_addr);
        }
    };
    
    /**
     * Allows receiving notifications about ARP updates received on an Ethernet
     * interface.
     * 
     * This class is based on @ref Observer and the functionality of
     * of that class is exposed. The specific @ref observe function is provided to
     * start observing.
     */
    class ArpObserver :
        public Observer<ArpObserver>
    {
        friend IpEthHw;
        friend ArpObservable;
        
    public:
        /**
         * Subscribe to ARP updates on an Ethernet interface.
         * 
         * Must not be observing already.
         * Updates will be reported using @ref arpInfoReceived.
         * 
         * @param hw The @ref HwIface of the network interface.
         */
        inline void observe (HwIface &hw)
        {
            hw.getArpObservable().addObserver(*this);
        }
        
    protected:
        /**
         * Reports a single ARP update.
         * 
         * @param ip_addr IP address to which the ARP update applies.
         * @param mac_addr The MAC address associated with this IP address.
         */
        virtual void arpInfoReceived (Ip4Addr ip_addr, MacAddr mac_addr) = 0;
    };
};

}

#endif
