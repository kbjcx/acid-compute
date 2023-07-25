/**
 * @file rpc_connection_pool.h
 * @author kbjcx (lulu5v@163.com)
 * @brief
 * @version 0.1
 * @date 2023-07-20
 *
 * @copyright Copyright (c) 2023
 *
 */

#ifndef ACID_RPC_CONNECTION_POOL_H
#define ACID_RPC_CONNECTION_POOL_H

#include "acid/common/co_mutex.h"
#include "acid/common/traits.h"
#include "route_strategy.h"
#include "rpc_client.h"
#include "rpc_session.h"
#include "serializer.h"

namespace acid::rpc {

class RpcConnectionPool : public std::enable_shared_from_this<RpcConnectionPool> {
public:
    using ptr = std::shared_ptr<RpcConnectionPool>;
    using MutexType = CoMutex;
    using LockGuard = MutexType::Lock;

    RpcConnectionPool(uint64_t timeout_ms = -1);
    ~RpcConnectionPool();

    /**
     * @brief 连接rpc服务中心
     *
     * @param address rpc服务中心地址
     * @return true
     * @return false
     */ 
    bool connect(Address::ptr address);

    /**
     * @brief 异步远程过程调用回调函数
     *
     * @tparam Params
     * @param name 函数名
     * @param params 可变参
     */
    template <class... Params>
    void callback(const std::string& name, Params&&... params) {
        static_assert(sizeof...(params), "without a callback function");
        auto tp = std::make_tuple(params...);
        constexpr auto size = std::tuple_size<typename std::decay<decltype(tp)>::type>::value;
        auto cb = std::get<size - 1>(tp);
        static_assert(function_traits<decltype(cb)> {}.arity == 1, "callback type not support");
        using res = typename function_traits<decltype(cb)>::template args<0>::type;
        using ret = typename res::raw_type;
        static_assert(std::invocable<decltype(cb), Result<ret>>, "callback type not support");
        RpcConnectionPool::ptr self = shared_from_this();
        IOManager::get_this()->schedule(
            [cb = std::move(cb), name = std::move(name), tp = std::move(tp), size, self, this]() {
                auto proxy = [&cb, &name, &tp, &size, &self,
                              this ]<std::size_t... Index>(std::index_sequence<Index...>) {
                    cb(call<ret>(name, std::get<Index>(tp)...));
                };

                proxy(std::make_index_sequence<size - 1> {});
            });
    }

    /**
     * @brief 异步远程过程调用
     *
     * @tparam R 返回值类型
     * @tparam Params 可变参
     * @param name 调用函数名
     * @param params
     * @return Channel<Result<R>>
     */
    template <class R, class... Params>
    Channel<Result<R>> async_call(const std::string& name, Params&&... params) {
        Channel<Result<R>> channel(1);
        RpcConnectionPool::ptr self = shared_from_this();
        IOManager::get_this()->schedule([channel, name, params..., self, this]() mutable {
            channel << call<R>(name, params...);
            self.reset();
        });
        return channel;
    }

    /**
     * @brief 远程过程调用
     *
     * @tparam R
     * @tparam Params
     * @param name
     * @param params
     * @return Result<R>
     */
    template <class R, class... Params>
    Result<R> call(const std::string& name, Params... params) {
        LockGuard lock(m_connections_mutex);
        // 从连接池中取出服务连接
        auto connection = m_connections.find(name);
        Result<R> result;
        if (connection != m_connections.end()) {
            lock.unlock();
            result = connection->second->template call<R>(name, params...);
            if (result.get_code() != RPC_CLOSED) {
                return result;
            }
            lock.lock();
            // 移除失效连接
            std::vector<std::string>& addrs = m_service_cache[name];
            // 将失效的远程连接从addrs中移除
            std::erase(addrs, connection->second->get_socket()->get_remote_address()->to_string());
            m_connections.erase(name);
        }

        std::vector<std::string>& addrs = m_service_cache[name];
        // 如果服务地址缓存为空则重新向服务中心请求服务发现
        if (addrs.empty()) {
            if (!m_registry || !m_registry->is_connected()) {
                result.set_code(RPC_CLOSED);
                result.set_message("registry closed");
                return result;
            }
            addrs = discover(name);
            // 如果没有发现服务返回错误
            if (addrs.empty()) {
                result.set_code(RPC_NO_METHOD);
                result.set_message("no method: " + name);
                return result;
            }
        }

        // 选择客户端负载均衡策略，根据路由策略选择服务地址
        RouteStrategy<std::string>::ptr strategy =
            RouteEngine<std::string>::query_strategy(Strategy::RANDOM);
        if (addrs.size()) {
            const std::string ip = strategy->select(addrs);
            Address::ptr address = Address::look_up_any(ip);
            // 选择的服务地址有效
            if (address) {
                RpcClient::ptr client = std::make_shared<RpcClient>();
                // 成功连接上服务器
                if (client->connect(address)) {
                    m_connections.emplace(name, client);
                    lock.unlock();
                    return client->template call<R>(name, params...);
                }
            }
        }

        result.set_code(RPC_FAIL);
        result.set_message("call fail");
        return result;
    }

    /**
     * @brief 订阅消息
     *
     * @tparam Func
     * @param key 订阅的key
     * @param func 回调函数
     */
    template <class Func>
    void subscribe(const std::string& key, Func func) {
        {
            LockGuard lock(m_sub_mutex);
            auto it = m_sub_handle.find(key);
            if (it != m_sub_handle.end()) {
                assert(false);
                return;
            }

            m_sub_handle.emplace(key, std::move(func));
        }

        Serializer s;
        s << key;
        s.reset();
        Protocol::ptr response =
            Protocol::create(Protocol::MessageType::RPC_SUBSCRIBE_REQUEST, s.to_string(), 0);
        m_channel << response;
    }

    void close();

private:
    /**
     * @brief 服务发现
     *
     * @param name 服务名称
     * @return std::vector<std::string> 服务地址列表
     */
    auto discover(const std::string& name) -> std::vector<std::string>;

    /**
     * @brief rpc 连接对象的发送协程，通过 Channel 收集调用请求，并转发请求给注册中心。
     */
    void handle_send();

    /**
     * @brief rpc 连接对象的接收协程，负责接收注册中心发送的 response 响应并根据响应类型进行处理
     */
    void handle_recv();

    /**
     * @brief 处理注册中心服务发现响应
     */
    void handle_service_discover(Protocol::ptr response);

    /**
     * @brief 处理发布消息
     */
    void handle_publish(Protocol::ptr protocol);

private:
    bool m_is_close = true;
    bool m_is_heart_close = true;
    uint64_t m_timeout_ms;
    // 保护m_connections
    MutexType m_connections_mutex;
    // 服务名到全部缓存的服务地址列表映射
    std::map<std::string, std::vector<std::string>> m_service_cache;
    // 服务名和服务地址的连接池
    std::map<std::string, RpcClient::ptr> m_connections;
    // 服务中心连接
    RpcSession::ptr m_registry;
    // 服务中心心跳定时器
    Timer::ptr m_heart_timer;
    // 注册中心消息发送通道
    Channel<Protocol::ptr> m_channel;
    // 服务名到对应调用者协程的 Channel 映射
    std::map<std::string, Channel<Protocol::ptr>> m_discover_handle;
    // m_discover_handle 的 mutex
    MutexType m_discover_mutex;
    // 处理订阅的消息回调函数
    std::map<std::string, std::function<void(Serializer)>> m_sub_handle;
    // 保护m_subHandle
    MutexType m_sub_mutex;
};

}  // namespace acid::rpc

#endif