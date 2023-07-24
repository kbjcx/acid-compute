#include "rpc_service_registry.h"

#include "acid/common/byte_array.h"
#include "acid/common/config.h"
#include "acid/common/iomanager.h"
#include "acid/logger/logger.h"
#include "acid/net/address.h"
#include "acid/net/socket.h"
#include "acid/net/tcp_server.h"
#include "acid/rpc/protocol.h"
#include "acid/rpc/rpc_session.h"
#include "acid/rpc/serializer.h"
#include "rpc.h"

#include <cstdint>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

namespace acid::rpc {

static auto logger = GET_LOGGER_BY_NAME("system");

static ConfigVar<uint64_t>::ptr g_heartbeat_timeout = Config::look_up<uint64_t>(
    "rpc.registry.heartbeat_timeout", 40'000, "rpc registry heartbeat timeout(ms)");

static uint64_t s_heartbeat_timeout = 0;

struct _RpcServiceRegistryIniter {
    _RpcServiceRegistryIniter() {
        s_heartbeat_timeout = g_heartbeat_timeout->get_value();
        g_heartbeat_timeout->add_listener([](const uint64_t& old_val, const uint64_t& new_val) {
            LOG_INFO(logger) << "rpc registry heartbeat timeout change from " << old_val << " to "
                             << new_val;
            s_heartbeat_timeout = new_val;
        });
    }
};

static _RpcServiceRegistryIniter s_initer;

RpcServiceRegistry::RpcServiceRegistry(IOManager* worker, IOManager* io_worker,
                                       IOManager* accept_worker)
    : TcpServer("RpcServiceRegistry", worker, io_worker, accept_worker)
    , m_alive_time(s_heartbeat_timeout) {
    // 开启协程定时清理订阅列表
    IOManager::get_this()->add_timer(
        5'000,
        [this]() {
            do {
                if (!m_stop_clean) {
                    LockGuard lock(m_sub_mutex);
                    if (m_stop_clean) {
                        break;
                    }
                    for (auto it = m_subscribes.cbegin(); it != m_subscribes.cend();) {
                        auto connection = it->second.lock();
                        if (connection == nullptr || !connection->is_connected()) {
                            it = m_subscribes.erase(it);
                        }
                        else {
                            ++it;
                        }
                    }
                    return;
                }
            } while (0);
            m_clean_channel << true;
        },
        true);
}

RpcServiceRegistry::~RpcServiceRegistry() {
    {
        LockGuard lock(m_sub_mutex);
        m_stop_clean = true;
    }
    bool join;
    m_clean_channel >> join;
}

void RpcServiceRegistry::update(Timer::ptr heart_timer, Socket::ptr client) {
    LOG_DEBUG(logger) << "RpcServiceRegistry::update";
    if (!heart_timer) {
        // 还没有创建heart_timer则创建一个心跳定时器, 定时器被触发说明在时间内没有收到过消息,
        // 则关闭该连接
        heart_timer = m_worker->add_timer(m_alive_time, [client]() {
            LOG_DEBUG(logger) << "client: " << client->to_string() << " closed";
            client->close();
        });
        return;
    }
    // 已经创建定时器了则更新定时器时间
    heart_timer->reset(m_alive_time, true);
}

void RpcServiceRegistry::handle_client(Socket::ptr client) {
    LOG_DEBUG(logger) << "RpcServiceRegistry::handle_client: " << client->to_string();
    // 创建session来管理socket
    RpcSession::ptr session = std::make_shared<RpcSession>(client);
    Timer::ptr heart_timer;
    // 开启心跳定时器
    update(heart_timer, client);
    Address::ptr provider_address;
    while (true) {
        // 接收请求消息
        Protocol::ptr request = session->recv_protocol();
        if (!request) {
            // 断开连接, 需要清除该地址提供的服务
            if (provider_address) {
                LOG_WARN(logger) << client->to_string() << " was closed; unregister "
                                 << provider_address->to_string();
                handle_unregidter_service(provider_address);
            }
        }
        // 每次收到消息, 更新定时器
        update(heart_timer, client);

        Protocol::ptr response;
        Protocol::MessageType type = request->get_message_type();

        // 根据请求类型来进行处理
        switch (type) {
            case Protocol::MessageType::HEARTBEAT_PACKET:
                response = handle_heartbeat_packet(request);
                break;
            case Protocol::MessageType::RPC_PROVIDER:
                LOG_DEBUG(logger) << client->to_string();
                provider_address = hanlde_provider(request, client);
                continue;
            case Protocol::MessageType::RPC_SERVICE_REGISTER:
                response = handle_register_service(request, provider_address);
                break;
            case Protocol::MessageType::RPC_SERVICE_DISCOVER:
                response = handle_discover_service(request);
                break;
            case Protocol::MessageType::RPC_SUBSCRIBE_REQUEST:
                response = handle_subscribe(request, session);
                break;
            case Protocol::MessageType::RPC_PUBLISH_RESPONSE:
                continue;
            default:
                LOG_WARN(logger) << "protocol: " << request->to_string();
                continue;
        }

        session->send_protocol(response);
    }
}

/**
 * @brief 直接返回心跳包
 *
 * @param protocol
 * @return Protocol::ptr
 */
Protocol::ptr RpcServiceRegistry::handle_heartbeat_packet(Protocol::ptr protocol) {
    return Protocol::heartbeat();
}

/**
 * @brief 对方注册为服务提供方, 记录对方地址
 *
 * @param protocol
 * @param sock
 * @return Address::ptr
 */
Address::ptr RpcServiceRegistry::hanlde_provider(Protocol::ptr protocol, Socket::ptr sock) {
    uint32_t port = 0;
    Serializer s(protocol->get_content());
    s.reset();
    s >> port;
    IPv4Address::ptr address(
        new IPv4Address(*std::dynamic_pointer_cast<IPv4Address>(sock->get_remote_address())));
    address->set_port(port);
    return address;
}

/**
 * @brief 对方注册服务, 一个地址可以注册多个服务, 因此需要注册为服务提供商, 在进行注册服务
 *
 * @param protocol
 * @param address
 * @return Protocol::ptr
 */
Protocol::ptr RpcServiceRegistry::handle_register_service(Protocol::ptr protocol,
                                                          Address::ptr address) {
    std::string service_address = address->to_string();
    std::string service_name = protocol->get_content();

    LockGuard lock(m_mutex);
    // 添加注册的服务名和响应的服务地址
    auto it = m_services.emplace(service_name, service_address);
    // 将该服务添加到该地址的所有服务列表下
    m_iters[service_address].push_back(it);
    lock.unlock();

    // 注册成功, 返回注册的服务名
    Result<std::string> res = Result<std::string>::success();
    res.set_value(service_name);

    Serializer s;
    s << res;
    s.reset();

    Protocol::ptr response =
        Protocol::create(Protocol::MessageType::RPC_SERVICE_REGISTER_RESPONSE, s.to_string(), 0);

    // 发布服务上线的消息
    std::tuple<bool, std::string> data = {true, service_address};
    // 告知订阅该消息的客户端, 该服务已经上线了
    publish(RPC_SERVICE_SUBSCRIBE + service_name, data);

    return response;
}

/**
 * @brief 注销该地址上注册的所有服务
 *
 * @param address
 */
void RpcServiceRegistry::handle_unregidter_service(Address::ptr address) {
    LockGuard lock(m_mutex);
    auto it = m_iters.find(address->to_string());
    if (it == m_iters.end()) {
        return;
    }

    auto its = it->second;  // std::vector<std::multimap<std::string, std::string>::iterator>
    for (auto& i : its) {
        m_services.erase(i);
        // 发布服务下线的消息
        std::tuple<bool, std::string> data = {false, it->first};
        publish(RPC_SERVICE_SUBSCRIBE + it->first, data);
    }

    m_iters.erase(it);
}

/**
 * @brief 处理服务发现的请求
 *
 * @param proto
 * @return Protocol::ptr
 */
Protocol::ptr RpcServiceRegistry::handle_discover_service(Protocol::ptr proto) {
    // 要发现的服务名
    std::string service_name = proto->get_content();
    // 返回的支持服务的地址列表, 用Result包装
    std::vector<Result<std::string>> results;
    ByteArray byte_array;

    LockGuard lock(m_mutex);
    // 支持该服务名的地址
    auto range = m_services.equal_range(service_name);
    uint32_t cnt = 0;
    // 未注册服务
    if (range.first == range.second) {
        Result<std::string> res;
        res.set_code(RPC_NO_METHOD);
        res.set_message("discover service: " + service_name);
        results.push_back(res);
        ++cnt;
    }
    else {
        // it->first: 服务名, it->second: 支持服务的地址
        for (auto it = range.first; it != range.second; ++it) {
            Result<std::string> res;
            res.set_code(RPC_SUCCESS);
            res.set_value(it->second);
            results.push_back(res);
            ++cnt;
        }
    }

    Serializer s;
    s << service_name << cnt;
    for (uint32_t i = 0; i < cnt; ++i) {
        s << results[i];
    }
    s.reset();
    Protocol::ptr response =
        Protocol::create(Protocol::MessageType::RPC_SERVICE_DISCOVER_RESPONSE, s.to_string(), 0);
    return response;
}

/**
 * @brief 处理订阅消息
 *
 * @param proto
 * @param client
 * @return Protocol::ptr
 */
Protocol::ptr RpcServiceRegistry::handle_subscribe(Protocol::ptr proto, RpcSession::ptr client) {
    LockGuard lock(m_sub_mutex);
    std::string key;
    Serializer s(proto->get_content());
    s >> key;
    // 将key和订阅的客户端加入map
    m_subscribes.emplace(key, std::weak_ptr<RpcSession>(client));
    // 回复success消息
    Result<> res = Result<>::success();
    s.clear();
    s << res;
    s.reset();
    return Protocol::create(Protocol::MessageType::RPC_SUBSCRIBE_RESPONSE, s.to_string(), 0);
}

};  // namespace acid::rpc