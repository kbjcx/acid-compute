/**
 * @file rpc_server.h
 * @author kbjcx (lulu5v@163.com)
 * @brief
 * @version 0.1
 * @date 2023-07-23
 *
 * @copyright Copyright (c) 2023
 *
 */

#ifndef ACID_RPC_SERVER_H
#define ACID_RPC_SERVER_H

#include "acid/common/channel.h"
#include "acid/common/co_mutex.h"
#include "acid/common/iomanager.h"
#include "acid/common/traits.h"
#include "acid/logger/logger.h"
#include "acid/net/address.h"
#include "acid/net/socket.h"
#include "acid/net/socket_stream.h"
#include "acid/net/tcp_server.h"
#include "acid/rpc/serializer.h"
#include "protocol.h"
#include "rpc.h"
#include "rpc_session.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace acid::rpc {

class RpcServer : public TcpServer {
public:
    using ptr = std::shared_ptr<RpcServer>;
    using MutexType = CoMutex;
    using LockGuard = MutexType::Lock;

    RpcServer(IOManager* worker = IOManager::get_this(),
              IOManager* io_worker = IOManager::get_this(),
              IOManager* accept_worker = IOManager::get_this());

    ~RpcServer();

    bool bind(Address::ptr addr, bool ssl = false) override;

    bool bind_registry(Address::ptr address);

    bool start() override;

    /**
     * @brief 注册函数
     *
     * @tparam Func
     * @param name 注册的函数名
     * @param func 注册的函数
     */
    template <class Func>
    void register_method(const std::string& name, Func func);

    void set_name(std::string& name) override;

    /**
     * @brief 发布消息
     *
     * @tparam T
     * @param name 发布的消息名
     * @param data 支持Serializer的数据都可以发布
     */
    template <class T>
    void publish(const std::string& name, T data);

protected:
    /**
     * @brief 向服务中心发起注册
     *
     * @param name 注册的函数名
     */
    void register_service(const std::string& name);

    /**
     * @brief 调用服务端注册的函数
     *
     * @param name 函数名
     * @param arg 参数
     * @return Serializer 调用结果的序列化
     */
    Serializer call(const std::string& name, const std::string& arg);

    /**
     * @brief 调用代理
     *
     * @tparam Func
     * @param func
     * @param serializer
     * @param arg
     */
    template <class Func>
    void proxy(Func func, Serializer serializer, const std::string& arg);

    /**
     * @brief 更新心跳定时器
     *
     * @param heart_timer
     * @param client
     */
    void update(Timer::ptr heart_timer, Socket::ptr client);

    /**
     * @brief 处理客户端连接
     *
     * @param client
     */
    void handle_client(Socket::ptr client) override;

    /**
     * @brief 处理客户端过程调用请求
     *
     * @param proto
     * @return Protocol::ptr
     */
    Protocol::ptr handle_method_call(Protocol::ptr proto);

    /**
     * @brief 处理心跳包
     *
     * @param proto
     * @return Protocol::ptr
     */
    Protocol::ptr handle_heartbeat_packet(Protocol::ptr proto);

    /**
     * @brief 处理订阅请求
     *
     * @param proto
     * @param client
     * @return Protocol::ptr
     */
    Protocol::ptr handle_subscribe(Protocol::ptr proto, RpcSession::ptr client);

private:
    // 保存服务端注册的函数
    std::map<std::string, std::function<void(Serializer, const std::string&)>> m_handlers;
    // 服务中心连接
    RpcSession::ptr m_registry;
    // 心跳定时器
    Timer::ptr m_heart_timer;
    // 开放服务的端口
    uint32_t m_port;
    // 心跳时间
    uint64_t m_alive_time;
    // 订阅的客户端
    std::unordered_map<std::string, std::weak_ptr<RpcSession>> m_subscribes;
    MutexType m_sub_mutex;
    // 停止清理订阅的协程
    bool m_stop_clean = false;
    // 等待清理协程停止
    Channel<bool> m_clean_channel {1};
};

};  // namespace acid::rpc


#endif