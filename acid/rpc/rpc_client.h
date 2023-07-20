/**
 * @file rpc_client.h
 * @author kbjcx (lulu5v@163.com)
 * @brief
 * @version 0.1
 * @date 2023-07-20
 *
 * @copyright Copyright (c) 2023
 *
 */

#ifndef ACID_RPC_CLIENT_H
#define ACID_RPC_CLIENT_H

#include "acid/common/channel.h"
#include "acid/common/co_mutex.h"
#include "acid/common/iomanager.h"
#include "acid/common/mutex.h"
#include "acid/common/traits.h"
#include "acid/net/socket.h"
#include "acid/net/socket_stream.h"
#include "protocol.h"
#include "route_strategy.h"
#include "rpc.h"
#include "rpc_session.h"

#include <functional>
#include <future>
#include <memory>

namespace acid::rpc {

class RpcClient : public std::enable_shared_from_this<RpcClient> {
public:
    using ptr = std::shared_ptr<RpcClient>;
    using MutexType = CoMutex;
    using LockGuard = MutexType::Lock;

    RpcClient(bool auto_heartbeat = true);

    ~RpcClient();

    void close();

    bool connect(Address::ptr address);

    void set_timeout(uint64_t timeout_ms) {
        m_timeout = timeout_ms;
    }

    template <class R, class... Params>
    Result<R> call(const std::string& name, Params... params) {
        using args_type = std::tuple<typename std::decay<Params>::type...>;
        args_type args = std::make_tuple(params...);
        Serializer s;
        s << name << args;
        s.reset();
        return call<R>(s);
    }

    template <class R>
    Result<R> call(const std::string& name) {
        Serializer s;
        s << name;
        s.reset();
        return call<R>(s);
    }

    template <class... Params>
    void callback(const std::string& name, Params&&... params) {
        static_assert(sizeof...(params), "without a callback function");
        auto tp = std::make_tuple(params...);
        constexpr auto size = std::tuple_size<typename std::decay<decltype(tp)>::type>::value;
        auto cb = std::get<size - 1>(tp);
        static_assert(function_traits<decltype(cb)> {}.arity == 1, "callback type not support");
        using res = typename function_traits<decltype(cb)>::args<0>::type;
        using ret = typename res::raw_type;
        static_assert(std::is_invocable_v<decltype(cb), Result<ret>>, "callback type not support");
        RpcClient::ptr self = shared_from_this();
        IOManager::get_this()->schedule(
            [cb = std::move(cb), name = std::move(name), tp = std::move(tp), size, self, this] {
                auto proxy = [&cb, &name, &tp, &size, &self,
                              this ]<std::size_t... Index>(std::index_sequence<Index...>) {
                    cb(call<rt>(name, std::get<Index>(tp)...));
                };
                proxy(std::make_index_sequence<size - 1> {});
            });
    }

    template <class R, class... Params>
    Channel<Result<R>> async_call(const std::string& name, Params&&... params) {
        Channel<Result<R>> channel(1);
        auto self = shared_from_this();
        IOManager::get_this()->schedule([self, channel, name, params..., this]() mutable {
            channel << call<R>(name, params...);
            self.reset();
        });
        return channel;
    }

    template <class Func>
    void subscribe(const std::string& key, Func func);

    Socket::ptr get_socket();

    bool is_close();

private:
    void handle_send();

    void handle_recv();

    void handle_method_response(Protocol::ptr response);

    void handle_publish(Protocol::ptr protocol);

    template <class R>
    Result<R> call(Serializer s);

private:
    bool m_auto_heartbeat = true;  // 是否自动开启心跳包
    bool m_is_close = true;        // 是否结束运行
    bool m_is_heart_close = true;
    uint64_t m_timeout;          // 超时时间
    RpcSession::ptr m_session;   // 服务器的连接
    uint32_t m_sequence_id = 0;  // 序列号
    // 序列号到对应调用者协程的Channel映射
    std::map<uint32_t, Channel<Protocol::ptr>> m_response_handle;
    MutexType m_mutex;                 // m_response_handle的mutex
    Channel<Protocol::ptr> m_channel;  // 消息发送通道
    Timer::ptr m_heart_timer;          // 心跳定时器
    std::map<std::string, std::function<void(Serializer)>> m_sub_handle;  // 处理订阅的消息回调函数
    MutexType m_sub_mutex;                                                // m_sub_handle的mutex
};

}  // namespace acid::rpc

#endif