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

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <sys/types.h>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>

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

    // 创建socket, 绑定地址
    bool bind(Address::ptr addr, bool ssl = false) override;

    // 创建socket, 连接服务中心
    bool bind_registry(Address::ptr address);

    /**
     * @brief 开启RpcServer, 并初始化各种服务
     *
     * @return true
     * @return false
     */
    bool start() override;

    /**
     * @brief 注册函数
     *
     * @tparam Func
     * @param name 注册的函数名
     * @param func 注册的函数
     */
    template <class Func>
    void register_method(const std::string& name, Func func) {
        // 注册调用的函数, 通过Serializer返回调用结果
        m_handlers[name] = [func, this](Serializer::ptr s, const std::string& arg) {
            proxy(func, s, arg);
        };
    }

    void set_name(std::string& name) override {
        TcpServer::set_name(name);
    }

    /**
     * @brief 发布消息, 当服务中心断开时, 由服务器直接提供服务时, 在服务变更时需要发布
     *
     * @tparam T
     * @param name 发布的消息名
     * @param data 支持Serializer的数据都可以发布
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
        // 向所有订阅了该key的客户端发布该key进行更改的消息
        Protocol::ptr pub =
            Protocol::create(Protocol::MessageType::RPC_PUBLISH_REQUEST, s.to_string(), 0);
        LockGuard lock(m_sub_mutex);
        auto range = m_subscribes.equal_range(key);
        for (auto it = range.first; it != range.second;) {
            auto connection = it->second.lock();
            if (connection == nullptr || !connection->is_connected()) {
                continue;
            }
            connection->send_protocol(pub);
        }
    }

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
    Serializer::ptr call(const std::string& name, const std::string& arg);

    /**
     * @brief 调用代理
     *
     * @tparam Func
     * @param func
     * @param serializer
     * @param arg
     */
    template <class Func>
    void proxy(Func func, Serializer::ptr serializer, const std::string& arg) {
        // 将传入的函数转换为std::function模式
        typename function_traits<Func>::stl_function_type func_stl(func);
        // 萃取该函数的返回类型以及参数类型
        using Return = typename function_traits<Func>::return_type;
        using Args = typename function_traits<Func>::tuple_type;

        Serializer s(arg);
        Args args;
        try {
            // 反序列化参数列表
            s >> args;
        }
        catch (...) {
            // 出现异常说明反序列化类型不匹配，即传入的参数与调用的函数参数不匹配
            Result<Return> res;
            res.set_code(RPC_NO_MATCH);
            res.set_code("params not match");
            *serializer << res;
            return;
        }

        return_type_t<Return> rt {};

        constexpr auto size = std::tuple_size<typename std::decay<Args>::type>::value;
        // 展开参数
        auto invoke = [&func_stl, &args ]<std::size_t... Index>(std::index_sequence<Index...>) {
            return func_stl(std::get<Index>(std::forward<Args>(args))...);
        };

        // 有返回值和无返回值情况
        if constexpr (std::is_same_v<Return, void>) {
            invoke(std::make_index_sequence<size> {});
        }
        else {
            rt = invoke(std::make_index_sequence<size> {});
        }

        Result<Return> val;
        val.set_code(RPC_SUCCESS);
        val.set_value(rt);
        *serializer << val;
    }

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
    std::map<std::string, std::function<void(Serializer::ptr, const std::string&)>> m_handlers;
    // 服务中心连接
    RpcSession::ptr m_registry;
    // 心跳定时器
    Timer::ptr m_heart_timer;
    // 开放服务的端口
    uint32_t m_port;
    // 心跳时间
    uint64_t m_alive_time;
    // 订阅的客户端
    std::unordered_multimap<std::string, std::weak_ptr<RpcSession>> m_subscribes;
    MutexType m_sub_mutex;
    // 停止清理订阅的协程
    bool m_stop_clean = false;
    // 等待清理协程停止
    Channel<bool> m_clean_channel {1};
};

};  // namespace acid::rpc


#endif