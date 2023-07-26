#include "rpc_server.h"

#include "acid/common/config.h"
#include "acid/common/iomanager.h"
#include "acid/logger/logger.h"
#include "acid/net/address.h"
#include "acid/net/socket.h"
#include "acid/net/tcp_server.h"
#include "acid/rpc/protocol.h"
#include "acid/rpc/rpc.h"
#include "acid/rpc/rpc_session.h"
#include "acid/rpc/serializer.h"

#include <cstdint>
#include <memory>

namespace acid::rpc {

static auto logger = GET_LOGGER_BY_NAME("system");

static ConfigVar<uint64_t>::ptr g_heartbeat_timeout = Config::look_up<uint64_t>(
    "rpc.server.heartbeat_timeout", 40'000, "rpc server heartbeat timeout(ms)");
static uint64_t s_heartbeat_timeout = 0;

struct _RpcServerIniter {
    _RpcServerIniter() {
        s_heartbeat_timeout = g_heartbeat_timeout->get_value();
        g_heartbeat_timeout->add_listener([](const uint64_t& old_val, const uint64_t& new_val) {
            LOG_INFO(logger) << "rpc server heartbeat timeout change from " << old_val << " to "
                             << new_val;
            s_heartbeat_timeout = new_val;
        });
    }
};

static _RpcServerIniter s_initer;

RpcServer::RpcServer(IOManager* worker, IOManager* io_worker, IOManager* accept_worker)
    : TcpServer("RpcServer", worker, io_worker, accept_worker), m_alive_time(s_heartbeat_timeout) {
}

RpcServer::~RpcServer() {
    {
        LockGuard lock(m_sub_mutex);
        m_stop_clean = true;
    }
    bool join;
    m_clean_channel >> join;
}

bool RpcServer::bind(Address::ptr addr, bool ssl) {
    m_port = std::dynamic_pointer_cast<IPv4Address>(addr)->get_port();
    return TcpServer::bind(addr);
}

bool RpcServer::bind_registry(Address::ptr address) {
    Socket::ptr sock = Socket::create_tcp(address);

    if (!sock) {
        return false;
    }

    if (!sock->connect(address)) {
        LOG_WARN(logger) << "can not connect to registry";
        m_registry.reset();
        return false;
    }

    m_registry = std::make_shared<RpcSession>(sock);

    Serializer s;
    s << m_port;
    s.reset();

    // 向服务中心声明为provider, 注册服务端口
    Protocol::ptr request = Protocol::create(Protocol::MessageType::RPC_PROVIDER, s.to_string(), 0);
    m_registry->send_protocol(request);
    return true;
}

bool RpcServer::start() {
    if (m_registry) {
        for (auto& item : m_handlers) {
            register_service(item.first);
        }

        auto self = std::dynamic_pointer_cast<RpcServer>(shared_from_this());
        // 服务中心心跳定时器
        m_registry->get_socket()->set_recv_timeout(30'000);
        m_heart_timer = m_worker->add_timer(
            30'000,
            [self]() {
                LOG_DEBUG(logger) << "heartbeat";
                Protocol::ptr proto =
                    Protocol::create(Protocol::MessageType::HEARTBEAT_PACKET, "", 0);
                self->m_registry->send_protocol(proto);
                Protocol::ptr response = self->m_registry->recv_protocol();

                if (!response) {
                    LOG_WARN(logger) << "Registry closed";
                    // 放弃服务中心, 独自提供服务
                    self->m_heart_timer->cancel();
                }
            },
            true);
    }

    // 开启协程定时清理订阅列表
    m_worker->add_timer(
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

    return TcpServer::start();
}

void RpcServer::handle_client(Socket::ptr client) {
    LOG_DEBUG(logger) << "RpcServer::handle_client: " << client->to_string();
    RpcSession::ptr session = std::make_shared<RpcSession>(client);

    Timer::ptr heart_timer;
    // 开启心跳定时器
    update(heart_timer, client);
    while (true) {
        Protocol::ptr request = session->recv_protocol();
        if (!request) {
            break;
        }

        // 更新定时器
        update(heart_timer, client);

        auto self = shared_from_this();

        // 启动一个任务协程
        m_worker->schedule([request, session, self, this]() mutable {
            Protocol::ptr response;
            Protocol::MessageType type = request->get_message_type();
            switch (type) {
                case Protocol::MessageType::HEARTBEAT_PACKET:
                    response = handle_heartbeat_packet(request);
                    break;
                case Protocol::MessageType::RPC_METHOD_REQUEST:
                    response = handle_method_call(request);
                    break;
                case Protocol::MessageType::RPC_SUBSCRIBE_REQUEST:
                    response = handle_subscribe(request, session);
                    break;
                case Protocol::MessageType::RPC_PUBLISH_RESPONSE:
                    return;
                default:
                    LOG_DEBUG(logger) << "protocol: " << request->to_string();
                    break;
            }

            if (response) {
                session->send_protocol(response);
            }

            self.reset();
        });
    }
}

void RpcServer::update(Timer::ptr heart_timer, Socket::ptr client) {
    if (!heart_timer) {
        heart_timer = m_worker->add_timer(
            m_alive_time,
            [client]() {
                LOG_DEBUG(logger) << "client: " << client->to_string() << " closed";
                client->close();
            },
            true);
        return;
    }
    m_heart_timer->reset(m_alive_time, true);
}

Serializer RpcServer::call(const std::string& name, const std::string& arg) {
    Serializer serializer;
    auto it = m_handlers.find(name);
    if (it == m_handlers.end()) {
        return serializer;
    }

    auto func = it->second;
    func(&serializer, arg);
    serializer.reset();
    return serializer;
}

Protocol::ptr RpcServer::handle_method_call(Protocol::ptr proto) {
    std::string func_name;
    Serializer request(proto->get_content());
    request >> func_name;
    Serializer ret = call(func_name, request.to_string());
    Protocol::ptr response = Protocol::create(Protocol::MessageType::RPC_METHOD_RESPONSE,
                                              ret.to_string(), proto->get_sequence_id());
    return response;
}

void RpcServer::register_service(const std::string& name) {
    Protocol::ptr proto = Protocol::create(Protocol::MessageType::RPC_SERVICE_REGISTER, name, 0);
    m_registry->send_protocol(proto);

    Protocol::ptr response = m_registry->recv_protocol();
    if (!response) {
        LOG_WARN(logger) << "register service: " << name
                         << " fail, registry socket: " << m_registry->get_socket()->to_string();
        return;
    }

    Result<std::string> res;
    Serializer s(response->get_content());
    s >> res;
    if (res.get_code() != RPC_SUCCESS) {
        LOG_WARN(logger) << res.to_string();
        return;
    }
    LOG_INFO(logger) << res.to_string();
}

Protocol::ptr RpcServer::handle_heartbeat_packet(Protocol::ptr proto) {
    return Protocol::heartbeat();
}

Protocol::ptr RpcServer::handle_subscribe(Protocol::ptr proto, RpcSession::ptr client) {
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