/**
 * @file rpc_session.h
 * @author kbjcx (lulu5v@163.com)
 * @brief
 * @version 0.1
 * @date 2023-07-20
 *
 * @copyright Copyright (c) 2023
 *
 */

#ifndef ACID_RPC_SESSION_H
#define ACID_RPC_SESSION_H

#include "acid/common/co_mutex.h"
#include "acid/net/socket_stream.h"
#include "protocol.h"

namespace acid::rpc {

class RpcSession : public SocketStream {
public:
    using ptr = std::shared_ptr<RpcSession>;
    using MutexType = CoMutex;

    RpcSession(Socket::ptr socket, bool owner = true);

    Protocol::ptr recv_protocol();

    ssize_t send_protocol(Protocol::ptr protocol);

private:
    MutexType m_mutex;
};

};  // namespace acid::rpc

#endif