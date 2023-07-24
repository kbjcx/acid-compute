#include "rpc_connection_pool.h"

#include "acid/common/channel.h"
#include "acid/common/config.h"
#include "acid/common/mutex.h"
#include "acid/logger/logger.h"
#include "acid/rpc/protocol.h"
#include "acid/rpc/rpc.h"
#include "acid/rpc/serializer.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace acid::rpc {

static auto logger = GET_LOGGER_BY_NAME("system");

static ConfigVar<size_t>::ptr g_channel_capacity = Config::look_up<size_t>(
    "rpc.connection_pool.channel_capacity", 1024, "rpc connection pool channel capacity");
static uint64_t s_channel_capacity = 1;

struct _RpcConnectionPoolIniter {
    _RpcConnectionPoolIniter() {
        s_channel_capacity = g_channel_capacity->get_value();
        g_channel_capacity->add_listener([](const size_t& old_val, const size_t& new_val) {
            LOG_INFO(logger) << "rpc connection pool channel capacity changed from " << old_val
                             << " to " << new_val;
            s_channel_capacity = new_val;
        });
    }
};

static _RpcConnectionPoolIniter s_initer;

RpcConnectionPool::RpcConnectionPool(uint64_t timeout_ms)
    : m_is_close(false), m_timeout_ms(timeout_ms), m_channel(s_channel_capacity) {
}

RpcConnectionPool::~RpcConnectionPool() {
    close();
}

void RpcConnectionPool::close() {
    LOG_DEBUG(logger) << "RpcConnectionPool::close()";

    if (m_is_close) {
        return;
    }

    m_is_heart_close = true;
    m_is_close = true;
    m_channel.close();
    if (m_heart_timer) {
        m_heart_timer->cancel();
        m_heart_timer.reset();
    }

    m_discover_handle.clear();

    IOManager::get_this()->del_event(m_registry->get_socket()->get_socketfd(),
                                     IOManager::Event::READ);
    if (m_registry->is_connected()) {
        m_registry->close();
    }
    m_registry.reset();
}

bool RpcConnectionPool::connect(Address::ptr address) {
    Socket::ptr sock = Socket::create_tcp(address);
    if (!sock) {
        return false;
    }

    if (!sock->connect(address, m_timeout_ms)) {
        LOG_ERROR(logger) << "connect to registry fail";
        m_registry.reset();
        return false;
    }

    m_registry = std::make_shared<RpcSession>(sock);

    LOG_DEBUG(logger) << "connect to registry: " << m_registry->get_socket()->to_string();

    IOManager::get_this()->schedule([this]() { handle_recv(); });

    IOManager::get_this()->schedule([this]() { handle_send(); });

    // 服务中心心跳定时器
    m_heart_timer = IOManager::get_this()->add_timer(
        30'000,
        [this]() {
            LOG_DEBUG(logger) << "heartbeat";
            if (m_is_heart_close) {
                LOG_DEBUG(logger) << "registry closed";
                // 放弃服务中心
                m_heart_timer->cancel();
                m_heart_timer.reset();
            }

            // 创建心跳包
            Protocol::ptr protocol = Protocol::create(Protocol::MessageType::HEARTBEAT_PACKET, "");
            // 向send协程发送消息
            m_channel << protocol;
            m_is_heart_close = true;
        },
        true);
    return true;
}

void RpcConnectionPool::handle_send() {
    Protocol::ptr request;
    // 通过 Channel 收集调用请求，如果没有消息时 Channel 内部会挂起该协程等待消息到达
    // Channel 被关闭时会退出循环
    while (m_channel >> request) {
        if (!request) {
            LOG_WARN(logger) << "RpcConnectionPool::handle_send() fail";
            continue;
        }
        // 发送请求
        m_registry->send_protocol(request);
    }
}

