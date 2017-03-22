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

#ifndef APRINTER_IPSTACK_IP_DHCP_CLIENT_OPTIONS_H
#define APRINTER_IPSTACK_IP_DHCP_CLIENT_OPTIONS_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <aprinter/meta/MinMax.h>
#include <aprinter/base/LoopUtils.h>
#include <aprinter/base/MemRef.h>
#include <aprinter/base/OneOf.h>
#include <aprinter/base/Assert.h>

#include <aipstack/misc/Buf.h>
#include <aipstack/misc/Struct.h>
#include <aipstack/proto/IpAddr.h>
#include <aipstack/proto/DhcpProto.h>

#include <aipstack/BeginNamespace.h>

template <
    uint8_t MaxDnsServers,
    uint8_t MaxClientIdSize,
    uint8_t MaxVendorClassIdSize
>
class IpDhcpClient_options
{
    APRINTER_USE_ONEOF
    
    // Calculates option size for given option data size.
    static constexpr size_t OptSize (size_t data_size)
    {
        return DhcpOptionHeader::Size + data_size;
    }
    
    // Calculates the size of a DHCP option.
    // OptDataType is the payload type declared with APRINTER_TSTRUCT.
    template <typename OptDataType>
    static constexpr size_t OptSize ()
    {
        return OptSize(OptDataType::Size);
    }
    
    
    // Possible regions where options can be located from.
    enum class OptionRegion {Options, File, Sname};
    
public:
    // Maximum packet size that we could possibly transmit.
    static size_t const MaxOptionsSendSize =
        // DHCP message type
        OptSize<DhcpOptMsgType>() +
        // requested IP address
        OptSize<DhcpOptAddr>() +
        // DHCP server identifier
        OptSize<DhcpOptServerId>() +
        // maximum message size
        OptSize<DhcpOptMaxMsgSize>() +
        // parameter request list
        OptSize(4) +
        // client identifier
        OptSize(MaxClientIdSize) +
        // vendor class identifier
        OptSize(MaxVendorClassIdSize) +
        // end
        1;
    
    // Structure representing received DHCP options.
    struct DhcpRecvOptions {
        // Which options have been received.
        struct Have {
            bool dhcp_message_type : 1;
            bool dhcp_server_identifier : 1;
            bool ip_address_lease_time : 1;
            bool subnet_mask : 1;
            bool router : 1;
            uint8_t dns_servers;
        } have;
        
        // The option values (only options set in Have are initialized).
        DhcpMessageType dhcp_message_type;
        uint32_t dhcp_server_identifier;
        uint32_t ip_address_lease_time;
        Ip4Addr subnet_mask;
        Ip4Addr router;
        Ip4Addr dns_servers[MaxDnsServers];
    };

    // Structure representing DHCP options to be sent.
    struct DhcpSendOptions {
        // Default constructor clears the Have fields.
        inline DhcpSendOptions ()
        : have(Have{})
        {}
        
        // Which options to send.
        struct Have {
            bool requested_ip_address : 1;
            bool dhcp_server_identifier : 1;
            bool client_identifier : 1;
            bool vendor_class_identifier : 1;
        } have;
        
        // The option values (only options set in Have are relevant).
        Ip4Addr requested_ip_address;
        uint32_t dhcp_server_identifier;
        APrinter::MemRef client_identifier;
        APrinter::MemRef vendor_class_identifier;
    };
    
