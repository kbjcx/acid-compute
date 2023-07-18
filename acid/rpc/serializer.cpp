#include "serializer.h"

namespace acid::rpc {

Serializer::Serializer() : m_byte_array(std::make_shared<ByteArray>()) {

}

Serializer::Serializer(ByteArray::ptr byte_array) : m_byte_array(byte_array) {

}

Serializer::Serializer(const std::string& in) : m_byte_array(std::make_shared<ByteArray>()) {
    write_raw_data(in.c_str(), in.size());
    reset();
}

Serializer::Serializer(const char* in, int len) : m_byte_array(std::make_shared<ByteArray>()) {
    write_raw_data(in, len);
    reset();
}

} // namespace acid::rpc