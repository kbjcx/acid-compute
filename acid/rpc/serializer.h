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
#include <string.h>
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

    Serializer();

    Serializer(ByteArray::ptr byte_array);

    Serializer(const std::string& in);

    Serializer(const char* in, int len);

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

    void offset(int off) {
        int old = m_byte_array->get_position();
        m_byte_array->set_position(old + off);
    }

    std::string to_string() {
        return m_byte_array->to_string();
    }

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
        m_byte_array->write_fix_in
    }

    void clear();

    template <class T>
    void read(T& t);

    template <class T>
    void write(T t);

public:
    template <class T>
    Serializer& operator>>(T& t);

    template <class T>
    Serializer& operator<<(const T& t);

    template <class... Args>
    Serializer& operator>>(std::tuple<Args...>& t);

    template <class... Args>
    Serializer& operator<<(const std::tuple<Args...>& t);

    template <class T>
    Serializer& operator>>(std::list<T>& t);

    template <class T>
    Serializer& operator<<(const std::list<T>& t);

    template <class T>
    Serializer& operator>>(std::vector<T>& t);

    template <class T>
    Serializer& operator<<(const std::vector<T>& t);

    template <class T>
    Serializer& operator>>(std::set<T>& t);

    template <class T>
    Serializer& operator<<(const std::set<T>& t);

    template <class T>
    Serializer& operator>>(std::unordered_set<T>& t);

    template <class T>
    Serializer& operator<<(const std::unordered_set<T>& t);

    template <class T>
    Serializer& operator>>(std::multiset<T>& t);

    template <class T>
    Serializer& operator<<(const std::multiset<T>& t);

    template <class T>
    Serializer& operator>>(std::unordered_multiset<T>& t);

    template <class T>
    Serializer& operator<<(const std::unordered_multiset<T>& t);

    template <class K, class V>
    Serializer& operator>>(std::map<K, V>& t);

    template <class K, class V>
    Serializer& operator<<(const std::map<K, V>& t);

    template <class K, class V>
    Serializer& operator>>(std::unordered_map<K, V>& t);

    template <class K, class V>
    Serializer& operator<<(const std::unordered_map<K, V>& t);

    template <class K, class V>
    Serializer& operator>>(std::multimap<K, V>& t);

    template <class K, class V>
    Serializer& operator<<(const std::multimap<K, V>& t);

    template <class K, class V>
    Serializer& operator>>(std::unordered_multimap<K, V>& t);

    template <class K, class V>
    Serializer& operator<<(const std::unordered_multimap<K, V>& t);

private:
    ByteArray::ptr m_byte_array;
};

}  // namespace acid::rpc


#endif