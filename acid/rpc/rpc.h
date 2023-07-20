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

inline const char* RPC_SERVICE_SUBSCRIBE = "[[rpc service subscribe]]";

template <class T>
struct return_type {
    using type = T;
};

template <>
struct return_type<void> {
    using type = int8_t;
};

template <class T>
using return_type_t = typename return_type<T>::type;

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
    using row_type = T;
    using type = return_type_t<T>;
    using message_type = std::string;
    using code_type = uint16_t;

    static Result<T> success();

    static Result<T> fail();

    Result();

    bool valid();

    type& get_value();

    void set_value(const type& value);

    void set_code(code_type code);

    int get_code();

    void set_message(message_type message);

    const message_type& get_message();

    type* operator->() noexcept {
        return &m_value;
    }

    const type* operator->() const noexcept {
        return &m_value;
    }

    std::string to_string();

    friend Serializer& operator>>(Serializer& in, Result<T>& d);

    friend Serializer& operator<<(Serializer& out, Result<T> d);

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