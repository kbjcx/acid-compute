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
        heart_timer = m_worker->add_timer(m_alive_time, [client]() {
            LOG_DEBUG(logger) << "client: " << client->to_string() << " closed";
            client->close();
        });
        return;
    }
    heart_timer->reset(m_alive_time, true);
}

void RpcServiceRegistry::handle_client(Socket::ptr client) {
    LOG_DEBUG(logger) << "RpcServiceRegistry::handle_client: " << client->to_string();
    RpcSession::ptr session = std::make_shared<RpcSession>(client);
    Timer::ptr heart_timer;
    // 开启心跳定时器
    update(heart_timer, client);
    Address::ptr provider_address;
    while (true) {
        Protocol::ptr request = session->recv_protocol();
        if (!request) {
            if (provider_address) {
                LOG_WARN(logger) << client->to_string() << " was closed; unregister "
                                 << provider_address->to_string();
                handle_unregidter_service(provider_address);
            }
        }
        // 收到消息, 更新定时器
        update(heart_timer, client);

        Protocol::ptr response;
        Protocol::MessageType type = request->get_message_type();

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

Protocol::ptr RpcServiceRegistry::handle_heartbeat_packet(Protocol::ptr protocol) {
    return Protocol::heartbeat();
}

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

Protocol::ptr RpcServiceRegistry::handle_register_service(Protocol::ptr protocol,
                                                          Address::ptr address) {
    std::string service_address = address->to_string();
    std::string service_name = protocol->get_content();

    LockGuard lock(m_mutex);
    auto it = m_services.emplace(service_name, service_address);
    m_iters[service_address].push_back(it);
    lock.unlock();

    Result<std::string> res = Result<std::string>::success();
    res.set_value(service_name);

    Serializer s;
    s << res;
    s.reset();

    Protocol::ptr response =
        Protocol::create(Protocol::MessageType::RPC_SERVICE_REGISTER_RESPONSE, s.to_string());

    // 发布服务上线的消息
    std::tuple<bool, std::string> data = {true, service_address};
    publish(RPC_SERVICE_SUBSCRIBE + service_name, data);

    return response;
}

void RpcServiceRegistry::handle_unregidter_service(Address::ptr address) {
    LockGuard lock(m_mutex);
    auto it = m_iters.find(address->to_string());
    if (it == m_iters.end()) {
        return;
    }

    auto its = it->second;
    for (auto& i : its) {
        m_services.erase(i);
        // 发布服务下线的消息
        std::tuple<bool, std::string> data = {false, it->first};
        publish(RPC_SERVICE_SUBSCRIBE + it->first, data);
    }

    m_iters.erase(it);
}

Protocol::ptr RpcServiceRegistry::handle_discover_service(Protocol::ptr proto) {
    std::string service_name = proto->get_content();
    std::vector<Result<std::string>> results;
    ByteArray byte_array;

    LockGuard lock(m_mutex);
    auto range = m_services.equal_range(service_name);
    uint32_t cnt = 0;
    // 未注册服务
    if (range.first == range.second) {
        ++cnt;
        Result<std::string> res;
        res.set_code(RPC_NO_METHOD);
        res.set_message("discover service: " + service_name);
        results.push_back(res);
    }
    else {
        for (auto it = range.first; it != range.second; ++it) {
            Result<std::string> res;
            std::string addr;
            res.set_code(RPC_SUCCESS);
            res.set_value(it->second);
            results.push_back(res);
        }
        cnt = results.size();
    }

    Serializer s;
    s << service_name << cnt;
    for (uint32_t i = 0; i < cnt; ++i) {
        s << results[i];
    }
    s.reset();
    Protocol::ptr response =
        Protocol::create(Protocol::MessageType::RPC_SERVICE_DISCOVER_RESPONSE, s.to_string());
    return response;
}

Protocol::ptr RpcServiceRegistry::handle_subscribe(Protocol::ptr proto, RpcSession::ptr client) {
    LockGuard lock(m_sub_mutex);
    std::string key;
    Serializer s(proto->get_content());
    s >> key;
    m_subscribes.emplace(key, std::weak_ptr<RpcSession>(client));
    Result<> res = Result<>::success();
    s.reset();
    s << res;
    return Protocol::create(Protocol::MessageType::RPC_SUBSCRIBE_RESPONSE, s.to_string(), 0);
}

};  // namespace acid::rpc