void RpcConnectionPool::handle_recv() {
    if (!m_registry->is_connected()) {
        return;
    }

    while (true) {
        // 接受响应
        Protocol::ptr response = m_registry->recv_protocol();
        if (!response) {
            LOG_WARN(logger) << "RpcConnectionPool::handle_recv() fail";
            close();
            break;
        }

        m_is_heart_close = false;
        Protocol::MessageType type = response->get_message_type();
        // 判断响应类型来进行对应的处理
        switch (type) {
            case Protocol::MessageType::HEARTBEAT_PACKET:
                m_is_heart_close = false;
                break;
            case Protocol::MessageType::RPC_SERVICE_DISCOVER_RESPONSE:
                handle_service_discover(response);
                break;
            case Protocol::MessageType::RPC_PUBLISH_REQUEST:
                handle_publish(response);
                m_channel << Protocol::create(Protocol::MessageType::RPC_PUBLISH_RESPONSE, "");
                break;
            case Protocol::MessageType::RPC_SUBSCRIBE_RESPONSE:
                break;
            default:
                LOG_DEBUG(logger) << "protocol: " << response->to_string();
                break;
        }
    }
}

void RpcConnectionPool::handle_service_discover(Protocol::ptr response) {
    Serializer s(response->get_content());
    std::string service;
    s >> service;
    std::map<std::string, Channel<Protocol::ptr>>::iterator it;

    LockGuard lock(m_discover_mutex);
    // 查找该序列号的 Channel 是否还存在，如果不存在直接返回
    it = m_discover_handle.find(service);
    if (it == m_discover_handle.end()) {
        return;
    }

    // 通过服务名获取等待该结果的 Channel
    Channel<Protocol::ptr> channel = it->second;
    // 对该 Channel 发送调用结果唤醒调用者
    channel << response;
}

auto RpcConnectionPool::discover(const std::string& name) -> std::vector<std::string> {
    if (!m_registry || !m_registry->is_connected()) {
        return {};
    }

    // 开启一个channel接收调用结果
    Channel<Protocol::ptr> recv_channel(1);

    std::map<std::string, Channel<Protocol::ptr>>::iterator it;
    {
        LockGuard lock(m_discover_mutex);
        // 将请求序列号与channel关联
        it = m_discover_handle.emplace(name, recv_channel).first;
    }

    // 创建请求协议, 付带请求ID
    Protocol::ptr request = Protocol::create(Protocol::MessageType::RPC_SERVICE_DISCOVER, name);

    // 向send协程发送消息
    m_channel << request;

    Protocol::ptr response = nullptr;
    // 等待 response，Channel内部会挂起协程，如果有消息到达或者被关闭则会被唤醒
    recv_channel >> response;

    {
        LockGuard lock(m_discover_mutex);
        m_discover_handle.erase(it);
    }

    if (!response) {
        return {};
    }

    std::vector<Result<std::string>> res;
    std::vector<std::string> ret;
    std::vector<Address::ptr> addrs;

    Serializer s(response->get_content());
    uint32_t cnt;
    std::string str;

    s >> str >> cnt;
    for (uint32_t i = 0; i < cnt; ++i) {
        Result<std::string> tmp;
        s >> tmp;
        res.push_back(tmp);
    }

    if (res.front().get_code() == RPC_NO_METHOD) {
        return {};
    }

    for (size_t i = 0; i < res.size(); ++i) {
        ret.push_back(res.at(i).get_value());
    }

    if (!m_sub_handle.contains(RPC_SERVICE_SUBSCRIBE + name)) {
        // 向注册中心订阅服务的变化消息
        subscribe(RPC_SERVICE_SUBSCRIBE + name, [name, this](Serializer s) {
            // false为服务下线, true为新服务节点上线
            bool is_new_server = false;
            std::string addr;
            s >> is_new_server >> addr;
            LockGuard lock(m_connections_mutex);
            if (is_new_server) {
                // 一个新的服务提供者节点加入，将服务地址加入服务列表缓存
                LOG_DEBUG(logger) << "service [ " << name << " : " << addr << " ] join";
                m_service_cache[name].push_back(addr);
            }
            else {
                // 已有服务提供者节点下线
                LOG_DEBUG(logger) << "service [ " << name << " : " << addr << " ] quit";
                // 清理缓存中断开的连接地址
                auto its = m_service_cache.find(name);
                if (its != m_service_cache.end()) {
                    std::erase(its->second, addr);
                }
            }
        });
    }
    return ret;
}

void RpcConnectionPool::handle_publish(Protocol::ptr protocol) {
    Serializer s(protocol->get_content());
    std::string key;
    s >> key;
    LockGuard lock(m_sub_mutex);
    auto it = m_sub_handle.find(key);
    if (it == m_sub_handle.end()) return;
    it->second(s);
}


}  // namespace acid::rpc
