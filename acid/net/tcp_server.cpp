#include "tcp_server.h"

namespace acid {

static auto logger = GET_LOGGER_BY_NAME("system");

TcpServer::TcpServer() = default;

TcpServer::TcpServer(const std::string &server_name, acid::IOManager *worker,
                     acid::IOManager *io_worker, acid::IOManager *accept_worker)
    : m_worker(worker)
    , m_io_worker(io_worker)
    , m_accept_worker(accept_worker)
    , m_recv_timeout(1000 * 60 * 2)
    , m_is_stop(true) {
    if (server_name.empty()) {
        m_name = "acid/1.0";
    }
    else {
        m_name = server_name;
    }
}

TcpServer::~TcpServer() {
    for (auto &socket : m_sockets) {
        socket->close();
    }
    m_sockets.clear();
}

bool TcpServer::bind(Address::ptr addr, bool ssl) {
    std::vector<Address::ptr> addrs;
    std::vector<Address::ptr> fails;
    addrs.push_back(addr);
    return bind(addrs, fails, ssl);
}

bool TcpServer::bind(const std::vector<Address::ptr> &addrs,
                     std::vector<Address::ptr> &fails, bool ssl) {
    m_ssl = ssl;

    // 每一个地址都会创建一个listen socket
    for (auto &addr : addrs) {
        Socket::ptr socket = Socket::create_tcp(addr);
        if (!socket->bind(addr)) {
            LOG_ERROR(logger)
                << "bind fail errno=" << errno << " errstr=" << strerror(errno)
                << " addr=[" << addr->to_string() << "]";
            fails.push_back(addr);
            continue;
        }

        if (!socket->listen()) {
            LOG_ERROR(logger) << "listen fail errno=" << errno
                              << " errstr=" << strerror(errno) << " addr=["
                              << addr->to_string() << "]";
            fails.push_back(addr);
            continue;
        }

        m_sockets.push_back(socket);
    }

    if (!fails.empty()) {
        m_sockets.clear();
        return false;
    }

    for (auto &socket : m_sockets) {
        LOG_INFO(logger) << "type=" << m_type << " name=" << m_name
                         << " ssl=" << m_ssl
                         << " server bind success: " << *socket;
    }
    return true;
}

void TcpServer::start_accept(Socket::ptr socket) {
    while (!m_is_stop) {
        LOG_DEBUG(logger) << "start accept";
        Socket::ptr client = socket->accept();

        if (client) {
            client->set_recv_timeout(m_recv_timeout);
            auto self = shared_from_this();
            m_io_worker->schedule([self, client] { self->handle_client(client); });
        }
        else {
            LOG_ERROR(logger) << "accept errno=" << errno
                              << " errstr=" << strerror(errno);
        }
    }
}

bool TcpServer::start() {
    if (!m_is_stop) {
        return true;
    }
    m_is_stop = false;
    for (auto& socket : m_sockets) {
        auto self = shared_from_this();
        m_accept_worker->schedule([self, socket]() {
            self->start_accept(socket);
        });
    }
    return true;
}

void TcpServer::stop() {
    m_is_stop = true;
    auto self = shared_from_this();
    m_accept_worker->schedule([self]() {
        for (auto& socket : self->m_sockets) {
            socket->cancel_all();
            socket->close();
        }
        self->m_sockets.clear();
    });
}

void TcpServer::handle_client(Socket::ptr client) {
    LOG_INFO(logger) << "handleClient: " << *client;
}

}  // namespace acid
