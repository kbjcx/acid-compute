/**
 * @file protocol.h
 * @author kbjcx (lulu5v@163.com)
 * @brief rpc协议设计
 * @version 0.1
 * @date 2023-07-18
 *
 * @copyright Copyright (c) 2023
 *
 */

#ifndef ACID_PROTOCOL_H
#define ACID_PROTOCOL_H

#include "acid/common/byte_array.h"

#include <memory>

namespace acid::rpc {
/*
 * 私有通信协议
 * +--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+
 * |  BYTE  |        |        |        |        |        |        |        |        |        | |
 * ........                                                           |
 * +--------------------------------------------+--------+--------------------------+--------+-----------------+--------+--------+--------+--------+--------+--------+-----------------+
 * |  magic | version|  type  |          sequence id              |          content length |
 * content byte[]                                                     |
 * +--------+-----------------------------------------------------------------------------------------------------------------------------+--------------------------------------------+
 * 第一个字节是魔法数。
 * 第二个字节代表协议版本号，以便对协议进行扩展，使用不同的协议解析器。
 * 第三个字节是请求类型，如心跳包，rpc请求。
 * 第四个字节开始是一个32位序列号。
 * 第七个字节开始的四字节表示消息长度，即后面要接收的内容长度。
 */

class Protocol {
public:
    using ptr = std::shared_ptr<Protocol>;

    static constexpr uint8_t MAGIC = 0xcc;
    static constexpr uint8_t DEFAULT_VERSION = 0x01;
    static constexpr uint8_t BASE_LENGTH = 11;

    enum class MessageType : uint8_t {
        HEARTBEAT_PACKET,  // 心跳包
        RPC_PROVIDER,      // 向服务中心声明为provider
        RPC_COMSUMER,      // 向服务中心生命为comsumer

        RPC_REQUEST,   // 通用请求
        RPC_RESPONSE,  // 通用响应

        RPC_METHOD_REQUEST,   // 请求方法调用
        RPC_METHOD_RESPONSE,  // 响应方法调用

        RPC_SERVICE_REGISTER,  // 向中心注册服务
        RPC_SERVICE_REGISTER_RESPONSE,

        RPC_SERVICE_DISCOVER,  // 向中心请求服务发现
        RPC_SERVICE_DISCOVER_RESPONSE,

        RPC_SUBSCRIBE_REQUEST,  // 订阅
        RPC_SUBSCRIBE_RESPONSE,

        RPC_PUBLISH_REQUEST,  // 发布
        RPC_PUBLISH_RESPONSE
    };

    static Protocol::ptr create(MessageType type, const std::string& content, uint32_t id = 0);

    static Protocol::ptr heartbeat();

    uint8_t get_magic() const {
        return m_magic;
    }

    uint8_t get_version() const {
        return m_version;
    }

    MessageType get_message_type() const {
        return static_cast<MessageType>(m_type);
    }

    uint32_t get_sequence_id() const {
        return m_sequence_id;
    }

    uint32_t get_content_length() const {
        return m_content_length;
    }

    const std::string& get_content() const {
        return m_content;
    }

    void set_magic(uint8_t magic) {
        m_magic = magic;
    }

    void set_version(uint8_t version) {
        m_version = version;
    }

    void set_message_type(MessageType type) {
        m_type = static_cast<uint8_t>(type);
    }

    void set_sequence_id(uint32_t sequence_id) {
        m_sequence_id = sequence_id;
    }

    void set_content_length(uint32_t content_length) {
        m_content_length = content_length;
    }

    void set_content(const std::string& content) {
        m_content = content;
    }

    ByteArray::ptr encode_meta();

    ByteArray::ptr encode();

    void decode_meta(ByteArray::ptr byte_array);

    void decode(ByteArray::ptr byte_array);

    std::string to_string();

private:
    uint8_t m_magic = MAGIC;
    uint8_t m_version = DEFAULT_VERSION;
    uint8_t m_type = 0;
    uint32_t m_sequence_id = 0;
    uint32_t m_content_length = 0;
    std::string m_content;
};


};  // namespace acid::rpc


#endif