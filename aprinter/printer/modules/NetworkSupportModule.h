/*
 * Copyright (c) 2015 Ambroz Bizjak
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

#ifndef APRINTER_NETWORK_SUPPORT_MODULE_H
#define APRINTER_NETWORK_SUPPORT_MODULE_H

#include <string.h>
#include <stdio.h>
#include <inttypes.h>

#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/meta/MemberType.h>
#include <aprinter/meta/FunctionIf.h>
#include <aprinter/meta/FuncUtils.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/ProgramMemory.h>
#include <aprinter/base/Callback.h>
#include <aprinter/base/LoopUtils.h>
#include <aprinter/printer/Configuration.h>
#include <aprinter/printer/utils/ModuleUtils.h>

#include <aipstack/ip/IpDhcpClient.h>

namespace APrinter {

template <typename ModuleArg>
class NetworkSupportModule {
    APRINTER_UNPACK_MODULE_ARG(ModuleArg)
    
    APRINTER_DEFINE_MEMBER_TYPE(MemberHasSimulatedLinkStatus, HasSimulatedLinkStatus)
    
public:
    struct Object;
    
private:
    using Config = typename ThePrinterMain::Config;
    using TheCommand = typename ThePrinterMain::TheCommand;
    using TheNetwork = typename Context::Network;
    
    static bool const HasSimulatedLinkStatus =
        FuncCall<typename MemberHasSimulatedLinkStatus::Has, typename TheNetwork::GetEthernet>::Value;
    
    using CNetEnabled = decltype(Config::e(Params::NetEnabled::i()));
    using CMacAddress = decltype(Config::e(Params::MacAddress::i()));
    using CDhcpEnabled = decltype(Config::e(Params::DhcpEnabled::i()));
    using CIpAddress = decltype(Config::e(Params::IpAddress::i()));
    using CIpNetmask = decltype(Config::e(Params::IpNetmask::i()));
    using CIpGateway = decltype(Config::e(Params::IpGateway::i()));
    
public:
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        o->network_event_listener.init(c, APRINTER_CB_STATFUNC_T(&NetworkSupportModule::network_event_handler));
        o->network_event_listener.startListening(c);
        
        // If there is no configuration store then we will activate
        // the network here already, otherwise configuration_changed will
        // be called when the configuration is loaded from the store.
        if (!ThePrinterMain::TheConfigManager::HasStore) {
            configuration_changed(c);
        }
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        o->network_event_listener.deinit(c);
    }
    
    static void configuration_changed (Context c)
    {
        bool cfg_net_enabled               = APRINTER_CFG(Config, CNetEnabled, c);
        ConfigTypeMacAddress cfg_mac       = APRINTER_CFG(Config, CMacAddress, c);
        bool cfg_dhcp_enabled              = APRINTER_CFG(Config, CDhcpEnabled, c);
        ConfigTypeIpAddress cfg_ip_addr    = APRINTER_CFG(Config, CIpAddress, c);
        ConfigTypeIpAddress cfg_ip_netmask = APRINTER_CFG(Config, CIpNetmask, c);
        ConfigTypeIpAddress cfg_ip_gateway = APRINTER_CFG(Config, CIpGateway, c);
        
        if (TheNetwork::isActivated(c)) {
            auto config = TheNetwork::getConfig(c);
            
            bool match =
                cfg_net_enabled &&
                memcmp(cfg_mac.mac_addr, config.mac_addr, ConfigTypeMacAddress::Size) == 0 &&
                cfg_dhcp_enabled == config.dhcp_enabled &&
                (cfg_dhcp_enabled || (
                    memcmp(cfg_ip_addr.ip_addr,    config.ip_addr,    ConfigTypeIpAddress::Size) == 0 &&
                    memcmp(cfg_ip_netmask.ip_addr, config.ip_netmask, ConfigTypeIpAddress::Size) == 0 &&
                    memcmp(cfg_ip_gateway.ip_addr, config.ip_gateway, ConfigTypeIpAddress::Size) == 0
                ));
            
            if (!match || TheNetwork::getStatus(c).activation_state == TheNetwork::ACTIVATE_FAILED) {
                TheNetwork::deactivate(c);
            }
        }
        
        if (!TheNetwork::isActivated(c) && cfg_net_enabled) {
            auto params = typename TheNetwork::NetworkParams();
            memcpy(params.mac_addr, cfg_mac.mac_addr, ConfigTypeMacAddress::Size);
            params.dhcp_enabled = cfg_dhcp_enabled;
            if (!cfg_dhcp_enabled) {
                memcpy(params.ip_addr,    cfg_ip_addr.ip_addr,    ConfigTypeIpAddress::Size);
                memcpy(params.ip_netmask, cfg_ip_netmask.ip_addr, ConfigTypeIpAddress::Size);
                memcpy(params.ip_gateway, cfg_ip_gateway.ip_addr, ConfigTypeIpAddress::Size);
            }
            
            TheNetwork::activate(c, &params);
        }
    }
    
    static bool check_command (Context c, TheCommand *cmd)
    {
        if (cmd->getCmdNumber(c) == 940) {
            handle_status_command(c, cmd);
            return false;
        }
        return check_simulated_link_command(c, cmd);
    }
    
private:
    static void handle_status_command (Context c, TheCommand *cmd)
    {
        cmd->reply_append_pstr(c, AMBRO_PSTR("Network: "));
        
        auto status = TheNetwork::getStatus(c);
        
        if (status.activation_state == TheNetwork::NOT_ACTIVATED) {
            cmd->reply_append_pstr(c, AMBRO_PSTR("NotActivated"));
        }
        else if (status.activation_state == TheNetwork::ACTIVATING) {
            cmd->reply_append_pstr(c, AMBRO_PSTR("Activating"));
        }
        else if (status.activation_state == TheNetwork::ACTIVATE_FAILED) {
            cmd->reply_append_pstr(c, AMBRO_PSTR("ActivationFailed"));
        }
        else if (status.activation_state == TheNetwork::ACTIVATED) {
            cmd->reply_append_pstr(c, AMBRO_PSTR("MAC="));
            print_mac_addr(c, cmd, status.mac_addr);
            
            cmd->reply_append_pstr(c, AMBRO_PSTR(" Link="));
            cmd->reply_append_pstr(c, status.link_up ? AMBRO_PSTR("Up") : AMBRO_PSTR("Down"));
            
            cmd->reply_append_pstr(c, AMBRO_PSTR(" DHCP="));
            cmd->reply_append_pstr(c, status.dhcp_enabled ? AMBRO_PSTR("On") : AMBRO_PSTR("Off"));
            
            cmd->reply_append_pstr(c, AMBRO_PSTR(" Addr="));
            print_ip_addr(c, cmd, status.ip_addr);
            
            cmd->reply_append_pstr(c, AMBRO_PSTR(" Mask="));
            print_ip_addr(c, cmd, status.ip_netmask);
            
            cmd->reply_append_pstr(c, AMBRO_PSTR(" Gateway="));
            print_ip_addr(c, cmd, status.ip_gateway);
        }
        
        cmd->reply_append_ch(c, '\n');
        cmd->finishCommand(c);
    }
    
    static void print_mac_addr (Context c, TheCommand *cmd, uint8_t const *addr)
    {
        for (auto i : LoopRange<int>(6)) {
            if (i > 0) {
                cmd->reply_append_ch(c, ':');
            }
            char str[3];
            sprintf(str, "%02" PRIx8, addr[i]);
            cmd->reply_append_str(c, str);
        }
    }
    
    static void print_ip_addr (Context c, TheCommand *cmd, uint8_t const *addr)
    {
        for (auto i : LoopRange<int>(4)) {
            if (i > 0) {
                cmd->reply_append_ch(c, '.');
            }
            cmd->reply_append_uint32(c, addr[i]);
        }
    }
    
    static void network_event_handler (Context c, typename TheNetwork::NetworkEvent event)
    {
        auto *out = ThePrinterMain::get_msg_output(c);
        
        switch (event.type) {
            case TheNetwork::NetworkEventType::ACTIVATION: {
                out->reply_append_pstr(c, event.activation.error ? AMBRO_PSTR("//EthActivateErr\n") : AMBRO_PSTR("//EthActivateOk\n"));
                out->reply_poke(c);
            } break;
            
            case TheNetwork::NetworkEventType::LINK: {
                out->reply_append_pstr(c, event.link.up ? AMBRO_PSTR("//EthLinkUp\n") : AMBRO_PSTR("//EthLinkDown\n"));
                out->reply_poke(c);
            } break;
            
            case TheNetwork::NetworkEventType::DHCP: {
                bool known = true;
                AMBRO_PGM_P msg;
                
                switch (event.dhcp.event) {
                    case AIpStack::IpDhcpClientEvent::LeaseObtained:
                        msg = AMBRO_PSTR("//DhcpLeaseObtained\n");
                        break;
                    case AIpStack::IpDhcpClientEvent::LeaseRenewed:
                        msg = AMBRO_PSTR("//DhcpLeaseRenewed\n");
                        break;
                    case AIpStack::IpDhcpClientEvent::LeaseLost:
                        msg = AMBRO_PSTR("//DhcpLeaseLost\n");
                        break;
                    // ignore LinkDown since EthLinkDown is shown already
                    default:
                        known = false;
                        break;
                }
                
                if (known) {
                    out->reply_append_pstr(c, msg);
                    out->reply_poke(c);
                }
            } break;
        }
    }
    
    APRINTER_FUNCTION_IF_ELSE_EXT(HasSimulatedLinkStatus, static, bool, check_simulated_link_command(Context c, TheCommand *cmd), {
        if (cmd->getCmdNumber(c) == 941) {
            uint32_t link_up_arg;
            if (cmd->find_command_param_uint32(c, 'L', &link_up_arg)) {
                TheNetwork::GetEthernet::setSimulatedLinkUp(c, link_up_arg != 0);
            }
            cmd->finishCommand(c);
            return false;
        }
        return true;
    }, {
        return true;
    })
    
public:
    using ConfigExprs = MakeTypeList<CNetEnabled, CMacAddress, CDhcpEnabled, CIpAddress, CIpNetmask, CIpGateway>;
    
    struct Object : public ObjBase<NetworkSupportModule, ParentObject, EmptyTypeList> {
        typename TheNetwork::NetworkEventListener network_event_listener;
    };
};

APRINTER_ALIAS_STRUCT_EXT(NetworkSupportModuleService, (
    APRINTER_AS_TYPE(NetEnabled),
    APRINTER_AS_TYPE(MacAddress),
    APRINTER_AS_TYPE(DhcpEnabled),
    APRINTER_AS_TYPE(IpAddress),
    APRINTER_AS_TYPE(IpNetmask),
    APRINTER_AS_TYPE(IpGateway)
), (
    APRINTER_MODULE_TEMPLATE(NetworkSupportModuleService, NetworkSupportModule)
))

}

#endif
