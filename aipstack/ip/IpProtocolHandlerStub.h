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

#ifndef APRINTER_IPSTACK_IP_PROTOCOL_HANDLER_STUB_H
#define APRINTER_IPSTACK_IP_PROTOCOL_HANDLER_STUB_H

#include <stdint.h>

#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/meta/BasicMetaUtils.h>
#include <aprinter/base/NonCopyable.h>

#include <aipstack/misc/Buf.h>
#include <aipstack/ip/IpStackHelperTypes.h>
#include <aipstack/platform/PlatformFacade.h>

namespace AIpStack {

/**
 * Stub IP protocol handler documenting the required interface.
 * 
 * This dummy class exists to document the interface for protocol handlers
 * expected by @ref IpStack, especially functions called by the stack.
 * 
 * The @ref IpStack is configured with a list of protocol handlers at
 * compile time and not at runtime. This is for performance reasons and
 * to simplify access to protocol handlers from the application.
 * 
 * Each protocol handler is defined through its corresponding service class.
 * (see @ref IpProtocolHandlerStubService) and the list of supported
 * protocol handlers is passed to @ref IpStackService when the @ref IpStack
 * is being instantiated. The @ref IpStack then instantiates the protocol
 * handler service classes to obtain the class types of the actual protocol
 * handles (such as an instantiated @ref IpProtocolHandlerStub in this case).
 * 
 * This protocol handler class provides different functions called by
 * the stack (using simple function calls, not virtual). It is the
 * responsibility of the implementor to make sure that the functions
 * are defined and compatible with how they are called. Note that the stack
 * static_cast's the function arguments to the types specified in this
 * documentation.
 * 
 * This class is also what is exposed via @ref IpStack::getProtocol,
 * so generally it will provide the application-level API for using the
 * implemented IP protocol in addition to the functions called by the stack.
 * 
 * This class only defines the functions called by the stack on the
 * protocol handler. Functions which the protocol handler can call on
 * the stack are defined in @ref IpStack class; perhaps the most important
 * one is @ref IpStack::sendIp4Dgram which is used for sending datagrams.
 * 
 * The stack offers the facilities needed to support Path MTU Discovery.
 * This can be achieved using the @ref IpStack::MtuRef class and the functions
 * @ref IpStack::handleIcmpPacketTooBig and
 * @ref IpStack::handleLocalPacketTooBig.
 * 
 * The protocol handler will have access to the interface (@ref IpStack::Iface)
 * from which a datagram has been received, in @ref recvIp4Dgram and also
 * @ref handleIp4DestUnreach. The protocol handler must not remember the
 * interface since it could generally disappear at any time. Currently there
 * is no mechanism to safely keep a reference to an interface.
 * 
 * The @ref IpStack only constructs and destructs protocol handlers, never
 * copies, moves, copy-assigns or move-assigns. Therefore the protocol handler
 * class can have such functions deleted, possibly by inheriting
 * @ref APrinter::NonCopyable.
 * 
 * @tparam Arg Instantiation parameters, that is a type derived from an
 *         instantiation of the Compose template in the service class for the
 *         protocol handlers, e.g. @ref IpProtocolHandlerStubService::Compose.
 *         Therefore all type aliases defined there are available in Arg.
 */
template <typename Arg>
class IpProtocolHandlerStub :
    private APrinter::NonCopyable<IpProtocolHandlerStub<Arg>>
{
public:
    /**
     * Type alias for the platform layer implementation.
     * 
     * This alias is not required but is used here for definitions.
     */
    using PlatformImpl = typename Arg::PlatformImpl;
    
    /**
     * Platform facade type.
     * 
     * This alias is not required but is used in definitions here.
     */
    using Platform = PlatformFacade<PlatformImpl>;
    
    /**
     * Type alias for the instantiated @ref IpStack as passed to Compose.
     * 
     * This alias is not required but is used in definitions here.
     */
    using TheIpStack = typename Arg::TheIpStack;
    
    /**
     * Type alias for @ref IpStack::Ip4RxInfo.
     * 
     * This alias is not required but is used in definitions here.
     */
    using Ip4RxInfo = typename TheIpStack::Ip4RxInfo;
    
    /**
     * Type alias for @ref IpStack::ProtocolHandlerArgs.
     * 
     * This alias is not required but is used in definitions here.
     */
    using ProtocolHandlerArgs = typename TheIpStack::ProtocolHandlerArgs;
    
    /**
     * Construct the protocol handler.
     * 
     * Protocol handlers are constructed as the last part of the
     * @ref IpStack::IpStack constructor in the order they are configured.
     * Therefore they can access the @ref IpStack, but since there are no interfaces
     * yet any attempt to send datagrams will fail.
     * 
     * @param args Encapsulated construction arguments. These include the
     *        pointer to the @ref IpStack and the platform facade (@ref Platform).
     *        The protocol handler should remember the @ref IpStack pointer so
     *        that it can access the stack later.
     */
    IpProtocolHandlerStub (ProtocolHandlerArgs args) :
        m_stack(args.stack)
    {
    }
    
    /**
     * Destruct the protocol handler.
     * 
     * Protocol handlers are destructed as the first part of the
     * @ref IpStack::~IpStack destructor, in the reverse order of how they were
     * constructed.
     */
    ~IpProtocolHandlerStub ()
    {
    }
    
    /**
     * Process a received IPv4 datagram for this protocol.
     * 
     * This is called when a (possibly reassembled) IPv4 datagram is
     * received containing the protocol number of this protocol handler.
     * 
     * The following has already been done by the stack:
     * - Basic checks of the IP header (e.g. IP version, header length,
     *   total length).
     * - Verification of the IP header checksum.
     * - Reassembly of fragmented packets.
     * - Invocation of any interface listener callbacks
     *   (see @ref IpStack::IfaceListener). Note that interfaced listeners
     *   have the ability to inhibit processing by protocol handlers.
     * 
     * Generally, the source and destination addresses have not been
     * checked. It is the responsibility of the protocol handler to check
     * them as needed, especially to ignore packets not intended for this
     * host.
     * 
     * @param ip_info Information about the received datagram.
     * @param dgram IP payload of the datagram. The referenced buffers
     *        must not be used outside of this function.
     */
    void recvIp4Dgram (Ip4RxInfo const &ip_info, IpBufRef dgram)
    {
    }
    
    /**
     * Process a received IPv4 ICMP Destination Unreachable message for
     * this protocol.
     * 
     * This function can be used to support the Path MTU Discovery process.
     * In this case, if the ICMP code (du_meta.icmp_code) is
     * @ref Icmp4CodeDestUnreachFragNeeded, the protocol handler should perform
     * protocol-specific sanity checks (e.g. TCP could check that the message is
     * for an active connection) then call @ref IpStack::handleIcmpPacketTooBig
     * if the checks pass. The latter may result in @ref IpStack::MtuRef::pmtuChanged
     * callbacks being called.
     * 
     * Note that to fully support Path MTU Discovery, the protocol handler
     * should also call @ref IpStack::handleLocalPacketTooBig when sending
     * fails with the @ref IpErr::FRAG_NEEDED error, in order to handle
     * reduction of the interface MTU.
     * 
     * The following has already been checked/determined by the stack:
     * - The source address of the ICMP packet looks like a unicast address
     *   (@ref IpStack::checkUnicastSrcAddr).
     * - The destination address of the ICMP packet is for the incoming
     *   interface address or a broadcast address.
     * - The ICMP header is present and the ICMP checksum is correct.
     * - The ICMP type is Destination Unreachable.
     * - The encapsulated IP header has the correct IP version (4),
     *   is complete (encapsulated data includes all options) and the specified
     *   total length is not less than the header length.
     * - The IP protocol number in the encapsulated IP header is the protocol
     *   number of this protocol handler.
     * 
     * @param du_meta Information about the Destination Unreachable datagram.
     * @param ip_info Information from the ensapsulated IP header. Except that
     *        the 'iface' member is the interface from which the ICMP message
     *        was received.
     * @param dgram_initial The encapsulated IP data as included in the ICMP
     *        message, typically only a small portion of the data of the
     *        datagram to which the message refers. The referenced buffers must
     *        not be used outside of this function. Note that the non-truncated
     *        length (as in the encapsulated IP header) is not available, it
     *        can be added if ever needed. 
     */
    void handleIp4DestUnreach (
        Ip4DestUnreachMeta const &du_meta,
        Ip4RxInfo const &ip_info, IpBufRef dgram_initial)
    {
    }
    
private:
    TheIpStack *m_stack;
};

/**
 * Example protocol handler service definition for @ref IpProtocolHandlerStub.
 * 
 * This type would be passed as an element of the ProtocolServicesList
 * template parameter to @ref IpStackService::Compose.
 * 
 * Note that the service definition for a real protocol handler will
 * typically be a template to allow some configurable parameters.
 */
struct IpProtocolHandlerStubService {
    /**
     * The IP protocol number of the protocol handler.
     * 
     * This type alias must appear so that the stack knows which protocol
     * number the handler is responsible for. The type alias should be for an
     * @ref APrinter::WrapValue type using uint8_t as the value type.
     */
    using IpProtocolNumber = APrinter::WrapValue<uint8_t, 99>;
    
