#include "rpc_session.h"

namespace acid::rpc {

RpcSession::RpcSession(Socket::ptr socket, bool owner) : SocketStream(socket, owner) {
}

Protocol::ptr RpcSession::recv_protocol() {
    Protocol::ptr protocol = std::make_shared<Protocol>();
    ByteArray::ptr byte_array = std::make_shared<ByteArray>();

    if (read_fix_size(byte_array, protocol->BASE_LENGTH) <= 0) {
        return nullptr;
    }

    byte_array->set_position(0);
    protocol->decode_meta(byte_array);

    if (protocol->get_magic() != Protocol::MAGIC) {
        return nullptr;
    }

    if (!protocol->get_content_length()) {
        return protocol;
    }

    std::string buffer;
    buffer.resize(protocol->get_content_length());

    if (read_fix_size(buffer.data(), protocol->get_content_length()) <= 0) {
        return nullptr;
    }

    protocol->set_content(buffer);
    return protocol;
}

ssize_t RpcSession::send_protocol(Protocol::ptr protocol) {
    ByteArray::ptr byte_array = protocol->encode();
    MutexType::Lock lock(m_mutex);
    return write_fix_size(byte_array, byte_array->get_size());
}

}  // namespace acid::rpc
