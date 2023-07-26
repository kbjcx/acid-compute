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

#include <cassert>
#include <functional>
#include <future>
#include <memory>

namespace acid::rpc {

class RpcClient : public std::enable_shared_from_this<RpcClient> {
public:
    using ptr = std::shared_ptr<RpcClient>;
    using MutexType = CoMutex;
    using LockGuard = MutexType::Lock;

    explicit RpcClient(bool auto_heartbeat = true);

    ~RpcClient();

    void close();

    bool connect(Address::ptr address);

    void set_timeout(uint64_t timeout_ms) {
        m_timeout = timeout_ms;
    }

    /**
     * @brief 有参数的函数调用
     *
     * @tparam R 返回值类型
     * @tparam Params 参数类型
     * @param name 函数名
     * @param params 参数
     * @return Result<R> 返回值包装
     */
    template <class R, class... Params>
    Result<R> call(const std::string& name, Params... params) {
        // std::decay去除参数类型的const volatile 以及引用
        using args_type = std::tuple<typename std::decay<Params>::type...>;
        args_type args = std::make_tuple(params...);
        Serializer s;
        s << name << args;
        s.reset();
        return call<R>(s);
    }

    /**
     * @brief 无参数的函数调用
     *
     * @tparam R
     * @param name
     * @return Result<R>
     */
    template <class R>
    Result<R> call(const std::string& name) {
        Serializer s;
        s << name;
        s.reset();
        return call<R>(s);
    }

    /**
     * @brief 异步回调模式
     *
     * @tparam Params
     * @param name 函数名
     * @param params 可变参
     */
    template <class... Params>
    void callback(const std::string& name, Params&&... params) {
        // 参数里携带回调函数
        static_assert(sizeof...(params), "without a callback function");
        auto tp = std::make_tuple(params...);
        // 在编译期获取tuple的长度
        constexpr auto size = std::tuple_size<typename std::decay<decltype(tp)>::type>::value;
        // 最后一个参数为回调函数
        auto cb = std::get<size - 1>(tp);
        // 只支持带一个参数的回调函数
        static_assert(function_traits<decltype(cb)> {}.arity == 1, "callback type not support");
        // 获取第一个参数的类型
        using res = typename function_traits<decltype(cb)>::template args<0>::type;
        // raw_type是Result类中的, 说明回调函数需要接收Restult<ret>作为参数
        using ret = typename res::raw_type;
        // 判断cb可调用对象是否可以接受Result<ret>作为参数
        static_assert(std::is_invocable_v<decltype(cb), Result<ret>>, "callback type not support");
        RpcClient::ptr self = shared_from_this();
        IOManager::get_this()->schedule(
            // index_sequence用于在编译期展开tuple
            [cb = std::move(cb), name = std::move(name), tp = std::move(tp), size, self, this] {
                auto proxy = [&cb, &name, &tp, &size, &self,
                              this ]<std::size_t... Index>(std::index_sequence<Index...>) {
                    // 调用结束后获取到了结果,再进行回调处理
                    cb(call<ret>(name, std::get<Index>(tp)...));
                };
                proxy(std::make_index_sequence<size - 1> {});
            });
    }

    /**
     * @brief 异步调用模式
     *
     * @tparam R
     * @tparam Params
     * @param name
     * @param params
     * @return Channel<Result<R>> 将结果存放到channel, 存放进去后, 就能够读取, 否则会阻塞在channel
     */
    template <class R, class... Params>
    Channel<Result<R>> async_call(const std::string& name, Params&&... params) {
        // channel的队列大小为1
        Channel<Result<R>> channel(1);
        auto self = shared_from_this();
        IOManager::get_this()->schedule([self, channel, name, params..., this]() mutable {
            // 将调用结果放到消息队列中去
            channel << call<R>(name, params...);
            // 重置智能指针, 引用计数-1
            self.reset();
        });
        return channel;
    }

    /**
     * @brief 订阅消息
     *
     * @tparam Func 回调函数类型
     * @param key 订阅的key
     * @param func 回调函数
     */
    template <class Func>
    void subscribe(const std::string& key, Func func) {
        {
            // 注册订阅消息的处理函数
            LockGuard lock(m_sub_mutex);
            auto it = m_sub_handle.find(key);
            if (it != m_sub_handle.end()) {
                // 已经存在该订阅, 则返回
                assert(false);
                return;
            }

            m_sub_handle.emplace(key, std::move(func));
        }

        // 向服务端订阅该key消息, 发送订阅请求的消息
        Serializer s;
        s << key;
        s.reset();
        Protocol::ptr request =
            Protocol::create(Protocol::MessageType::RPC_SUBSCRIBE_REQUEST, s.to_string(), 0);
        m_channel << request;
    }

    Socket::ptr get_socket() {
        return m_session->get_socket();
    }

    bool is_close() {
        return !m_session || !m_session->is_connected();
    }

private:
    void handle_send();

    void handle_recv();

    void handle_method_response(Protocol::ptr response);

    void handle_publish(Protocol::ptr protocol);

    template <class R>
    Result<R> call(Serializer s) {
        Result<R> ret;
        // 连接关闭直接返回RPC_CLOSED
        if (is_close()) {
            ret.set_code(RPC_CLOSED);
            ret.set_message("socket closed");
            return ret;
        }

        // 开启一个channel, 接收消息
        Channel<Protocol::ptr> channel(1);
        // 本次调用的序列号
        uint32_t id = 0;
        // 将调用的channel插入到映射, 根据序列号区分不同的调用协程
        std::map<uint32_t, Channel<Protocol::ptr>>::iterator it;
        {
            LockGuard lock(m_mutex);
            id = m_sequence_id;
            // 将请求序列号与接收的channel相关联, 用来获取结果
            it = m_response_handle.emplace(m_sequence_id, channel).first;
            ++m_sequence_id;
        }

        // 创建请求协议, 附带上请求ID, 请求调用
        Protocol::ptr request =
            Protocol::create(Protocol::MessageType::RPC_METHOD_REQUEST, s.to_string(), id);
        // 向send协程的channel发送消息
        m_channel << request;

        Timer::ptr timer;
        bool timeout = false;
        if (m_timeout != static_cast<uint64_t>(-1)) {
            // 如果超时还没有获取到response则关闭channel
            timer = IOManager::get_this()->add_timer(
                m_timeout,
                [channel, &timeout]() mutable {
                    timeout = true;
                    channel.close();
                },
                false);
        }

        Protocol::ptr response;
        // 等待 response，Channel内部会挂起协程，如果有消息到达或者被关闭则会被唤醒
        channel >> response;
        // 收到回复取消定时器
        if (timer) {
            timer->cancel();
        }
        // 清除该channel的映射
        {
            LockGuard lock(m_mutex);
            if (!m_is_close) {
                m_response_handle.erase(it);
            }
        }

        if (timeout) {
            // 超时
            ret.set_code(RPC_TIMEOUT);
            ret.set_message("call timeout");
            return ret;
        }

        if (!response) {
            ret.set_code(RPC_CLOSED);
            ret.set_message("socket closed");
            return ret;
        }

        if (response->get_content().empty()) {
            ret.set_code(RPC_NO_METHOD);
            ret.set_message("method not found");
            return ret;
        }

        Serializer serializer(response->get_content());
        try {
            serializer >> ret;
        }
        catch (...) {
            // 返回的结果类型不匹配
            ret.set_code(RPC_NO_MATCH);
            ret.set_message("return value not match");
        }
        return ret;
    }

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