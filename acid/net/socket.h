//
// Created by llz on 23-6-22.
//

#ifndef DF_SOCKET_H
#define DF_SOCKET_H

#include "acid/common/noncopyable.h"
#include "address.h"

#include <memory>

namespace acid {
class Socket : public std::enable_shared_from_this<Socket>, Noncopyable {
public:
    using ptr = std::shared_ptr<Socket>;
    using weak_ptr = std::weak_ptr<Socket>;

    enum Type { TCP = SOCK_STREAM, UDP = SOCK_DGRAM };

    enum Family { IPv4 = AF_INET, IPv6 = AF_INET6, Unix = AF_UNIX };

    /*!
     * @brief 通过addr创建socket对象
     * @param addr
     * @return 创建的socket对象
     */
    static Socket::ptr create_tcp(Address::ptr addr);

    static Socket::ptr create_udp(Address::ptr addr);

    /*!
     * @brief 创建TCP/UDP类型的Ipv4/Ipv6socket对象
     * @return 创建好的socket对象
     */
    static Socket::ptr create_tcp_socket();

    static Socket::ptr create_udp_socket();

    static Socket::ptr create_tcp_socket6();

    static Socket::ptr create_udp_socket6();

    static Socket::ptr create_unix_tcp_socket();

    static Socket::ptr create_unix_udp_socket();

    Socket(int family, int type, int protocol);

    virtual ~Socket();

    void set_send_timeout(int64_t timeout);

    uint64_t get_send_timeout() const;

    void set_recv_timeout(int64_t timeout);

    uint64_t get_recv_timeout() const;

    /*!
     * @brief 获取sockopt @see setsockopt
     */
    bool get_option(int level, int option, void *result, size_t *len);

    /*!
     * @brief 模板函数，不关心类型长度，自动获取类型长度
     * @tparam T
     * @param level
     * @param option
     * @param result
     * @return
     */
    template <class T>
    bool get_option(int level, int option, T &result) {
        size_t len = sizeof(T);
        return get_option(level, option, &result, &len);
    }

    bool set_option(int level, int option, const void *result, size_t len);

    template <class T>
    bool set_option(int level, int option, const T &result) {
        size_t len = sizeof(T);
        return set_option(level, option, &result, len);
    }

    /*!
     * @brief 接受新连接
     * @return 成功返回socket对象， 失败返回空指针
     */
    Socket::ptr accept();

    /*!
     * @brief 将addr地址绑定到socket对象
     * @param addr
     * @return
     */
    bool bind(const Address::ptr addr);

    /*!
     * @brief 向addr指定的地址发起连接
     * @param addr
     * @param timeout_ms
     * @return
     */
    bool connect(const Address::ptr addr, uint64_t timeout_ms = -1);

    /*!
     * @brief 向远程地址重新发起连接
     * @param timeout_ms
     * @return
     */
    bool reconnect(uint64_t timeout_ms = -1);

    /*!
     * @brief 监听socket上的连接事件
     * @param backlog
     * @return
     */
    bool listen(int backlog = SOMAXCONN);

    /*!
     * @brief 关闭socket连接
     * @return
     */
    bool close();

    /*!
     * @brief 发送数据
     * @param[in] buffer 待发送数据的内存
     * @param[in] length 待发送数据的长度
     * @param[in] flags 标志字
     * @return
     * @retval >0 发送成功对应大小的数据
     * @retval =0 socket被关闭
     * @retval <0 socket出错
     */
    virtual ssize_t send(const void *buffer, size_t length, int flags = 0);

    /*!
     * @brief 发送数据
     * @param[in] buffers 待发送数据的内存(iovec数组)
     * @param[in] length 待发送数据的长度(iovec长度)
     * @param[in] flags 标志字
     * @return
     *      @retval >0 发送成功对应大小的数据
     *      @retval =0 socket被关闭
     *      @retval <0 socket出错
     */
    virtual ssize_t send(const iovec *buffers, size_t length, int flags = 0);

