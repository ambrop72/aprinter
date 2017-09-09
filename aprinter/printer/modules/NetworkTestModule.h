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

#ifndef APRINTER_NETWORK_TEST_MODULE_H
#define APRINTER_NETWORK_TEST_MODULE_H

#include <stddef.h>
#include <stdint.h>

#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/meta/MinMax.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/Preprocessor.h>
#include <aprinter/base/ProgramMemory.h>
#include <aprinter/misc/IpAddrUtils.h>
#include <aprinter/printer/utils/ModuleUtils.h>

#include <aipstack/common/Buf.h>
#include <aipstack/common/Struct.h>
#include <aipstack/proto/IpAddr.h>

namespace APrinter {

template <typename ModuleArg>
class NetworkTestModule {
    APRINTER_UNPACK_MODULE_ARG(ModuleArg)
    APRINTER_USE_VALS(Params, (BufferSize))
    APRINTER_USE_TYPES2(AIpStack, (Ip4Addr, IpErr, IpBufNode, IpBufRef))
    
    using Network = typename Context::Network;
    APRINTER_USE_TYPES1(Network, (TcpProto))
    APRINTER_USE_TYPES1(TcpProto, (TcpConnection, SeqType))
    
public:
    struct Object;
    
public:
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        o->connection.init(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        
        o->connection.deinit(c);
    }
    
    static bool check_command (Context c, typename ThePrinterMain::TheCommand *cmd)
    {
        if (cmd->getCmdNumber(c) == 945) {
            handle_connect_command(c, cmd);
            return false;
        }
        if (cmd->getCmdNumber(c) == 946) {
            handle_abort_command(c, cmd);
            return false;
        }
        return true;
    }
    
private:
    static void handle_connect_command (Context c, typename ThePrinterMain::TheCommand *cmd)
    {
        auto *o = Object::self(c);
        
        {
            char const *ipaddr_str = cmd->get_command_param_str(c, 'A', "");
            uint32_t port = cmd->get_command_param_uint32(c, 'P', 0);
            
            char ipaddr_bytes[4];
            if (!IpAddrUtils::ParseIp4Addr(ipaddr_str, ipaddr_bytes) ||
                !(port > 0 && port <= 65535))
            {
                cmd->reportError(c, AMBRO_PSTR("BadParams"));
                goto end;
            }
            
            Ip4Addr addr = AIpStack::ReadSingleField<Ip4Addr>(ipaddr_bytes);
            
            if (!o->connection.start_connection(c, addr, port, cmd)) {
                goto end;
            }
            
            cmd->reply_append_pstr(c, AMBRO_PSTR("TestConnectionStarted\n"));
        }
    end:
        cmd->finishCommand(c);
    }
    
    static void handle_abort_command (Context c, typename ThePrinterMain::TheCommand *cmd)
    {
        auto *o = Object::self(c);
        
        if (o->connection.abort_connection(c)) {
            cmd->reply_append_pstr(c, AMBRO_PSTR("ConnectionAborted\n"));
        } else {
            cmd->reply_append_pstr(c, AMBRO_PSTR("ConnectionNotActive\n"));
        }
        
        cmd->finishCommand(c);
    }
    
    struct Connection :
        private TcpConnection
    {
        void init (Context c)
        {
        }
        
        void deinit (Context c)
        {
            TcpConnection::reset();
        }
        
        bool start_connection (Context c, Ip4Addr addr, uint16_t port, typename ThePrinterMain::TheCommand *cmd)
        {
            if (!TcpConnection::isInit()) {
                cmd->reportError(c, AMBRO_PSTR("ConnectionAlreadyActive"));
                return false;
            }
            
            IpErr res = TcpConnection::startConnection(Network::getTcpProto(c), addr, port, BufferSize);
            if (res != IpErr::SUCCESS) {
                cmd->reportError(c, AMBRO_PSTR("FailedToStartConnection"));
                return false;
            }
            
            m_buf_node = IpBufNode{m_buffer, BufferSize, &m_buf_node};
            
            SeqType max_rx_window = MinValueU(BufferSize, TcpProto::MaxRcvWnd);
            SeqType thres = MaxValue((SeqType)1, max_rx_window / Network::TcpWndUpdThrDiv);
            TcpConnection::setWindowUpdateThreshold(thres);
            
            TcpConnection::setSendBuf(IpBufRef{&m_buf_node, 0, 0});
            TcpConnection::setRecvBuf(IpBufRef{&m_buf_node, 0, BufferSize});
            
            return true;
        }
        
        bool abort_connection (Context c)
        {
            if (TcpConnection::isInit()) {
                return false;
            }
            TcpConnection::reset();
            return true;
        }
        
        void connectionAborted () override final
        {
            ThePrinterMain::print_pgm_string(Context(), AMBRO_PSTR("//TestConnectionAborted\n"));
            
            TcpConnection::reset();
        }
        
        void connectionEstablished () override final
        {
            ThePrinterMain::print_pgm_string(Context(), AMBRO_PSTR("//TestConnectionEstablished\n"));
        }
        
        void dataReceived (size_t amount) override final
        {
            if (amount > 0) {
                TcpConnection::extendSendBuf(amount);
                TcpConnection::sendPush();
            } else {
                TcpConnection::closeSending();
            }
        }
        
        void dataSent (size_t amount) override final
        {
            TcpConnection::extendRecvBuf(amount);
        }
        
        IpBufNode m_buf_node;
        char m_buffer[BufferSize];
    };
    
public:
    struct Object : public ObjBase<NetworkTestModule, ParentObject, EmptyTypeList> {
        Connection connection;
    };
};

APRINTER_ALIAS_STRUCT_EXT(NetworkTestModuleService, (
    APRINTER_AS_VALUE(size_t, BufferSize)
), (
    APRINTER_MODULE_TEMPLATE(NetworkTestModuleService, NetworkTestModule)
))

}

#endif
