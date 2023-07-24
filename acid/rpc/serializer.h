/**
 * @file serializer.h
 * @author kbjcx (lulu5v@163.com)
 * @brief
 * @version 0.1
 * @date 2023-07-18
 *
 * @copyright Copyright (c) 2023
 *
 */

#ifndef ACID_SERIALIZER_H
#define ACID_SERIALIZER_H

#include "acid/common/byte_array.h"
#include "protocol.h"

#include <algorithm>
#include <cstdint>
#include <list>
#include <map>
#include <set>
#include <sstream>
#include <cstring>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace acid::rpc {

/**
 * @brief RPC 序列化与反序列化包装, 会自动进行网络序的转换
 * @details 序列化有以下规则:
 * 1.默认情况下序列化, 8, 16为类型以及浮点数不压缩, 32,
 * 64位有符号和无符号整数采用zigzag和varints编码压缩 2.针对std::string会将长度信息压缩到头部
 * 3.调用write_fix_int将不会压缩数字, 调用write_raw_data时不会加入长度信息
 * 支持标准库容器序列化:
 * 顺序容器: string, list, vector
 * 关联容器: set, multiset, map, multimap
 * 无序容器: unordered_set, unordered_multiset, unordered_mao, unordered_multimap
 * 异构容器: tulpe
 *
 */
class Serializer {
public:
    using ptr = std::shared_ptr<Serializer>;

    Serializer() : m_byte_array(std::make_shared<ByteArray>()) {
    }

    explicit Serializer(ByteArray::ptr byte_array) : m_byte_array(byte_array) {
    }

    explicit Serializer(const std::string& in) : m_byte_array(std::make_shared<ByteArray>()) {
        write_raw_data(in.c_str(), in.size());
        reset();
    }

    Serializer(const char* in, int len) : m_byte_array(std::make_shared<ByteArray>()) {
        write_raw_data(in, len);
        reset();
    }

    // 获取byte_array的size
    int size() {
        return m_byte_array->get_size();
    }

    /**
     * @brief 重置偏移, 从头开始读
     *
     */
    void reset() {
        m_byte_array->set_position(0);
    }

    // 移动byte_array的position指针
    void offset(int off) {
        int old = m_byte_array->get_position();
        m_byte_array->set_position(old + off);
    }

    // 把剩余序列化数据转化为string
    std::string to_string() {
        return m_byte_array->to_string();
    }

    // 获取bye_array
    ByteArray::ptr get_byte_array() {
        return m_byte_array;
    }

    /**
     * @brief 写入原始数据, 不加入长度信息
     *
     * @param in
     * @param len
     */
    void write_raw_data(const char* in, int len) {
        m_byte_array->write(in, len);
    }

    /**
     * @brief 写入数字并不进行压缩
     *
     * @tparam T
     * @param value
     */
    template <class T>
    void write_fix_int(T value) {
        m_byte_array->write_fix_int(value);
    }

    // 清空序列化的数据
    void clear() {
        m_byte_array->clear();
    }

    // 根据t的类型读取正确的类型
    template <class T>
    void read(T& t) {
        if constexpr (std::is_same_v<T, bool>) {
            t = m_byte_array->read_fix_int8();
        }
        else if constexpr (std::is_same_v<T, float>) {
            t = m_byte_array->read_float();
        }
        else if constexpr (std::is_same_v<T, double>) {
            t = m_byte_array->read_double();
        }
        else if constexpr (std::is_same_v<T, int8_t>) {
            t = m_byte_array->read_fix_int8();
        }
        else if constexpr (std::is_same_v<T, uint8_t>) {
            t = m_byte_array->read_fix_uint8();
        }
        else if constexpr (std::is_same_v<T, int16_t>) {
            t = m_byte_array->read_fix_int16();
        }
        else if constexpr (std::is_same_v<T, uint16_t>) {
            t = m_byte_array->read_fix_uint16();
        }
        else if constexpr (std::is_same_v<T, int32_t>) {
            t = m_byte_array->read_var_int32();
        }
        else if constexpr (std::is_same_v<T, uint32_t>) {
            t = m_byte_array->read_var_uint32();
        }
        else if constexpr (std::is_same_v<T, int64_t>) {
            t = m_byte_array->read_var_int64();
        }
        else if constexpr (std::is_same_v<T, uint64_t>) {
            t = m_byte_array->read_var_uint64();
        }
        else if constexpr (std::is_same_v<T, std::string>) {
            t = m_byte_array->read_string_vint();
        }
    }

