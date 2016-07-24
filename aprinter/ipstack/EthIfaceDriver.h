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

#ifndef APRINTER_IPSTACK_ETH_IFACE_DRIVER_H
#define APRINTER_IPSTACK_ETH_IFACE_DRIVER_H

#include <stddef.h>

#include <aprinter/ipstack/Buf.h>
#include <aprinter/ipstack/Err.h>
#include <aprinter/ipstack/proto/EthernetProto.h>

#include <aprinter/BeginNamespace.h>

class EthIfaceDriverCallback;

class EthIfaceDriver {
public:
    virtual void setCallback (EthIfaceDriverCallback *callback) = 0;
    virtual MacAddr const * getMacAddr () = 0;
    virtual size_t getEthMtu () = 0;
    virtual IpErr sendFrame (IpBufRef frame) = 0;
};

class EthIfaceDriverCallback {
public:
    virtual void recvFrame (IpBufRef frame) = 0;
};

#include <aprinter/EndNamespace.h>

#endif