    // Parse DHCP options from a buffer into DhcpRecvOptions.
    static bool parseOptions (IpBufRef dhcp_header2, IpBufRef data, DhcpRecvOptions &opts)
    {
        AMBRO_ASSERT(dhcp_header2.tot_len == DhcpHeader2::Size)
        
        // Clear all the "have" fields.
        opts.have = typename DhcpRecvOptions::Have{};
        
        // The region of options we are currently parsing.
        // We start by parsing the "options" region which the data
        // argument initially refers to.
        OptionRegion region = OptionRegion::Options;
        
        // The option overload mode (defined by the OptionOverload option).
        DhcpOptionOverload option_overload = DhcpOptionOverload::None;
        
        // This loop is for parsing different regions of options.
        while (true) {
            // Whether we have seen the end option (in this region).
            bool have_end = false;
            
            while (data.tot_len > 0) {
                // Read option type.
                DhcpOptionType opt_type = DhcpOptionType(data.takeByte());
                
                // Pad option?
                if (opt_type == DhcpOptionType::Pad) {
                    continue;
                }
                
                // It is an error for options other than pad to follow
                // the end option.
                if (have_end) {
                    return false;
                }
                
                // End option?
                if (opt_type == DhcpOptionType::End) {
                    // Skip over any following pad options.
                    have_end = true;
                    continue;
                }
                
                // Read option length.
                if (data.tot_len == 0) {
                    return false;
                }
                uint8_t opt_len = data.takeByte();
                
                // Check option length.
                if (opt_len > data.tot_len) {
                    return false;
                }
                
                // Handle different options.
                switch (opt_type) {
                    case DhcpOptionType::DhcpMessageType: {
                        if (opt_len != DhcpOptMsgType::Size) {
                            goto skip_data;
                        }
                        DhcpOptMsgType::Val val;
                        data.takeBytes(opt_len, val.data);
                        opts.have.dhcp_message_type = true;
                        opts.dhcp_message_type = val.get(DhcpOptMsgType::MsgType());
                    } break;
                    
                    case DhcpOptionType::DhcpServerIdentifier: {
                        if (opt_len != DhcpOptServerId::Size) {
                            goto skip_data;
                        }
                        DhcpOptServerId::Val val;
                        data.takeBytes(opt_len, val.data);
                        opts.have.dhcp_server_identifier = true;
                        opts.dhcp_server_identifier = val.get(DhcpOptServerId::ServerId());
                    } break;
                    
                    case DhcpOptionType::IpAddressLeaseTime: {
                        if (opt_len != DhcpOptTime::Size) {
                            goto skip_data;
                        }
                        DhcpOptTime::Val val;
                        data.takeBytes(opt_len, val.data);
                        opts.have.ip_address_lease_time = true;
                        opts.ip_address_lease_time = val.get(DhcpOptTime::Time());
                    } break;
                    
                    case DhcpOptionType::SubnetMask: {
                        if (opt_len != DhcpOptAddr::Size) {
                            goto skip_data;
                        }
                        DhcpOptAddr::Val val;
                        data.takeBytes(opt_len, val.data);
                        opts.have.subnet_mask = true;
                        opts.subnet_mask = val.get(DhcpOptAddr::Addr());
                    } break;
                    
                    case DhcpOptionType::Router: {
                        // We only care about the first router.
                        if (opt_len % DhcpOptAddr::Size != 0 || opt_len == 0 || opts.have.router) {
                            goto skip_data;
                        }
                        DhcpOptAddr::Val val;
                        data.takeBytes(DhcpOptAddr::Size, val.data);
                        opts.have.router = true;
                        opts.router = val.get(DhcpOptAddr::Addr());
                        data.skipBytes(opt_len - DhcpOptAddr::Size);
                    } break;
                    
                    case DhcpOptionType::DomainNameServer: {
                        if (opt_len % DhcpOptAddr::Size != 0) {
                            goto skip_data;
                        }
                        uint8_t num_servers = opt_len / DhcpOptAddr::Size;
                        for (auto server_index : APrinter::LoopRangeAuto(num_servers)) {
                            // Must consume all servers from data even if we can't save them.
                            DhcpOptAddr::Val val;
                            data.takeBytes(DhcpOptAddr::Size, val.data);
                            if (opts.have.dns_servers < MaxDnsServers) {
                                opts.dns_servers[opts.have.dns_servers++] = val.get(DhcpOptAddr::Addr());
                            }
                        }
                    } break;
                    
                    case DhcpOptionType::OptionOverload: {
                        // Ignore if it appears in the file or sname region.
                        if (opt_len != DhcpOptOptionOverload::Size || region != OptionRegion::Options) {
                            goto skip_data;
                        }
                        DhcpOptOptionOverload::Val val;
                        data.takeBytes(opt_len, val.data);
                        DhcpOptionOverload overload_val = val.get(DhcpOptOptionOverload::Overload());
                        if (overload_val == OneOf(DhcpOptionOverload::FileOptions,
                            DhcpOptionOverload::SnameOptions, DhcpOptionOverload::FileSnameOptions))
                        {
                            option_overload = overload_val;
                        }
                    } break;
                    
                    // Unknown or bad option, consume the option data.
                    skip_data:
                    default: {
                        data.skipBytes(opt_len);
                    } break;
                }
            }
            
            // Check that the end option was found.
            if (!have_end) {
                return false;
            }
            
            // Check if we need to continue parsing options from another region.
            if (region == OptionRegion::Options &&
                option_overload == OneOf(DhcpOptionOverload::FileOptions, DhcpOptionOverload::FileSnameOptions))
            {
                // Parse options in file.
                region = OptionRegion::File;
                data = dhcp_header2.subFromTo(64, 128);
            }
            else if (
                (region == OptionRegion::Options && option_overload == DhcpOptionOverload::SnameOptions) ||
                (region == OptionRegion::File && option_overload == DhcpOptionOverload::FileSnameOptions)
            ) {
                // Parse options in sname.
                region = OptionRegion::Sname;
                data = dhcp_header2.subFromTo(0, 64);
            }
            else {
                // Done parsing options.
                break;
            }
        }
        
        return true;
    }
    