    /**
     * Template through which the @ref IpStack instantiates the service.
     * 
     * The template parameters of this class must be as shown here since
     * this is what @ref IpStack passes. This Compose class will be accessible
     * in the protocol handler class template as the 'Arg' template parameter
     * (actually Arg will be a class derived from Compose).
     * 
     * The @ref APRINTER_DEF_INSTANCE / @ref APRINTER_MAKE_INSTANCE system
     * is used to obtain the actual protocol handler class type, in this
     * case an @ref IpProtocolHandlerStub template class. 
     * 
     * @tparam Param_PlatformImpl The platform implementation class as used by
     *         the stack. Note that this is the original PlatformImpl type
     *         provided by to @ref IpStackService::Compose and not the
     *         @ref PlatformFacade.
     * @tparam Param_TheIpStack The @ref IpStack template class type.
     */
    APRINTER_ALIAS_STRUCT_EXT(Compose, (
        APRINTER_AS_TYPE(PlatformImpl),
        APRINTER_AS_TYPE(TheIpStack)
    ), (
        /**
         * Alias for accessing the service definition.
         */
        using Params = IpProtocolHandlerStubService;
        
        /**
         * Template alias to instantiate the service.
         */
        APRINTER_DEF_INSTANCE(Compose, IpProtocolHandlerStub)
    ))
};

}

#endif
