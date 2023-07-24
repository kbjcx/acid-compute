#include "rpc_session.h"

namespace acid::rpc {

RpcSession::RpcSession(Socket::ptr socket, bool owner) : SocketStream(socket, owner) {
}

Protocol::ptr RpcSession::recv_protocol() {
    Protocol::ptr protocol = std::make_shared<Protocol>();
    ByteArray::ptr byte_array = std::make_shared<ByteArray>();

    // 以序列化的格式读取协议长度的元数据
    if (read_fix_size(byte_array, protocol->BASE_LENGTH) <= 0) {
        return nullptr;
    }

    // 指针移动到协议头
    byte_array->set_position(0);
    // 解析协议元数据
    protocol->decode_meta(byte_array);

    // 判断魔法数是否合法
    if (protocol->get_magic() != Protocol::MAGIC) {
        return nullptr;
    }

    // 文本长度为0, 怎没有文本消息, 直接返回解析到的协议
    if (!protocol->get_content_length()) {
        return protocol;
    }

    std::string buffer;
    buffer.resize(protocol->get_content_length());

    // 读取剩余文本消息
    if (read_fix_size(buffer.data(), protocol->get_content_length()) <= 0) {
        return nullptr;
    }

    // 设置协议文本
    protocol->set_content(buffer);
    return protocol;
}

ssize_t RpcSession::send_protocol(Protocol::ptr protocol) {
    ByteArray::ptr byte_array = protocol->encode();
    MutexType::Lock lock(m_mutex);
    // 利用socketstream发送出去
    return write_fix_size(byte_array, byte_array->get_size());
}

}  // namespace acid::rpc
