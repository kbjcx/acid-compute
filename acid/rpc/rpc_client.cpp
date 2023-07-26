#include "rpc_client.h"

#include "acid/common/config.h"

#include <cstddef>
#include <cstdint>

static auto logger = GET_LOGGER_BY_NAME("sysytem");

namespace acid::rpc {

static ConfigVar<size_t>::ptr g_channel_capacity =
    Config::look_up<size_t>("rpc.client.channel_capacity", 1024, "rpc client channel capacity");

static uint64_t s_channel_capacity = 1;

// static ConfigVar<size_t>::ptr g_sequence_id_upper_bound = Config::look_up<size_t>(
//     "rpc.client.sequence_id_upper_bound", 1024, "rpc client sequence id upper bound");

// static uint32_t s_sequence_id_upper_bound = 0;
// 初始化操作, 当需要在main函数之间执行时可以这么写
struct _RpcClientIniter {
    _RpcClientIniter() {
        s_channel_capacity = g_channel_capacity->get_value();
        g_channel_capacity->add_listener([](const size_t& old_val, const size_t& new_val) {
            LOG_INFO(logger) << "rpc client channel capacity change from " << old_val << " to "
                             << new_val;
            s_channel_capacity = new_val;
        });

        // s_sequence_id_upper_bound = g_sequence_id_upper_bound->get_value();
        // g_sequence_id_upper_bound->add_listener([](const size_t& old_val, const size_t& new_val)
        // {
        //     LOG_INFO(logger) << "rpc client sequence id upper bound change from " << old_val
        //                      << " to " << new_val;
        //     s_sequence_id_upper_bound = new_val;
        // });
    }
};

static _RpcClientIniter s_rpc_client_initer;

RpcClient::RpcClient(bool auto_heartbeat)
    : m_auto_heartbeat(auto_heartbeat)
    // , m_sequence_id_upper_bound(s_sequence_id_upper_bound)
    , m_channel(s_channel_capacity) {
}

RpcClient::~RpcClient() {
    close();
}

void RpcClient::close() {
    LOG_DEBUG(logger) << "RpcClient::close()";
    LockGuard lock(m_mutex);
    if (m_is_close) {
        return;
    }
    m_is_heart_close = true;
    m_is_close = true;
    m_channel.close();
    for (auto i : m_response_handle) {
        // 用来告知Result<R> call(Serializer s)RPC关闭, 退出协程
        i.second << nullptr;
    }

    m_response_handle.clear();
    if (m_heart_timer) {
        m_heart_timer->cancel();
        m_heart_timer.reset();
    }
    IOManager::get_this()->del_event(m_session->get_socket()->get_socketfd(),
                                     IOManager::Event::READ);
    m_session->close();
}

bool RpcClient::connect(Address::ptr address) {
    Socket::ptr sock = Socket::create_tcp(address);
    if (!sock) {
        return false;
    }

    if (!sock->connect(address, m_timeout)) {
        m_session.reset();
        return false;
    }

    m_is_heart_close = false;
    m_is_close = false;
    m_session = std::make_shared<RpcSession>(sock);
    // 一个rpc连接会接受多个调用请求, 通过channel来区分不同的调用请求
    m_channel = Channel<Protocol::ptr>(s_channel_capacity);
    IOManager::get_this()->schedule([this]() { handle_recv(); });
    IOManager::get_this()->schedule([this]() { handle_send(); });

    if (m_auto_heartbeat) {
        m_heart_timer = IOManager::get_this()->add_timer(
            30'000,
            [this]() {
                LOG_DEBUG(logger) << "heartbeat";
                if (m_is_heart_close) {
                    LOG_DEBUG(logger) << "server closed";
                    close();
                }
                // 创建心跳包
                Protocol::ptr protocol =
                    Protocol::create(Protocol::MessageType::HEARTBEAT_PACKET, "");
                // 向send协程发送Channel消息
                m_channel << protocol;
                // 若没收到回复, 则表明心跳包失效
                m_is_heart_close = true;
            },
            true);
    }

    return true;
}

void RpcClient::handle_send() {
    Protocol::ptr request;
    // 通过 Channel 收集调用请求，如果没有消息时 Channel 内部会挂起该协程等待消息到达
    // Channel 被关闭时会退出循环
    while (m_channel >> request) {
        if (!request) {
            LOG_WARN(logger) << "RpcClient::handle_send() fail";
            continue;
        }
        // 发送请求
        m_session->send_protocol(request);
    }
}

void RpcClient::handle_recv() {
    if (!m_session->is_connected()) {
        return;
    }

    while (true) {
        // 接收响应
        Protocol::ptr response = m_session->recv_protocol();
        if (!response) {
            LOG_WARN(logger) << "RpcClient::handle_recv() fail";
            close();
            break;
        }

        m_is_heart_close = false;
        Protocol::MessageType type = response->get_message_type();
        // 根据响应类型进行处理
        switch (type) {
            case Protocol::MessageType::HEARTBEAT_PACKET:
                m_is_heart_close = false;
                break;
            case Protocol::MessageType::RPC_METHOD_RESPONSE:
                // 处理调用结果
                handle_method_response(response);
                break;
            case Protocol::MessageType::RPC_PUBLISH_RESPONSE:
                handle_publish(response);
                break;
            case Protocol::MessageType::RPC_SUBSCRIBE_RESPONSE:
                break;
            default:
                LOG_DEBUG(logger) << "protocol: " << response->to_string();
                break;
        }
    }
}

void RpcClient::handle_method_response(Protocol::ptr response) {
    // 获取该调用结果的序列号
    uint32_t id = response->get_sequence_id();
    std::map<uint32_t, Channel<Protocol::ptr>>::iterator it;

    LockGuard lock(m_mutex);
    // 查找该序列号对应的channel是否还存在
    it = m_response_handle.find(id);
    if (it == m_response_handle.end()) {
        return;
    }

    // 获取等待结果的channel
    Channel<Protocol::ptr> channel = it->second;
    // 向channel发送调用结果, 唤醒Result<R> call(Serializer s)协程
    channel << response;
}

// 通过客户端直连服务端或者注册中心才会有publish
void RpcClient::handle_publish(Protocol::ptr protocol) {
    Serializer s(protocol->get_content());
    std::string key;
    s >> key;
    LockGuard lock(m_sub_mutex);
    auto it = m_sub_handle.find(key);
    if (it == m_sub_handle.end()) return;

    // 由自定义的订阅消息处理函数处理回复
    it->second(s);
}

}  // namespace acid::rpc