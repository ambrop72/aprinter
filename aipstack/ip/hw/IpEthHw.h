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

#include <aprinter/base/Assert.h>

#include <aipstack/misc/Err.h>
#include <aipstack/misc/SendRetry.h>
#include <aipstack/proto/IpAddr.h>
#include <aipstack/proto/EthernetProto.h>

#include <aipstack/BeginNamespace.h>

class IpEthHwArpQuery;

/**
 * Hardware-specific interface for Ethernet interfaces.
 */
class IpEthHw
{
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
    
protected:
    /**
     * Start an ARP query for a specific IP address.
     * 
     * This is used by DHCP to check than an offered IP address
     * is not used.
     * 
     * If there is no error, this function must queue the query to an
     * IpSendRetry::List (using IpSendRetry::List::addSpecialRequest)
     * and return IpErr::SUCCESS. In case of an error, this function
     * must return different than IpErr::SUCCESS and must not add the
     * query to a list.
     * 
     * After an ARP message arrives from the specified IP address, the
     * implementation should call reportArpQueryResponse to report the
     * result of the query, as part of the special-request callback
     * to IpSendRetry::List::dispatchRequests.
     * 
     * @param query Object representing the ARP query.
     * @param ip_addr The IP address to query.
     * @return IpErr::SUCCESS or other value in case of error.
     */
    virtual IpErr startArpQuery (IpEthHwArpQuery &query, Ip4Addr ip_addr) = 0;
    
protected:
    /**
     * Report the result of an ARP query (see startArpQuery).
     */
    inline static void reportArpQueryResponse (IpEthHwArpQuery &query, MacAddr mac_addr);
};

/**
 * Represents an ARP query used with IpEthHw.
 */
class IpEthHwArpQuery :
    public IpSendRetry::SpecialRequest<IpSendRetry::RequestTypeIpEthHwArpQuery>
{
    using SpecialRequest = IpSendRetry::SpecialRequest<IpSendRetry::RequestTypeIpEthHwArpQuery>;
    
    friend class IpEthHw;
    
public:
    /**
     * Initialize the object.
     */
    using SpecialRequest::init;
    
    /**
     * Deinitialize the object.
     */
    using SpecialRequest::deinit;
    
    /**
     * Reset an initialized object to the default inactive state.
     */
    using SpecialRequest::reset;
    
    /**
     * Check if an ARP query is active.
     * 
     * @return True if active, false if not.
     */
    using SpecialRequest::isActive;
    
    /**
     * Start an ARP query for some IP address.
     * 
     * Upon success, the object becomes active (isActive will return true).
     * Upon error, the object will be inactive (even if it was active
     * before the call).
     * 
     * If a response to the query is received, it will be reported
     * by calling the virtual function queryResponse. The query will
     * be automatically deactivated just before the call.
     * 
     * The query may be automatically deactivated, for example due to a
     * timeout or the ARP cache entry being reused. The user is not
     * notified about such deactivation.
     * 
     * @param hw The IpEthHw interface for the query.
     * @param ip_addr The IP address to query.
     * @return IpErr::SUCCESS or other value in case of error
     */
    IpErr startQuery (IpEthHw &hw, Ip4Addr ip_addr)
    {
        reset();
        IpErr result = hw.startArpQuery(*this, ip_addr);
        AMBRO_ASSERT(isActive() == (result == IpErr::SUCCESS))
        return result;
    }
    
protected:
    /**
     * Reports a reponse to the ARP query.
     * 
     * This will only be called when a query is active, and
     * the object will become inactive just before the call.
     * 
     * @param mac_addr The MAC address found for the queried IP address.
     */
    virtual void queryResponse (MacAddr mac_addr) = 0;
};

void IpEthHw::reportArpQueryResponse (IpEthHwArpQuery &query, MacAddr mac_addr)
{
    AMBRO_ASSERT(!query.isActive()) // was just dequeued
    
    query.queryResponse(mac_addr);
}

#include <aipstack/EndNamespace.h>

#endif