    // Write DHCP options to a buffer, return the end pointer.
    static char * writeOptions (
        char *opt_writeptr, DhcpMessageType msg_type, uint16_t max_msg_size,
        DhcpSendOptions const &opts)
    {
        // The opt_writeptr will be incremented as options are written.
        // NOTE: If adding new options, adjust the MaxDhcpSendMsgSize definition.
        
        // Helper function for writing options.
        auto write_option = [&](DhcpOptionType opt_type, auto payload_func) {
            auto oh = DhcpOptionHeader::Ref(opt_writeptr);
            oh.set(DhcpOptionHeader::OptType(), opt_type);
            size_t opt_len = payload_func(opt_writeptr + DhcpOptionHeader::Size);
            oh.set(DhcpOptionHeader::OptLen(), opt_len);
            opt_writeptr += DhcpOptionHeader::Size + opt_len;
        };
        
        // DHCP message type
        write_option(DhcpOptionType::DhcpMessageType, [&](char *opt_data) {
            auto opt = DhcpOptMsgType::Ref(opt_data);
            opt.set(DhcpOptMsgType::MsgType(), msg_type);
            return opt.Size();
        });
        
        // Requested IP address
        if (opts.have.requested_ip_address) {
            write_option(DhcpOptionType::RequestedIpAddress, [&](char *opt_data) {
                auto opt = DhcpOptAddr::Ref(opt_data);
                opt.set(DhcpOptAddr::Addr(), opts.requested_ip_address);
                return opt.Size();
            });
        }
        
        // DHCP server identifier
        if (opts.have.dhcp_server_identifier) {
            write_option(DhcpOptionType::DhcpServerIdentifier, [&](char *opt_data) {
                auto opt = DhcpOptServerId::Ref(opt_data);
                opt.set(DhcpOptServerId::ServerId(), opts.dhcp_server_identifier);
                return opt.Size();
            });
        }
        
        // Maximum message size
        write_option(DhcpOptionType::MaximumMessageSize, [&](char *opt_data) {
            auto opt = DhcpOptMaxMsgSize::Ref(opt_data);
            opt.set(DhcpOptMaxMsgSize::MaxMsgSize(), max_msg_size);
            return opt.Size();
        });
        
        // Parameter request list
        write_option(DhcpOptionType::ParameterRequestList, [&](char *opt_data) {
            DhcpOptionType opt[] = {
                DhcpOptionType::SubnetMask,
                DhcpOptionType::Router,
                DhcpOptionType::DomainNameServer,
                DhcpOptionType::IpAddressLeaseTime,
            };
            ::memcpy(opt_data, opt, sizeof(opt));
            return sizeof(opt);
        });
        
        // Client identifier
        if (opts.have.client_identifier) {
            uint8_t eff_len = APrinter::MinValueU(MaxClientIdSize, opts.client_identifier.len);
            write_option(DhcpOptionType::ClientIdentifier, [&](char *opt_data) {
                ::memcpy(opt_data, opts.client_identifier.ptr, eff_len);
                return eff_len;
            });
        }
        
        // Vendor class identifier
        if (opts.have.vendor_class_identifier) {
            uint8_t eff_len = APrinter::MinValueU(MaxVendorClassIdSize, opts.vendor_class_identifier.len);
            write_option(DhcpOptionType::VendorClassIdentifier, [&](char *opt_data) {
                ::memcpy(opt_data, opts.vendor_class_identifier.ptr, eff_len);
                return eff_len;
            });
        }
        
        // end option
        WriteSingleField<DhcpOptionType>(opt_writeptr++, DhcpOptionType::End);
        
        return opt_writeptr;
    }
};

#include <aipstack/EndNamespace.h>

#endif
