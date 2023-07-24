#include "protocol.h"

#include <sstream>

namespace acid::rpc {

Protocol::ptr Protocol::create(MessageType type, const std::string& content, uint32_t id) {
    Protocol::ptr protocol = std::make_shared<Protocol>();
    protocol->set_message_type(type);
    protocol->set_content(content);
    protocol->set_content_length(content.size());
    protocol->set_sequence_id(id);
    return protocol;
}

Protocol::ptr Protocol::heartbeat() {
    static Protocol::ptr s_heartbeat =
        Protocol::create(Protocol::MessageType::HEARTBEAT_PACKET, "");
    return s_heartbeat;
}

ByteArray::ptr Protocol::encode_meta() {
    ByteArray::ptr byte_array = std::make_shared<ByteArray>();
    byte_array->write_fix_uint8(m_magic);
    byte_array->write_fix_uint8(m_version);
    byte_array->write_fix_uint8(m_type);
    byte_array->write_fix_uint32(m_sequence_id);
    byte_array->write_fix_uint32(m_content.size());
    byte_array->set_position(0);
    return byte_array;
}

ByteArray::ptr Protocol::encode() {
    ByteArray::ptr byte_array = std::make_shared<ByteArray>();
    byte_array->write_fix_uint8(m_magic);
    byte_array->write_fix_uint8(m_version);
    byte_array->write_fix_uint8(m_type);
    byte_array->write_fix_uint32(m_sequence_id);
    byte_array->write_string_f32(m_content);
    byte_array->set_position(0);
    return byte_array;
}

void Protocol::decode_meta(ByteArray::ptr byte_array) {
    m_magic = byte_array->read_fix_uint8();
    m_version = byte_array->read_fix_uint8();
    m_type = byte_array->read_fix_uint8();
    m_sequence_id = byte_array->read_fix_uint32();
    m_content_length = byte_array->read_fix_uint32();
}

void Protocol::decode(ByteArray::ptr byte_array) {
    m_magic = byte_array->read_fix_uint8();
    m_version = byte_array->read_fix_uint8();
    m_type = byte_array->read_fix_uint8();
    m_sequence_id = byte_array->read_fix_uint32();
    m_content = byte_array->read_string_f32();
    m_content_length = m_content.size();
}

std::string Protocol::to_string() {
    std::stringstream ss;
    // clang-format off
    ss  << "[ magic = " << m_magic 
        << " version = " << m_version 
        << " type = " << m_type
        << " sequence id = " << m_sequence_id
        << " length = " << m_content_length
        << " content = " << m_content << "]";
    // clang-format on
    return ss.str();
}

};  // namespace acid::rpc
