//
// Created by llz on 23-7-3.
//

#ifndef DF_BYTE_ARRAY_H
#define DF_BYTE_ARRAY_H

#include "acid/common/util.h"

#include <memory>
#include <stdint.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <vector>

namespace acid {

/*!
 * @brief 二进制数组， 提供基础类型的序列化与反序列化
 */
class ByteArray {
public:
    using ptr = std::shared_ptr<ByteArray>;

    /*!
     * @brief 存储节点
     */
    struct Node {
        // 构造指定大小的内存块
        Node(size_t size);

        Node();

        ~Node();

        char* ptr;    // 内存块指针
        Node* next;   // 下一个内存块的地址
        size_t size;  // 内存块大小
    };

    // 使用指定长度的内存块构建bytearray
    ByteArray(size_t base_size = 4096, std::endian en = std::endian::big);

    ~ByteArray();

    template <class T>
    void write_fix_int(T value) {
        // 保存类型不等于主机字节序的话需要进行转化
        if (m_endian != std::endian::native) {
            value = byte_swap(value);
        }
        write(&value, sizeof(value));
    }

    // 写入固定长度int8_t的数据，
    void write_fix_int8(int8_t value);

    // 写入固定长度uint8_t的数据
    void write_fix_uint8(uint8_t value);

    // 写入固定长度int16的数据
    void write_fix_int16(int16_t value);

    // 写入固定长度uint16_t的数据
    void write_fix_uint16(uint16_t value);

    // 写入固定长度int32_t的数据
    void write_fix_int32(int32_t value);

    // 写入固定长度uint32_t的数据
    void write_fix_uint32(uint32_t value);

    // 写入固定长度int64_t的数据
    void write_fix_int64(int64_t value);

    // 写入固定长度uint64_t的数据
    void write_fix_uint64(uint64_t value);

    // 写入动态长度的int32_t的数据，实际长度不一定为32位
    void write_var_int32(int32_t value);

    void write_var_uint32(uint32_t value);

    void write_var_int64(int64_t value);

    void write_var_uint64(uint64_t value);

    void write_float(float value);

    void write_double(double value);

    // 写入string类型数据，用int16_t作为长度标记
    void write_string_f16(const std::string& value);

    void write_string_f32(const std::string& value);

    void write_string_f64(const std::string& value);

    void write_string_vint(const std::string& value);

    void write_string_without_length(const std::string& value);

    int8_t read_fix_int8();

    uint8_t read_fix_uint8();

    int16_t read_fix_int16();

    uint16_t read_fix_uint16();

    int32_t read_fix_int32();

    uint32_t read_fix_uint32();

    int64_t read_fix_int64();

    uint64_t read_fix_uint64();

    int32_t read_var_int32();

    uint32_t read_var_uint32();

    int64_t read_var_int64();

    uint64_t read_var_uint64();

    float read_float();

    double read_double();

    std::string read_string_f16();

    std::string read_string_f32();

    std::string read_string_f64();

    std::string read_string_vint();

    // 清空bytearray
    void clear();

    // 写入size大小的数据
    void write(const void* buffer, size_t size);

    // 读取size大小的数据
    void read(void* buffer, size_t size);

    // 从指定位置读取size大小的数据
    void read(void* buffer, size_t size, size_t position) const;

    size_t get_position() const {
        return m_position;
    }

    void set_position(size_t pos);

    // 把bytearray写入文件
    bool write_to_file(const std::string& filename) const;

    // 从文件读取数据
    bool read_from_file(const std::string& filename);

    // 返回内存块的大小
    size_t get_base_size() const {
        return m_base_size;
    }

    // 返回可读数据的大小
    size_t get_read_size() const {
        return m_size - m_position;
    }

    bool is_little_endian() const {
        return m_endian == std::endian::little;
    }

    void set_is_little_endian(bool is_little) {
        if (is_little) {
            m_endian = std::endian::little;
        }
        else {
            m_endian = std::endian::big;
        }
    }

    // 将bytearray里面的数据转为string（m_position到m_size）
    std::string to_string();

    // 将bytearray的内容转为16进制的string
    std::string to_hex_string();

    /*!
     * @brief 获取可读取的缓存，保存为iovec数组
     * @param[out] buffers
     * @param[in] len 读取数据的长度
     * @return 实际读取的长度
     */
    uint64_t get_read_buffers(std::vector<iovec>& buffers, uint64_t len = ~0ull) const;

    /*!
     * @brief 获取可读取的缓存，保存为iovec数组
     * @param[out] buffers
     * @param[in] len 读取数据的长度
     * @param[in] position 读取数据的位置
     * @return 实际读取的长度
     */
    uint64_t get_read_buffers(std::vector<iovec>& buffers, uint64_t len, uint64_t position) const;

    /**
     * @brief 获取可写入的缓存,保存成iovec数组
     * @param[out] buffers 保存可写入的内存的iovec数组
     * @param[in] len 写入的长度
     * @return 返回实际的长度
     * @post 如果(m_position + len) > m_capacity 则 m_capacity扩容N个节点以容纳len长度
     */
    uint64_t get_write_buffers(std::vector<iovec>& buffers, uint64_t len);

    // 返回数据的长度
    size_t get_size() const {
        return m_size;
    }

private:
    // 扩容使其能容纳size个数据
    void add_capacity(size_t size);

    // 获取当前的剩余容量
    size_t get_capacity() const {
        return m_capacity - m_position;
    }

private:
    size_t m_base_size;
    size_t m_position;
    size_t m_capacity;
    size_t m_size;
    std::endian m_endian;  // 默认使用大端序
    Node* m_root;
    Node* m_cur;
};

}  // namespace acid

#endif  // DF_BYTE_ARRAY_H
