//
// Created by llz on 23-6-22.
//

#include "socket.h"

#include "acid/common/fd_manager.h"
#include "acid/common/hook.h"
#include "acid/common/iomanager.h"
#include "acid/logger/logger.h"

#include <netinet/tcp.h>
#include <sstream>

namespace acid {

static auto logger = GET_LOGGER_BY_NAME("system");

Socket::ptr Socket::create_tcp(Address::ptr addr) {
    Socket::ptr sock(new Socket(addr->get_family(), Type::TCP, 0));
    return sock;
}

Socket::ptr Socket::create_udp(Address::ptr addr) {
    Socket::ptr sock(new Socket(addr->get_family(), Type::UDP, 0));
    sock->new_socket();
    sock->m_is_connected = true;
    return sock;
}

Socket::ptr Socket::create_tcp_socket() {
    Socket::ptr sock(new Socket(Family::IPv4, Type::TCP, 0));
    return sock;
}

Socket::ptr Socket::create_udp_socket() {
    Socket::ptr sock(new Socket(Family::IPv4, Type::UDP, 0));
    sock->new_socket();
    sock->m_is_connected = true;
    return sock;
}

Socket::ptr Socket::create_tcp_socket6() {
    Socket::ptr sock(new Socket(Family::IPv6, Type::TCP, 0));
    return sock;
}

Socket::ptr Socket::create_udp_socket6() {
    Socket::ptr sock(new Socket(Family::IPv6, Type::UDP, 0));
    sock->new_socket();
    sock->m_is_connected = true;
    return sock;
}

Socket::ptr Socket::create_unix_tcp_socket() {
    Socket::ptr sock(new Socket(Family::Unix, Type::TCP, 0));
    return sock;
}

Socket::ptr Socket::create_unix_udp_socket() {
    Socket::ptr sock(new Socket(Family::Unix, Type::UDP, 0));
    return sock;
}

Socket::Socket(int family, int type, int protocol)
    : m_sockfd(-1)
    , m_family(family)
    , m_type(type)
    , m_protocol(protocol)
    , m_is_connected(false) {
}

Socket::~Socket() {
    // 析构时关闭套接字
    close();
}

void Socket::set_send_timeout(int64_t timeout) {
    // 设置socket的发送超时时间
    struct timeval tv {};
    tv.tv_sec = timeout / 1000;
    tv.tv_usec = timeout % 1000 * 1000;
    set_option(SOL_SOCKET, SO_SNDTIMEO, tv);
}

uint64_t Socket::get_send_timeout() const {
    // 获取socket的发送超时时间
    FdCtx::ptr ctx = FdMgr::instance()->get(m_sockfd);
    if (ctx) {
        return ctx->get_timeout(SO_SNDTIMEO);
    }
    return -1;
}

void Socket::set_recv_timeout(int64_t timeout) {
    struct timeval tv {};
    tv.tv_sec = timeout / 1000;
    tv.tv_usec = timeout % 1000 * 1000;
    set_option(SOL_SOCKET, SO_RCVTIMEO, tv);
}

uint64_t Socket::get_recv_timeout() const {
    FdCtx::ptr ctx = FdMgr::instance()->get(m_sockfd);
    if (ctx) {
        return ctx->get_timeout(SO_RCVTIMEO);
    }
    return -1;
}

bool Socket::get_option(int level, int option, void *result, size_t *len) {
    int ret = getsockopt(m_sockfd, level, option, result, (socklen_t*)len);
    if (ret != 0) {
        LOG_DEBUG(logger) << "get_option sock=" << m_sockfd
                          << " level=" << level << " option=" << option
                          << " errno=" << errno
                          << " errstr=" << strerror(errno);
        return false;
    }
    return true;
}

bool Socket::set_option(int level, int option, const void *result, size_t len) {
    int ret = setsockopt(m_sockfd, level, option, result,
                         static_cast<socklen_t>(len));
    if (ret != 0) {
        LOG_DEBUG(logger) << "set_option sock=" << m_sockfd
                          << " level=" << level << " option=" << option
                          << " errno=" << errno
                          << " errstr=" << strerror(errno);
        return false;
    }
    return true;
}

Socket::ptr Socket::accept() {
    Socket::ptr sock(new Socket(m_family, m_type, m_protocol));
    // 在init时再去获取对端ip
    int new_sockfd = ::accept(m_sockfd, nullptr, nullptr);
    if (new_sockfd == -1) {
        LOG_ERROR(logger) << "accept(" << m_sockfd << ") errno=" << errno
                          << " errstr=" << strerror(errno);
        return nullptr;
    }

    // 接受连接后利用接受的sockfd初始化一个socket对象
    if (sock->init(new_sockfd)) {
        return sock;
    }
    return nullptr;
}

bool Socket::bind(const Address::ptr addr) {
    m_local_addr = addr;
    if (!is_valid()) {
        new_socket();
        if (!is_valid()) [[unlikely]] {
            return false;
        }
    }

    if (addr->get_family() != m_family) [[unlikely]] {
        LOG_ERROR(logger) << "bind sock.family(" << m_family << ") addr.family("
                          << addr->get_family()
                          << ") not equal, addr=" << addr->to_string();
        return false;
    }

    UnixAddress::ptr u_addr = std::dynamic_pointer_cast<UnixAddress>(addr);
    if (u_addr) {
        Socket::ptr sock = Socket::create_unix_tcp_socket();
        if (sock->connect(u_addr)) {
            return false;
        }
        else {
            FSUtil::unlink(u_addr->get_path(), true);
        }
    }

    if (::bind(m_sockfd, addr->get_addr(), addr->get_addr_len()) != 0) {
        LOG_ERROR(logger) << "bind error errrno=" << errno
                          << " errstr=" << strerror(errno);
        return false;
    }

    get_local_address();
    return true;
}

bool Socket::reconnect(uint64_t timeout_ms) {
    if (!m_remote_addr) {
        LOG_ERROR(logger) << "reconnect m_remoteAddress is null";
        return false;
    }
    // 重新连接远端， 先删除本地地址，因为下次连接自动分配的地址不一定相同
    m_local_addr.reset();
    return connect(m_remote_addr, timeout_ms);
}

bool Socket::connect(const Address::ptr addr, uint64_t timeout_ms) {
    m_remote_addr = addr;
    if (!is_valid()) {
        new_socket();
        if (!is_valid()) [[unlikely]] {
            return false;
        }
    }

    if (addr->get_family() != m_family) [[unlikely]] {
        LOG_ERROR(logger) << "connect sock.family(" << m_family
                          << ") addr.family(" << addr->get_family()
                          << ") not equal, addr=" << addr->to_string();
        return false;
    }

    if (timeout_ms == static_cast<uint64_t>(-1)) {
        if (::connect(m_sockfd, addr->get_addr(), addr->get_addr_len())) {
            LOG_ERROR(logger)
                << "sock=" << m_sockfd << " connect(" << addr->to_string()
                << ") error errno=" << errno << " errstr=" << strerror(errno);
            // 连接失败则释放上面创建的套接字
            close();
            return false;
        }
    }
    else {
        if (::connect_with_timeout(m_sockfd, addr->get_addr(),
                                   addr->get_addr_len(), timeout_ms)) {
            LOG_ERROR(logger)
                << "sock=" << m_sockfd << " connect(" << addr->to_string()
                << ") timeout=" << timeout_ms << " error errno=" << errno
                << " errstr=" << strerror(errno);
            close();
            return false;
        }
    }

    m_is_connected = true;
    get_remote_address();
    get_local_address();
    return true;
}

bool Socket::listen(int backlog) {
    if (!is_valid()) {
        LOG_ERROR(logger) << "listen error sock=-1";
        return false;
    }
    if (::listen(m_sockfd, backlog)) {
        LOG_ERROR(logger) << "listen error errno=" << errno
                          << " errstr=" << strerror(errno);
        return false;
    }
    return true;
}

bool Socket::close() {
    if (!m_is_connected && m_sockfd == -1) {
        return true;
    }

    m_is_connected = false;
    if (m_sockfd != -1) {
        ::close(m_sockfd);
        m_sockfd = -1;
    }
    return false;
}

ssize_t Socket::send(const void *buffer, size_t length, int flags) {
    if (is_connected()) {
        return ::send(m_sockfd, buffer, length, flags);
    }
    return -1;
}

ssize_t Socket::send(const iovec *buffers, size_t length, int flags) {
    if (is_connected()) {
        msghdr msg;
        memset(&msg, 0, sizeof(msg));
        msg.msg_iov = const_cast<iovec*>(buffers);
        msg.msg_iovlen = length;
        return ::sendmsg(m_sockfd, &msg, flags);
    }
    return -1;
}

ssize_t Socket::sendto(const void *buffer, size_t length, const Address::ptr to,
                       int flags) {
    if (is_connected()) {
        return ::sendto(m_sockfd, buffer, length, flags, to->get_addr(),
                        to->get_addr_len());
    }
    return -1;
}

ssize_t Socket::sendto(const iovec *buffers, size_t length,
                       const Address::ptr to, int flags) {
    if (is_connected()) {
        msghdr msg;
        memset(&msg, 0, sizeof(msg));
        msg.msg_iov = const_cast<iovec*>(buffers);
        msg.msg_iovlen = length;
        msg.msg_name = to->get_addr();
        msg.msg_namelen = to->get_addr_len();
        return ::sendmsg(m_sockfd, &msg, flags);
    }
    return -1;
}

ssize_t Socket::recv(void *buffer, size_t length, int flags) {
    if (is_connected()) {
        LOG_DEBUG(logger) << "Socket::recv from socket fd = " << m_sockfd;
        return ::recv(m_sockfd, buffer, length, flags);
    }
    return -1;
}

ssize_t Socket::recv(iovec *buffers, size_t length, int flags) {
    if (is_connected()) {
        msghdr msg;
        memset(&msg, 0, sizeof(msg));
        msg.msg_iov = buffers;
        msg.msg_iovlen = length;
        return ::recvmsg(m_sockfd, &msg, flags);
    }
    return -1;
}

ssize_t Socket::recvfrom(void *buffer, size_t length, Address::ptr from,
                         int flags) {
    if (is_connected()) {
        socklen_t len = from->get_addr_len();
        return ::recvfrom(m_sockfd, buffer, length, flags, from->get_addr(),
                          &len);
    }
    return -1;
}

ssize_t Socket::recvfrom(iovec *buffers, size_t length, Address::ptr from,
                         int flags) {
    if (is_connected()) {
        msghdr msg;
        memset(&msg, 0, sizeof(msg));
        msg.msg_iov = buffers;
        msg.msg_iovlen = length;
        msg.msg_name = from->get_addr();
        msg.msg_namelen = from->get_addr_len();
        return ::recvmsg(m_sockfd, &msg, flags);
    }
    return -1;
}

Address::ptr Socket::get_remote_address() {
    if (m_remote_addr) {
        return m_remote_addr;
    }

    Address::ptr result;
    switch (m_family) {
        case Family::IPv4:
            result.reset(new IPv4Address);
            break;
        case Family::IPv6:
            result.reset(new IPv6Address);
            break;
        default:
            break;
    }

    socklen_t addr_len = result->get_addr_len();
    // 获取对端的IP地址
    if (getpeername(m_sockfd, result->get_addr(), &addr_len)) {
        LOG_ERROR(logger) << "getpeername error sock=" << m_sockfd
                          << " errno=" << errno
                          << " errstr=" << strerror(errno);
        return nullptr;
    }
    m_remote_addr = result;
    return m_remote_addr;
}

Address::ptr Socket::get_local_address() {
    if (m_local_addr) {
        return m_local_addr;
    }

    Address::ptr result;
    switch (m_family) {
        case Family::IPv4:
            result.reset(new IPv4Address);
            break;
        case Family::IPv6:
            result.reset(new IPv6Address);
            break;
        default:
            break;
    }

    socklen_t addr_len = result->get_addr_len();
    // 获取当前socket的IP地址
    if (getsockname(m_sockfd, result->get_addr(), &addr_len)) {
        LOG_ERROR(logger) << "getsockname error sock=" << m_sockfd
                          << " errno=" << errno
                          << " errstr=" << strerror(errno);
        return nullptr;
    }

    m_local_addr = result;
    return m_local_addr;
}

int Socket::get_error() {
    int error = 0;
    if (!get_option(SOL_SOCKET, SO_ERROR, error)) {
        error = errno;
    }
    return error;
}

std::ostream &Socket::dump(std::ostream &os) const {
    os << "[Socket sockfd=" << m_sockfd << " is_connected=" << m_is_connected
       << " family=" << m_family << " type=" << m_type
       << " protocol=" << m_protocol;
    if (m_local_addr) {
        os << " local_address=" << m_local_addr->to_string();
    }
    if (m_remote_addr) {
        os << " remote_address=" << m_remote_addr->to_string();
    }
    os << "]";
    return os;
}

std::string Socket::to_string() const {
    std::stringstream ss;
    dump(ss);
    return ss.str();
}

bool Socket::cancel_read() {
    return IOManager::get_this()->cancel_event(m_sockfd, IOManager::READ);
}

bool Socket::cancel_write() {
    return IOManager::get_this()->cancel_event(m_sockfd, IOManager::WRITE);
}

bool Socket::cancel_accept() {
    return IOManager::get_this()->cancel_event(m_sockfd, IOManager::READ);
}

bool Socket::cancel_all() {
    return IOManager::get_this()->cancel_all(m_sockfd);
}

void Socket::init_socket() {
    int val = 1;
    set_option(SOL_SOCKET, SO_REUSEADDR, val);
    if (m_type == TCP) {
        set_option(IPPROTO_TCP, TCP_NODELAY, val);
    }
}

void Socket::new_socket() {
    m_sockfd = socket(m_family, m_type, m_protocol);
    if (m_sockfd != -1) {
        init_socket();
    }
    else {
        LOG_ERROR(logger) << "socket(" << m_family << ", " << m_type << ", "
                          << m_protocol << ") errno=" << errno
                          << " errstr=" << strerror(errno);
    }
}

bool Socket::init(int sock) {
    FdCtx::ptr ctx = FdMgr::instance()->get(sock);
    if (ctx && ctx->is_socket() && !ctx->is_close()) {
        m_sockfd = sock;
        m_is_connected = true;
        init_socket();
        get_local_address();
        get_remote_address();
        return true;
    }
    return false;
}

// 重载流输出， 方便输出socket信息
std::ostream &operator<<(std::ostream &os, const Socket &sock) {
    return sock.dump(os);
}

}  // namespace acid
