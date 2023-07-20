/**
 * @file rpc.h
 * @author kbjcx (lulu5v@163.com)
 * @brief
 * @version 0.1
 * @date 2023-07-18
 *
 * @copyright Copyright (c) 2023
 *
 */

#ifndef ACID_RPC_H
#define ACID_RPC_H

#include "serializer.h"

#include <functional>
#include <map>
#include <memory>
#include <sstream>
#include <string>


namespace acid::rpc {

// 连接池向注册中心订阅的前缀
inline const char* RPC_SERVICE_SUBSCRIBE = "[[rpc service subscribe]]";

// 结果返回类型
template <class T>
struct return_type {
    using type = T;
};

// void类型特化为int8_t
template <>
struct return_type<void> {
    using type = int8_t;
};

// 取类型
template <class T>
using return_type_t = typename return_type<T>::type;

// Rpc调用状态
enum RpcState {
    RPC_SUCCESS = 0,  // 成功
    RPC_FAIL,         // 失败
    RPC_NO_MATCH,     // 函数不匹配
    RPC_NO_METHOD,    // 没有找到调用函数
    RPC_CLOSED,       // RCP连接关闭
    RPC_TIMEOUT       // RPC调用超时
};

/**
 * @brief 包装RPC调用结果
 *
 * @tparam T
 */
template <class T = void>
class Result {
public:
    // 纯类型
    using raw_type = T;
    // 结果返回类型
    using type = return_type_t<T>;
    // 调用消息类型
    using message_type = std::string;
    // 调用状态类型
    using code_type = uint16_t;

    static Result<T> success() {
        Result<T> res;
        res.set_code(RPC_SUCCESS);
        res.set_message("success");
        return res;
    }

    static Result<T> fail() {
        Result<T> res;
        res.set_code(RPC_FAIL);
        res.set_message("fail");
        return res;
    }

    Result() = default;

    bool valid() {
        return m_code == 0;
    }

    type& get_value() {
        return m_value;
    }

    void set_value(const type& value) {
        m_value = value;
    }

    void set_code(code_type code) {
        m_code = code;
    }

    int get_code() {
        return static_cast<int>(m_code);
    }

    void set_message(message_type message) {
        m_message = message;
    }

    const message_type& get_message() {
        return m_message;
    }

    type* operator->() noexcept {
        return &m_value;
    }

    const type* operator->() const noexcept {
        return &m_value;
    }

    std::string to_string() {
        std::stringstream ss;
        ss << "[ code = " << m_code << " message = " << m_message << " value = " << m_value << " ]";
        return ss.str();
    }

    /**
     * @brief 反序列化result
     *
     * @param[out] in 序列化的结果
     * @param[out] d 反序列化到result
     * @return Serializer&
     */
    friend Serializer& operator>>(Serializer& in, Result<T>& result) {
        in >> result.m_code >> result.m_message;
        if (result.m_code == 0) {
            in >> result.m_value;
        }
        return in;
    }

    /**
     * @brief 将result进行序列化
     *
     * @param[out] out
     * @param[in] result
     * @return Serializer&
     */
    friend Serializer& operator<<(Serializer& out, Result<T> result) {
        out << result.m_code << result.m_message << result.m_value;
        return out;
    }

private:
    // 调用状态
    code_type m_code = 0;
    // 调用消息
    message_type m_message;
    // 调用结果
    type m_value;
};



};  // namespace acid::rpc


#endif