    // 根据t的类型写入正确的序列化数据
    template <class T>
    void write(T t) {
        if constexpr (std::is_same_v<T, bool>) {
            m_byte_array->write_fix_int8(t);
        }
        else if constexpr (std::is_same_v<T, float>) {
            m_byte_array->write_float(t);
        }
        else if constexpr (std::is_same_v<T, double>) {
            m_byte_array->write_double(t);
        }
        else if constexpr (std::is_same_v<T, int8_t>) {
            m_byte_array->write_fix_int8(t);
        }
        else if constexpr (std::is_same_v<T, uint8_t>) {
            m_byte_array->write_fix_uint8(t);
        }
        else if constexpr (std::is_same_v<T, int16_t>) {
            m_byte_array->write_fix_int16(t);
        }
        else if constexpr (std::is_same_v<T, uint16_t>) {
            m_byte_array->write_fix_uint16(t);
        }
        else if constexpr (std::is_same_v<T, int32_t>) {
            m_byte_array->write_var_int32(t);
        }
        else if constexpr (std::is_same_v<T, uint32_t>) {
            m_byte_array->write_var_uint32(t);
        }
        else if constexpr (std::is_same_v<T, int64_t>) {
            m_byte_array->write_var_int64(t);
        }
        else if constexpr (std::is_same_v<T, uint64_t>) {
            m_byte_array->write_var_uint64(t);
        }
        else if constexpr (std::is_same_v<T, std::string>) {
            m_byte_array->write_string_vint(t);
        }
        else if constexpr (std::is_same_v<T, char*>) {
            m_byte_array->write_string_vint(std::string(t));
        }
        else if constexpr (std::is_same_v<T, const char*>) {
            m_byte_array->write_string_vint(std::string(t));
        }
    }

public:
    template <class T>
    Serializer& operator>>(T& t) {
        read(t);
        return *this;
    }

    template <class T>
    Serializer& operator<<(const T& t) {
        write(t);
        return *this;
    }

    // 利用折叠表达式展开tuple进行序列化
    template <class... Args>
    Serializer& operator>>(std::tuple<Args...>& t) {
        const auto& deserializer = [this]<class Tulpe, std::size_t... Index>(
                                       Tulpe& t, std::index_sequence<Index...>) {
            (*this >> ... >> std::get<Index>(t));
        };
        deserializer(t, std::index_sequence_for<Args...> {});
        return *this;
    }

    template <class... Args>
    Serializer& operator<<(const std::tuple<Args...>& t) {
        const auto& serializer = [this]<class Tuple, std::size_t... Index>(
                                     Tuple& t, std::index_sequence<Index...>) {
            (*this << ... << std::get<Index>(t));
        };
        serializer(t, std::index_sequence_for<Args...> {});
        return *this;
    }

    /**
     * @brief 读取链表结构, 先读取长度, 在依次读取链表里的值, 支持基本数据类型以及stl容器的嵌套
     *
     * @tparam T
     * @param t
     * @return Serializer&
     */
    template <class T>
    Serializer& operator>>(std::list<T>& t) {
        size_t size;
        read(size);
        for (size_t i = 0; i < size; ++i) {
            T tmp;
            read(tmp);
            t.emplace_back(tmp);
        }
        return *this;
    }

    template <class T>
    Serializer& operator<<(const std::list<T>& t) {
        write(t.size());
        for (auto& i : t) {
            *this << i;
        }
        return *this;
    }

    template <class T>
    Serializer& operator>>(std::vector<T>& t) {
        size_t size;
        read(size);
        for (size_t i = 0; i < size; ++i) {
            T tmp;
            read(tmp);
            t.emplace_back(tmp);
        }
        return *this;
    }

    template <class T>
    Serializer& operator<<(const std::vector<T>& t) {
        write(t.size());
        for (auto& i : t) {
            *this << i;
        }
        return *this;
    }

