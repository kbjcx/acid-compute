#include "socket_stream.h"

#include "acid/logger/logger.h"

namespace acid {

static auto logger = GET_LOGGER_BY_NAME("system");

SocketStream::SocketStream(Socket::ptr socket, bool owner)
    : m_socket(socket), m_owner(owner) {
}

SocketStream::~SocketStream() {
    if (m_owner && m_socket) {
        m_socket->close();
    }
}

bool SocketStream::is_connected() const {
    return m_socket && m_socket->is_connected();
}

size_t SocketStream::read(void *buffer, size_t length) {
    if (!is_connected()) {
        LOG_INFO(logger) << "socket is not connected";
        return -1;
    }
    return m_socket->recv(buffer, length);
}

size_t SocketStream::read(ByteArray::ptr byte_array, size_t length) {
    if (!is_connected()) {
        return -1;
    }
    std::vector<iovec> iovs;
    byte_array->get_write_buffers(iovs, length);
    ssize_t ret = m_socket->recv(&iovs[0], iovs.size());
    if (ret > 0) {
        byte_array->set_position(byte_array->get_position() + ret);
    }
    return ret;
}

size_t SocketStream::write(const void *buffer, size_t length) {
    if (!is_connected()) {
        return -1;
    }

    return m_socket->send(buffer, length);
}

size_t SocketStream::write(ByteArray::ptr byte_array, size_t length) {
    if (!is_connected()) {
        return -1;
    }

    std::vector<iovec> iovs;
    byte_array->get_read_buffers(iovs, length);
    ssize_t ret = m_socket->send(&iovs[0], iovs.size());
    if (ret > 0) {
        byte_array->set_position(byte_array->get_position() + ret);
    }
    return ret;
}

void SocketStream::close() {
    if (m_socket) {
        m_socket->close();
    }
}

Address::ptr SocketStream::get_remote_address() {
    if (m_socket) {
        return m_socket->get_remote_address();
    }
    return nullptr;
}

Address::ptr SocketStream::get_local_address() {
    if (m_socket) {
        return m_socket->get_local_address();
    }
    return nullptr;
}

std::string SocketStream::get_remote_address_string() {
    auto addr = get_remote_address();
    if (addr) {
        return addr->to_string();
    }
    return {};
}

std::string SocketStream::get_local_address_string() {
    auto addr = get_local_address();
    if (addr) {
        return addr->to_string();
    }
    return {};
}

}  // namespace acid