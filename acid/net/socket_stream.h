/*!
 * @file socket_stream.h
 * @author kbjcx(lulu5v@163.com)
 * @brief
 * @version 0.1
 * @date 2023-07-04
 *
 * @copyright Copyright (c) 2023
 */
#ifndef DF_SOCKET_STREAM_H
#define DF_SOCKET_STREAM_H

#include "acid/common/iomanager.h"
#include "acid/common/mutex.h"
#include "acid/common/stream.h"
#include "socket.h"

namespace acid {

class SocketStream : public Stream {
public:
    using ptr = std::shared_ptr<SocketStream>;

    /*!
     *
     * @param socket socket类
     * @param owner 是否完全掌握socket对象
     */
    explicit SocketStream(Socket::ptr socket, bool owner = true);

    /*!
     * @brief 析构函数
     * @attention 如果完全控制sicket对象的话，需要关闭socket
     */
    ~SocketStream() override;

    /**
     * @brief 读取数据
     * @param[out] buffer 待接收数据的内存
     * @param[in] length 待接收数据的内存长度
     * @return
     *      @retval >0 返回实际接收到的数据长度
     *      @retval =0 socket被远端关闭
     *      @retval <0 socket错误
     */
    size_t read(void *buffer, size_t length) override;

    size_t read(ByteArray::ptr byte_array, size_t length) override;

    /**
     * @brief 写入数据
     * @param[in] buffer 待发送数据的内存
     * @param[in] length 待发送数据的内存长度
     * @return
     *      @retval >0 返回实际接收到的数据长度
     *      @retval =0 socket被远端关闭
     *      @retval <0 socket错误
     */
    size_t write(const void *buffer, size_t length) override;

    size_t write(ByteArray::ptr byte_array, size_t length) override;

    void close() override;

    Socket::ptr get_socket() const {
        return m_socket;
    }

    bool is_connected() const;

    Address::ptr get_remote_address();

    Address::ptr get_local_address();

    std::string get_remote_address_string();

    std::string get_local_address_string();

private:
    Socket::ptr m_socket;
    bool m_owner;

};  // class SocketStream : public Stream

}  // namespace acid

#endif  // DF_SOCKET_STREAM_H