    template <class T>
    Serializer& operator>>(std::set<T>& t) {
        size_t size;
        read(size);
        for (size_t i = 0; i < size; ++i) {
            T tmp;
            read(tmp);
            t.emplace(tmp);
        }
        return *this;
    }

    template <class T>
    Serializer& operator<<(const std::set<T>& t) {
        write(t.size());
        for (auto& i : t) {
            *this << i;
        }
        return *this;
    }

    template <class T>
    Serializer& operator>>(std::unordered_set<T>& t) {
        size_t size;
        read(size);
        for (size_t i = 0; i < size; ++i) {
            T tmp;
            read(tmp);
            t.emplace(tmp);
        }
        return *this;
    }

    template <class T>
    Serializer& operator<<(const std::unordered_set<T>& t) {
        write(t.size());
        for (auto& i : t) {
            write(i);
        }
        return *this;
    }

    template <class T>
    Serializer& operator>>(std::multiset<T>& t) {
        size_t size;
        read(size);
        for (size_t i = 0; i < size; ++i) {
            T tmp;
            read(tmp);
            t.emplace(tmp);
        }
        return *this;
    }

    template <class T>
    Serializer& operator<<(const std::multiset<T>& t) {
        write(t.size());
        for (auto& i : t) {
            write(i);
        }
        return *this;
    }

    template <class T>
    Serializer& operator>>(std::unordered_multiset<T>& t) {
        size_t size;
        read(size);
        for (size_t i = 0; i < size; ++i) {
            T tmp;
            read(tmp);
            t.emplace(tmp);
        }
        return *this;
    }

    template <class T>
    Serializer& operator<<(const std::unordered_multiset<T>& t) {
        write(t.size());
        for (auto& i : t) {
            write(i);
        }
        return *this;
    }

    template <class K, class V>
    Serializer& operator>>(std::pair<K, V>& t) {
        *this >> t.first >> t.second;
        return *this;
    }

    template <class K, class V>
    Serializer& operator<<(std::pair<K, V>& t) {
        *this << t.first << t.second;
    }

    /**
     * @brief map需要先读取pair结构
     *
     * @tparam K
     * @tparam V
     * @param t
     * @return Serializer&
     */
    template <class K, class V>
    Serializer& operator>>(std::map<K, V>& t) {
        size_t size;
        read(size);
        for (size_t i = 0; i < size; ++i) {
            std::pair<K, V> tmp;
            *this >> tmp;
            t.emplace(tmp);
        }
        return *this;
    }

    template <class K, class V>
    Serializer& operator<<(const std::map<K, V>& t) {
        write(t.size());
        for (auto& i : t) {
            *this << i;
        }
        return *this;
    }

    template <class K, class V>
    Serializer& operator>>(std::unordered_map<K, V>& t) {
        size_t size;
        read(size);
        for (size_t i = 0; i < size; ++i) {
            std::pair<K, V> tmp;
            *this >> tmp;
            t.emplace(tmp);
        }
        return *this;
    }

    template <class K, class V>
    Serializer& operator<<(const std::unordered_map<K, V>& t) {
        write(t.size());
        for (auto& i : t) {
            *this << i;
        }
        return *this;
    }

    template <class K, class V>
    Serializer& operator>>(std::multimap<K, V>& t) {
        size_t size;
        read(size);
        for (size_t i = 0; i < size; ++i) {
            std::pair<K, V> tmp;
            *this >> tmp;
            t.emplace(tmp);
        }
        return *this;
    }

    template <class K, class V>
    Serializer& operator<<(const std::multimap<K, V>& t) {
        write(t.size());
        for (auto& i : t) {
            *this << i;
        }
        return *this;
    }

    template <class K, class V>
    Serializer& operator>>(std::unordered_multimap<K, V>& t) {
        size_t size;
        read(size);
        for (size_t i = 0; i < size; ++i) {
            std::pair<K, V> tmp;
            *this >> tmp;
            t.emplace(tmp);
        }
        return *this;
    }

    template <class K, class V>
    Serializer& operator<<(const std::unordered_multimap<K, V>& t) {
        write(t.size());
        for (auto& i : t) {
            *this << i;
        }
        return *this;
    }

private:
    ByteArray::ptr m_byte_array;
};

}  // namespace acid::rpc


#endif