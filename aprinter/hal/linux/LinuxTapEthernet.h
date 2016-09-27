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

#ifndef APRINTER_LINUX_TAP_ETHERNET_H
#define APRINTER_LINUX_TAP_ETHERNET_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <linux/if_tun.h>

#include <aprinter/meta/WrapFunction.h>
#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/Callback.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Preprocessor.h>

#include <aprinter/BeginNamespace.h>

template <typename Arg>
class LinuxTapEthernet {
    APRINTER_USE_TYPE1(Arg, Context)
    APRINTER_USE_TYPE1(Arg, ParentObject)
    APRINTER_USE_TYPE1(Arg, ClientParams)
    
    APRINTER_USE_TYPE1(ClientParams, SendBufferType)
    APRINTER_USE_TYPE1(ClientParams, ReceiveHandler)
    
public:
    struct Object;
    
public:
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        o->fd_event.init(c, APRINTER_CB_STATFUNC_T(&LinuxTapEthernet::fd_event_handler));
        o->activated = false;
        memset(o->mac_addr, 0, 6);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        
        reset(c);
        o->fd_event.deinit(c);
    }
    
    static void reset (Context c)
    {
        auto *o = Object::self(c);
        
        if (o->activated) {
            o->fd_event.reset(c);
            if (o->tap_fd >= 0) {
                ::close(o->tap_fd);
            }
            ::free(o->write_buffer);
            ::free(o->read_buffer);
            o->activated = false;
            memset(o->mac_addr, 0, 6);
        }
    }
    
    static void activate (Context c, uint8_t const *ignored_mac_addr)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(!o->activated)
        
        o->activated = true;
        o->read_buffer = nullptr;
        o->write_buffer = nullptr;
        
        int sock = -1;
        
        do {
            o->tap_fd = ::open("/dev/net/tun", O_RDWR);
            if (o->tap_fd < 0) {
                fprintf(stderr, "ERROR: Failed to open() /dev/net/tun.\n");
                goto error:
            }
            
            struct ifreq ifr;
            
            memset(&ifr, 0, sizeof(ifr));
            ifr.ifr_flags |= IFF_NO_PI|IFF_TAP;
            if (cmdline_options.tap_dev != nullptr) {
                snprintf(ifr.ifr_name, IFNAMSIZ, "%s", cmdline_options.tap_dev);
            }
            
            if (::ioctl(o->tap_fd, TUNSETIFF, (void *)&ifr) < 0) {
                fprintf(stderr, "ERROR: ioctl(TUNSETIFF) failed.\n");
                goto error;
            }
            
            char devname_real[IFNAMSIZ];
            strcpy(devname_real, ifr.ifr_name);
            
            sock = socket(AF_INET, SOCK_DGRAM, 0);
            if (sock < 0) {
                fprintf(stderr, "ERROR: socket(AF_INET, SOCK_DGRAM) failed.\n");
                goto error;
            }
            
            memset(&ifr, 0, sizeof(ifr));
            strcpy(ifr.ifr_name, devname_real);
            
            if (::ioctl(sock, SIOCGIFMTU, (void *)&ifr) < 0) {
                fprintf(stderr, "ERROR: ioctl(SIOCGIFMTU) failed.\n");
                goto error;
            }
            
            o->eth_mtu = ifr.ifr_mtu + (size_t)14;
            
            if (o->eth_mtu < 128) {
                fprintf(stderr, "ERROR: MTU is extremely small.\n");
                goto error;
            }
            
            memset(&ifr, 0, sizeof(ifr));
            strcpy(ifr.ifr_name, devname_real);
            
            if (::ioctl(sock, SIOCGIFHWADDR, (void *)&ifr) < 0) {
                fprintf(stderr, "ERROR: ioctl(SIOCGIFHWADDR) failed.\n");
                goto error;
            }
            
            memcpy(o->mac_addr, ifr.ifr_hwaddr.sa_data, 6);
            
            ::close(sock);
        } while (false);
        
        o->read_buffer = ::malloc(o->eth_mtu);
        AMBRO_ASSERT_FORCE(o->read_buffer != nullptr)
        
        o->write_buffer = ::malloc(o->eth_mtu);
        AMBRO_ASSERT_FORCE(o->write_buffer != nullptr)
        
        Context::EventLoop::setFdNonblocking(o->tap_fd);
        
        o->fd_event.start(c, o->tap_fd, FdEvFlags::EV_READ);
        
        o->working = true;
        return;
        
    error:
        if (sock >= 0) {
            ::close(sock);
        }
        if (o->tap_fd >= 0) {
            ::close(o->tap_fd);
            o->tap_fd = -1;
        }
        o->working = false;
    }
    
    static bool sendFrame (Context c, SendBufferType *send_buffer)
    {
        auto *o = Object::self(c);
        
        if (!o->activated || !o->working) {
            return false;
        }
        
        size_t len = send_buffer->tot_len;
        if (len > o->eth_mtu) {
            return false;
        }
        
        send_buffer->takeBytes(len, o->write_buffer);
        
        ssize_t write_res = ::write(o->tap_fd, o->write_buffer, len);
        if (write_res < 0 || write_res != len) {
            return false;
        }
        
        return true;
    }
    
    static bool getLinkUp (Context c)
    {
        auto *o = Object::self(c);
        
        return o->activated && o->working;
    }
    
    static uint8_t const * getMacAddr (Context c)
    {
        auto *o = Object::self(c);
        
        return o->mac_addr;
    }
    
private:
    static void fd_event_handler (Context c, int events)
    {
        auto *o = Object::self(c);
        AMBRO_ASSERT(o->activated)
        AMBRO_ASSERT(o->working)
        
        ssize_t read_res = ::read(o->tap_fd, o->read_buffer, o->eth_mtu);
        if (read_res <= 0) {
            bool is_error = false;
            if (read_res < 0) {
                int err = errno;
                is_error = !(err == EAGAIN || err == EWOULDBLOCK);
            }
            if (is_error) {
                fprintf(stderr, "ERROR: read() TAP failed, stopping TAP operation.\n");
                o->working = false;
                o->fd_event.reset(c);
            }
            return;
        }
        
        AMBRO_ASSERT(read_res <= o->eth_mtu)
        size_t frame_size = read_res;
        
        ReceiveHandler::call(c, o->read_buffer, nullptr, frame_size, 0);
    }
    
private:
    struct Object : public ObjBase<LinuxTapEthernet, ParentObject, EmptyTypeList> {
        typename Context::EventLoop::FdEvent fd_event;
        size_t eth_mtu;
        char *read_buffer;
        char *write_buffer;
        int tap_fd;
        bool activated;
        bool working;
        uint8_t mac_addr[6];
    };
};

struct LinuxTapEthernetService {
    APRINTER_ALIAS_STRUCT_EXT(Ethernet, (
        APRINTER_AS_TYPE(Context),
        APRINTER_AS_TYPE(ParentObject),
        APRINTER_AS_TYPE(ClientParams)
    ), (
        APRINTER_DEF_INSTANCE(Ethernet, LinuxTapEthernet)
    ))
};

#include <aprinter/EndNamespace.h>

#endif
