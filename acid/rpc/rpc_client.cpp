#include "rpc_client.h"

#include "acid/common/config.h"

static auto logger = GET_LOGGER_BY_NAME("sysytem");

namespace acid::rpc {

static ConfigVar<size_t>::ptr g_channel_capacity =
    Config::look_up<size_t>("rpc.client.channel_capacity", 1024, "rpc client channel capacity");

static uint64_t s_channel_capacity = 1;

struct _RpcClientIniter {
    _RpcClientIniter() {
        s_channel_capacity = g_channel_capacity->get_value();
        g_channel_capacity->add_listener([](const size_t& old_val, const size_t& new_val) {
            LOG_INFO(logger) << "rpc client channel capacity change from " << old_val << " to "
                             << new_val;
            s_channel_capacity = new_val;
        });
    }
};

static _RpcClientIniter s_rpc_client_initer;

RpcClient::RpcClient(bool auto_heartbeat)
    : m_auto_heartbeat(auto_heartbeat), m_channel(s_channel_capacity) {
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
        i.second << nullptr;
    }

    m_response_handle.clear();
    if (m_heart_timer) {
        m_heart_timer->cancel();
        m_heart_timer.reset();
    }
    IOManager::get_this()->del_event(m_session->get_socket()->get_socket(), IOManager::Event::READ);
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
                m_is_heart_close = false;
            },
            true);
    }

    return true;
}



}  // namespace acid::rpc