//
// Created by llz on 23-7-3.
//

#include "byte_array.h"

#include "acid/logger/logger.h"
#include "acid/net/address.h"

#include <iomanip>

namespace acid {

static auto logger = GET_LOGGER_BY_NAME("system");

ByteArray::Node::Node(size_t size) : ptr(new char[size]), next(nullptr), size(size) {
}

ByteArray::Node::Node() : ptr(nullptr), next(nullptr), size(0) {
}

ByteArray::Node::~Node() {
    if (ptr) {
        delete[] ptr;
    }
}

ByteArray::ByteArray(size_t base_size, std::endian en)
    : m_base_size(base_size)
    , m_position(0)
    , m_capacity(base_size)
    , m_size(0)
    , m_endian(en)
    , m_root(new Node(base_size))
    , m_cur(m_root) {
}

ByteArray::~ByteArray() {
    Node* tmp = m_root;
    while (tmp) {
        m_cur = tmp;
        tmp = tmp->next;
        delete m_cur;
    }
}

void ByteArray::write_fix_int8(int8_t value) {
    write(&value, sizeof(value));
}

void ByteArray::write_fix_uint8(uint8_t value) {
    write(&value, sizeof(value));
}

void ByteArray::write_fix_int16(int16_t value) {
    write_fix_int(value);
}

void ByteArray::write_fix_uint16(uint16_t value) {
    write_fix_int(value);
}

void ByteArray::write_fix_int32(int32_t value) {
    write_fix_int(value);
}

void ByteArray::write_fix_uint32(uint32_t value) {
    write_fix_int(value);
}

void ByteArray::write_fix_int64(int64_t value) {
    write_fix_int(value);
}

void ByteArray::write_fix_uint64(uint64_t value) {
    write_fix_int(value);
}

// zigzag编码，解决varint对负数编码效率低的问题，因为负数的符号位为1,无法进行压缩
// 因此zigzag利用变换将符号位转为最低位，使得能够最大程度上压缩int
static uint32_t encode_zigzag32(const int32_t& value) {
    return static_cast<uint32_t>((value << 1) ^ (value >> 31));
}

static uint64_t encode_zigzag64(const int64_t& value) {
    return static_cast<uint64_t>((value << 1) ^ (value >> 63));
}

static int32_t decode_zigzag32(const uint32_t& value) {
    return static_cast<int32_t>((value >> 1) ^ (-(value & 1)));
}

static int64_t decode_zigzag64(const uint64_t& value) {
    return static_cast<int64_t>((value >> 1) ^ (-(value & 1)));
}

void ByteArray::write_var_int32(int32_t value) {
    write_var_uint32(encode_zigzag32(value));
}

void ByteArray::write_var_uint32(uint32_t value) {
    uint8_t tmp[5];
    uint8_t i = 0;
    // 1000,0000, 每个数字存7位原数字，第一位表示最高有效位，
    // 表示下一字节是否属于当前数字
    while (value >= 0x80) {
        tmp[i++] = (value & 0x7F) | 0x80;
        value >>= 7;
    }
    tmp[i++] = value;
    write(tmp, i);
}

void ByteArray::write_var_int64(int64_t value) {
    write_var_uint64(encode_zigzag64(value));
}

void ByteArray::write_var_uint64(uint64_t value) {
    uint8_t tmp[10];
    uint8_t i = 0;
    while (value >= 0x80) {
        tmp[i++] = (value & 0x7F) | 0x80;
        value >>= 7;
    }
    tmp[i++] = value;
    write(tmp, i);
}

void ByteArray::write_float(float value) {
    uint32_t v;
    memcpy(&v, &value, sizeof(value));
    write_fix_uint32(v);
}

void ByteArray::write_double(double value) {
    uint64_t v;
    memcpy(&v, &value, sizeof(value));
    write_fix_uint64(v);
}

void ByteArray::write_string_f16(const std::string& value) {
    // 先写入长度，再写入string
    write_fix_uint16(value.size());
    write(value.c_str(), value.size());
}

void ByteArray::write_string_f32(const std::string& value) {
    write_fix_uint32(value.size());
    write(value.c_str(), value.size());
}

void ByteArray::write_string_f64(const std::string& value) {
    write_fix_uint64(value.size());
    write(value.c_str(), value.size());
}

void ByteArray::write_string_vint(const std::string& value) {
    write_var_uint64(value.size());
    write(value.c_str(), value.size());
}

void ByteArray::write_string_without_length(const std::string& value) {
    write(value.c_str(), value.size());
}

int8_t ByteArray::read_fix_int8() {
    int8_t v;
    read(&v, sizeof(v));
    return v;
}

uint8_t ByteArray::read_fix_uint8() {
    uint8_t v;
    read(&v, sizeof(v));
    return v;
}

#define XX(type)                           \
    type v;                                \
    read(&v, sizeof(v));                   \
    if (m_endian == std::endian::native) { \
        return v;                          \
    }                                      \
    else {                                 \
        return byte_swap(v);               \
    }

int16_t ByteArray::read_fix_int16() {
    XX(int16_t);
}

uint16_t ByteArray::read_fix_uint16() {
    XX(uint16_t);
}

int32_t ByteArray::read_fix_int32() {
    XX(int32_t);
}

uint32_t ByteArray::read_fix_uint32() {
    XX(uint32_t);
}

int64_t ByteArray::read_fix_int64() {
    XX(int64_t);
}

uint64_t ByteArray::read_fix_uint64() {
    XX(uint64_t);
}
#undef XX

int32_t ByteArray::read_var_int32() {
    return decode_zigzag32(read_var_uint32());
}

uint32_t ByteArray::read_var_uint32() {
    uint32_t result = 0;
    for (int i = 0; i < 32; i += 7) {
        uint8_t tmp = read_fix_uint8();
        if (tmp < 0x80) {
            // 第一位是0, 说明该字节是最后一个字节，越往后越是高位字节
            result |= static_cast<uint32_t>(tmp) << i;
            break;
        }
        else {
            result |= static_cast<uint32_t>(tmp & 0x7F) << i;
        }
    }
    return result;
}

int64_t ByteArray::read_var_int64() {
    return decode_zigzag64(read_var_uint64());
}

uint64_t ByteArray::read_var_uint64() {
    uint64_t result = 0;
    for (int i = 0; i < 64; i += 7) {
        uint8_t tmp = read_fix_uint8();
        if (tmp < 0x80) {
            result |= static_cast<uint64_t>(tmp) << i;
            break;
        }
        else {
            result |= static_cast<uint64_t>(tmp & 0x7F) << i;
        }
    }
    return result;
}

float ByteArray::read_float() {
    uint32_t v = read_fix_uint32();
    float result;
    memcpy(&result, &v, sizeof(v));
    return result;
}

double ByteArray::read_double() {
    uint64_t v = read_fix_uint64();
    double result;
    memcpy(&result, &v, sizeof(v));
    return result;
}

std::string ByteArray::read_string_f16() {
    uint16_t len = read_fix_uint16();
    std::string result;
    result.resize(len);
    read(&result[0], len);
    return result;
}

std::string ByteArray::read_string_f32() {
    uint32_t len = read_fix_uint32();
    std::string result;
    result.resize(len);
    read(&result[0], len);
    return result;
}

std::string ByteArray::read_string_f64() {
    uint64_t len = read_fix_uint64();
    std::string result;
    result.resize(len);
    read(&result[0], len);
    return result;
}

std::string ByteArray::read_string_vint() {
    uint64_t len = read_var_uint64();
    std::string result;
    result.resize(len);
    read(&result[0], len);
    return result;
}

void ByteArray::clear() {
    m_position = m_size = 0;
    m_capacity = m_base_size;
    Node* tmp = m_root->next;
    while (tmp) {
        m_cur = tmp;
        tmp = tmp->next;
        delete m_cur;
    }
    m_cur = m_root;
    m_root->next = nullptr;
}

void ByteArray::write(const void* buffer, size_t size) {
    if (size == 0) {
        return;
    }

    // 如果容量不够则扩容，不涉及拷贝扩容，不必扩容太多
    add_capacity(size);

    size_t npos = m_position % m_base_size;  // Node的偏移量
    size_t ncap = m_cur->size - npos;        // Node的剩余容量
    size_t bpos = 0;                         // buffer的写指针

    while (size > 0) {
        if (ncap >= size) {
            memcpy(m_cur->ptr + npos, (const char*) buffer + bpos, size);
            if (ncap == size) {
                m_cur = m_cur->next;
            }
            m_position += size;
            break;
        }
        else {
            memcpy(m_cur->ptr + npos, (const char*) buffer + bpos, ncap);
            m_cur = m_cur->next;
            m_position += ncap;
            bpos += ncap;
            size -= ncap;
            npos = 0;
            ncap = m_cur->size;
        }
    }

    if (m_position > m_size) {
        m_size = m_position;
    }
}

void ByteArray::read(void* buffer, size_t size) {
    if (size > get_read_size()) {
        // 没有足够的数据可读
        throw std::out_of_range("not enough len");
    }

    size_t npos = m_position % m_base_size;
    size_t ncap = m_cur->size - m_position;
    size_t bpos = 0;

    while (size > 0) {
        if (ncap >= size) {
            memcpy((char*) buffer + bpos, m_cur->ptr + npos, size);
            if (ncap == size) {
                m_cur = m_cur->next;
            }
            m_position += size;
            break;
        }
        else {
            memcpy((char*) buffer + bpos, m_cur->ptr + npos, ncap);
            m_position += ncap;
            bpos += ncap;
            size -= ncap;
            m_cur = m_cur->next;
            npos = 0;
            ncap = m_cur->size;
        }
    }
}

void ByteArray::read(void* buffer, size_t size, size_t position) const {
    if (size > m_size - position) {
        throw std::out_of_range("not enough len");
    }

    size_t npos = position % m_base_size;
    size_t ncap = m_cur->size - npos;
    size_t bpos = 0;
    // TODO 不该是当前Node吧
    Node* cur = m_cur;
    while (size > 0) {
        if (ncap >= size) {
            memcpy((char*) buffer + bpos, cur->ptr + npos, size);
            break;
        }
        else {
            memcpy((char*) buffer + bpos, cur->ptr + npos, ncap);
            position += ncap;
            bpos += ncap;
            size -= ncap;
            cur = cur->next;
            npos = 0;
            ncap = cur->size;
        }
    }
}

void ByteArray::set_position(size_t pos) {
    if (pos > m_capacity) {
        throw std::out_of_range("set_position out of range");
    }

    m_position = pos;
    if (m_position > m_size) {
        m_size = m_position;
    }

    m_cur = m_root;

    while (pos > m_cur->size) {
        pos -= m_cur->size;
        m_cur = m_cur->next;
    }
    if (pos == m_cur->size) {
        m_cur = m_cur->next;
    }
}

bool ByteArray::write_to_file(const std::string& filename) const {
    std::ofstream ofs;
    // std::ios::trunc表示打开前清空文件，不存在则创建文件
    ofs.open(filename, std::ios::trunc | std::ios::binary);
    if (!ofs) {
        LOG_ERROR(logger) << "writeToFile name=" << filename << " error , errno=" << errno
                          << " errstr=" << strerror(errno);
        return false;
    }
    int64_t read_size = static_cast<int64_t>(get_read_size());
    int64_t pos = static_cast<int64_t>(m_position);
    Node* cur = m_cur;

    while (read_size > 0) {
        int64_t diff = pos % static_cast<int64_t>(m_base_size);
        int64_t len =
            (read_size > static_cast<int64_t>(m_base_size) ? static_cast<int64_t>(m_base_size)
                                                           : read_size) -
            diff;
        ofs.write(cur->ptr + diff, len);
        cur = cur->next;
        pos += len;
        read_size -= len;
    }
    return true;
}

bool ByteArray::read_from_file(const std::string& filename) {
    std::ifstream ifs;
    ifs.open(filename, std::ios::binary);
    if (!ifs) {
        LOG_ERROR(logger) << "readFromFile name=" << filename << " error, errno=" << errno
                          << " errstr=" << strerror(errno);
        return false;
    }
    std::shared_ptr<char> buffer(new char[m_base_size], [](const char* ptr) { delete[] ptr; });
    while (!ifs.eof()) {
        ifs.read(buffer.get(), m_base_size);
        // 分多次将文件写入Byterray Node缓存
        write(buffer.get(), ifs.gcount());
    }
    return true;
}

void ByteArray::add_capacity(size_t size) {
    if (size == 0) {
        return;
    }
    // 剩余容量
    size_t old_capacity = get_capacity();
    if (old_capacity > size) {
        return;
    }

    size = size - old_capacity;
    size_t count = (size + m_base_size - 1) / m_base_size;
    Node* tmp = m_root;
    while (tmp->next) {
        tmp = tmp->next;
    }

    Node* first = nullptr;
    for (size_t i = 0; i < count; ++i) {
        tmp->next = new Node(m_base_size);
        if (first == nullptr) {
            first = tmp->next;
        }
        tmp = tmp->next;
        m_capacity += m_base_size;
    }
    if (old_capacity == 0) {
        m_cur = first;
    }
}

std::string ByteArray::to_string() {
    std::string result;
    result.resize(get_read_size());
    if (result.empty()) {
        return result;
    }
    read(&result[0], result.size(), m_position);
    return result;
}

std::string ByteArray::to_hex_string() {
    std::string result = to_string();
    std::stringstream ss;
    for (size_t i = 0; i < result.size(); ++i) {
        if (i > 0 && i % 32 == 0) {
            ss << std::endl;
        }
        // std::hex只会输出有效的十六进制位，忽略高位0, 利用std::setw(2) <<
        // std::setfill('0')补全两位
        ss << std::setw(2) << std::setfill('0') << std::hex << (int) (uint8_t) result[i] << " ";
    }
    return ss.str();
}

uint64_t ByteArray::get_read_buffers(std::vector<iovec>& buffers, uint64_t len) const {
    len = len > get_read_size() ? get_read_size() : len;
    if (len == 0) {
        return 0;
    }

    uint64_t size = len;

    size_t npos = m_position % m_base_size;
    size_t ncap = m_cur->size - npos;
    struct iovec iov;
    Node* cur = m_cur;

    while (len > 0) {
        if (ncap >= len) {
            iov.iov_base = cur->ptr + npos;
            iov.iov_len = len;
            break;
        }
        else {
            iov.iov_base = cur->ptr + npos;
            iov.iov_len = ncap;
            len -= ncap;
            cur = cur->next;
            ncap = cur->size;
            npos = 0;
        }
        buffers.push_back(iov);
    }
    return size;
}

uint64_t ByteArray::get_read_buffers(std::vector<iovec>& buffers, uint64_t len,
                                     uint64_t position) const {
    len = len > get_read_size() ? get_read_size() : len;
    if (len == 0) {
        return 0;
    }
    uint64_t size = len;

    size_t npos = position % m_base_size;
    size_t count = position / m_base_size;
    Node* cur = m_cur;

    while (count > 0) {
        cur = cur->next;
        --count;
    }

    size_t ncap = cur->size - npos;
    struct iovec iov;

    while (len > 0) {
        if (ncap >= len) {
            iov.iov_base = cur->ptr + npos;
            iov.iov_len = len;
            break;
        }
        else {
            iov.iov_base = cur->ptr + npos;
            iov.iov_len = ncap;
            len -= ncap;
            cur = cur->next;
            ncap = cur->size;
            npos = 0;
        }
        buffers.push_back(iov);
    }
    return size;
}

uint64_t ByteArray::get_write_buffers(std::vector<iovec>& buffers, uint64_t len) {
    if (len == 0) {
        return 0;
    }

    add_capacity(len);
    uint64_t size = len;

    size_t npos = m_position % m_base_size;
    size_t ncap = m_cur->size - npos;
    struct iovec iov;
    Node* cur = m_cur;

    while (len > 0) {
        if (ncap >= len) {
            iov.iov_base = cur->ptr + npos;
            iov.iov_len = len;
            break;
        }
        else {
            iov.iov_base = cur->ptr + npos;
            iov.iov_len = ncap;
            len -= ncap;
            cur = cur->next;
            ncap = cur->size;
            npos = 0;
        }
        buffers.push_back(iov);
    }
    return size;
}

}  // namespace acid