    /**
     * @brief 发送数据
     * @param[in] buffer 待发送数据的内存
     * @param[in] length 待发送数据的长度
     * @param[in] to 发送的目标地址
     * @param[in] flags 标志字
     * @return
     *      @retval >0 发送成功对应大小的数据
     *      @retval =0 socket被关闭
     *      @retval <0 socket出错
     */
    virtual ssize_t sendto(const void *buffer, size_t length, const Address::ptr to, int flags = 0);

    /**
     * @brief 发送数据
     * @param[in] buffers 待发送数据的内存(iovec数组)
     * @param[in] length 待发送数据的长度(iovec长度)
     * @param[in] to 发送的目标地址
     * @param[in] flags 标志字
     * @return
     *      @retval >0 发送成功对应大小的数据
     *      @retval =0 socket被关闭
     *      @retval <0 socket出错
     */
    virtual ssize_t sendto(const iovec *buffers, size_t length, const Address::ptr to,
                           int flags = 0);

    /**
     * @brief 接受数据
     * @param[out] buffer 接收数据的内存
     * @param[in] length 接收数据的内存大小
     * @param[in] flags 标志字
     * @return
     *      @retval >0 接收到对应大小的数据
     *      @retval =0 socket被关闭
     *      @retval <0 socket出错
     */
    virtual ssize_t recv(void *buffer, size_t length, int flags = 0);

    /**
     * @brief 接受数据
     * @param[out] buffers 接收数据的内存(iovec数组)
     * @param[in] length 接收数据的内存大小(iovec数组长度)
     * @param[in] flags 标志字
     * @return
     *      @retval >0 接收到对应大小的数据
     *      @retval =0 socket被关闭
     *      @retval <0 socket出错
     */
    virtual ssize_t recv(iovec *buffers, size_t length, int flags = 0);

    /**
     * @brief 接受数据
     * @param[out] buffer 接收数据的内存
     * @param[in] length 接收数据的内存大小
     * @param[out] from 发送端地址
     * @param[in] flags 标志字
     * @return
     *      @retval >0 接收到对应大小的数据
     *      @retval =0 socket被关闭
     *      @retval <0 socket出错
     */
    virtual ssize_t recvfrom(void *buffer, size_t length, Address::ptr from, int flags = 0);

    /**
     * @brief 接受数据
     * @param[out] buffers 接收数据的内存(iovec数组)
     * @param[in] length 接收数据的内存大小(iovec数组长度)
     * @param[out] from 发送端地址
     * @param[in] flags 标志字
     * @return
     *      @retval >0 接收到对应大小的数据
     *      @retval =0 socket被关闭
     *      @retval <0 socket出错
     */
    virtual ssize_t recvfrom(iovec *buffers, size_t length, Address::ptr from, int flags = 0);

    // 获取远端地址
    Address::ptr get_remote_address();

    // 获取本地地址
    Address::ptr get_local_address();

    int get_family() const {
        return m_family;
    }

    int get_type() const {
        return m_type;
    }

    int get_protocol() const {
        return m_protocol;
    }

    bool is_connected() const {
        return m_is_connected;
    }

    int get_socketfd() const {
        return m_sockfd;
    }

    bool is_valid() const {
        return m_sockfd != -1;
    }

    // 获取socket上发生的错误
    int get_error();

    // 输出socket信息到流中
    std::ostream &dump(std::ostream &os) const;

    // 输出string类型的信息
    std::string to_string() const;

    bool cancel_read();

    bool cancel_write();

    bool cancel_accept();

    bool cancel_all();

private:
    // 初始化socket
    // 设置端口重用， TCP：禁用Nagle算法
    void init_socket();

    // 重新创建新的socket
    void new_socket();

    // 初始化socket， 创建套接字
    bool init(int sock);

private:
    int m_sockfd;
    int m_family;
    int m_type;
    int m_protocol;

    bool m_is_connected {false};

    Address::ptr m_remote_addr;
    Address::ptr m_local_addr;
};


std::ostream &operator<<(std::ostream &os, const Socket &socket);
}  // namespace acid

#endif  // DF_SOCKET_H
