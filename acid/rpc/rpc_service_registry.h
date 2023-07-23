/**
 * @file rpc_service_registry.h
 * @author kbjcx (lulu5v@163.com)
 * @brief
 * @version 0.1
 * @date 2023-07-23
 *
 * @copyright Copyright (c) 2023
 *
 */

#ifndef ACID_RPC_SERVICE_REGISTRY_H
#define ACID_RPC_SERVICE_REGISTRY_H

#include "acid/common/channel.h"
#include "acid/common/co_mutex.h"
#include "acid/common/iomanager.h"
#include "acid/common/timer.h"
#include "acid/net/address.h"
#include "acid/net/socket.h"
#include "acid/net/socket_stream.h"
#include "acid/net/tcp_server.h"
#include "protocol.h"
#include "rpc_session.h"
#include "serializer.h"

#include <cstdint>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace acid::rpc {

/**
 * @brief RPC服务注册中心
 * @details 接收客户端服务发现请求。接收服务端服务注册请求，断开连接后移除服务。
 * 以上统称为服务注册中心的用户
 */
class RpcServiceRegistry : public TcpServer {
public:
    using ptr = std::shared_ptr<RpcServiceRegistry>;
    using MutexType = CoMutex;
    using LockGuard = MutexType::Lock;

    RpcServiceRegistry(IOManager* worker = IOManager::get_this(),
                       IOManager* io_worker = IOManager::get_this(),
                       IOManager* accept_worker = IOManager::get_this());
    ~RpcServiceRegistry();

    // 设置Rpc服务中心的名字
    void set_name(std::string& name) override {
        TcpServer::set_name(name);
    }

    /**
     * @brief 发布消息
     *
     * @tparam T Serializer支持的类型
     * @param key 发布的key
     * @param data 发布的数据
     */
    template <class T>
    void publish(const std::string& key, T data) {
        {
            LockGuard lock(m_sub_mutex);
            if (m_subscribes.empty()) {
                return;
            }
        }

        Serializer s;
        s << key << data;
        s.reset();
        Protocol::ptr sub =
            Protocol::create(Protocol::MessageType::RPC_PUBLISH_REQUEST, s.to_string(), 0);
        LockGuard lock(m_mutex);
        auto range = m_subscribes.equal_range(key);
        for (auto it = range.first; it != range.second; ++it) {
            auto connection = it->second.lock();
            if (connection == nullptr || !connection->is_connected()) {
                continue;
            }
            connection->send_protocol(sub);
        }
    }

protected:
    /**
     * @brief 更新心跳定时器
     *
     * @param heart_timer 心跳定时器
     * @param client 客户端连接
     */
    void update(Timer::ptr heart_timer, Socket::ptr client);

    /**
     * @brief 处理客户端请求
     *
     * @param client
     */
    void handle_client(Socket::ptr client) override;

    /**
     * @brief 为服务端提供服务注册, 将服务地址注册到对应的服务名下面, 断开连接后自动清除
     *
     * @param protocol
     * @param address
     * @return Protocol::ptr
     */
    Protocol::ptr handle_register_service(Protocol::ptr protocol, Address::ptr address);

    /**
     * @brief 移除注册服务
     *
     * @param address 服务注册的地址
     */
    void handle_unregidter_service(Address::ptr address);

    /**
     * @brief 为客户端提供服务发现
     *
     * @param protocol 服务名称
     * @return Protocol::ptr 服务地址列表
     */
    Protocol::ptr handle_discover_service(Protocol::ptr proto);

    /**
     * @brief 处理provider初次连接时的事件, 获取开放服务的端口
     *
     * @param protocol
     * @param sock
     * @return Address::ptr 服务地址
     */
    Address::ptr hanlde_provider(Protocol::ptr protocol, Socket::ptr sock);

    /**
     * @brief 处理心跳包
     *
     * @param protocol
     * @return Protocol::ptr
     */
    Protocol::ptr handle_heartbeat_packet(Protocol::ptr protocol);

    /**
     * @brief 处理订阅请求
     *
     * @param protocol
     * @param client
     * @return Protocol::ptr
     */
    Protocol::ptr handle_subscribe(Protocol::ptr protocol, RpcSession::ptr client);

private:
    /**
     * 维护服务名和服务地址列表的多重映射
     * serviceName -> serviceAddress1
     *             -> serviceAddress2
     *             ...
     */
    std::multimap<std::string, std::string> m_services;
    // 维护服务地址到迭代器的映射
    std::map<std::string, std::vector<std::multimap<std::string, std::string>::iterator>> m_iters;
    MutexType m_mutex;
    // 允许心跳超时的时间
    uint64_t m_alive_time;
    // 订阅的客户端
    std::unordered_map<std::string, std::weak_ptr<RpcSession>> m_subscribes;
    // 保护m_subscribes
    MutexType m_sub_mutex;
    // 停止清理订阅的协程
    bool m_stop_clean = false;
    // 等待清理协程停止
    Channel<bool> m_clean_channel {1};
};

};  // namespace acid::rpc

#endif