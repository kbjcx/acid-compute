

#ifndef DF_ACID_NET_ADDRESS_H_
#define DF_ACID_NET_ADDRESS_H_

#include <iostream>
#include <memory>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <vector>
#include <map>
#include "acid/common/util.h"

namespace acid {

/*!
* @brief 网络字节序转主机字节序
*/
    template<class T>
    constexpr T network_to_host(T t) {
        return endian_cast(t);
    }

/*!
 * @brief 主机字节序转网络字节序
 */
    template<class T>
    constexpr T host_to_network(T t) {
        return endian_cast(t);
    }

    template<class T>
    static T create_mask(uint32_t bit) {
        return (1 << (sizeof(T) * 8 - bit)) - 1;
    }

/*!
 * @brief 计算value有多少个位为1
 */
    template<class T>
    static uint32_t count_bytes(T value) {
        uint32_t result = 0;
        for (; value; ++result) {
            value &= value - 1;
        }
        return result;
    }

    class IPAddress;

    class Address {
    public:
        using ptr = std::shared_ptr<Address>;

        static Address::ptr create(const sockaddr *addr, socklen_t addr_len);

        /*!
         * @brief 通过host地址返回所有Address
         * @param[out] result 保存满足条件的所有Address
         * @param[in] host 域名
         * @param[in] family 协议族(AF_INT, AF_INT6, AF_UNIX)
         * @param[in] type socket类型SOCK_STREAM、SOCK_DGRAM 等
         * @param[in] protocol 协议IPPROTO_TCP、IPPROTO_UDP 等
         * @return 返回是否转换成功
         */
        static bool look_up(std::vector<Address::ptr> &result, const std::string &host,
                            int family = AF_INET, int type = SOCK_STREAM, int protocol = 0);

        /**
         * @brief 通过host地址返回对应条件的任意Address
         * @param[in] host 域名,服务器名等.举例: www.sylar.top[:80] (方括号为可选内容)
         * @param[in] family 协议族(AF_INT, AF_INT6, AF_UNIX)
         * @param[in] type socketl类型SOCK_STREAM、SOCK_DGRAM 等
         * @param[in] protocol 协议,IPPROTO_TCP、IPPROTO_UDP 等
         * @return 返回满足条件的任意Address,失败返回nullptr
         */
        static Address::ptr look_up_any(const std::string &host, int family = AF_INET,
                                        int type = SOCK_STREAM, int protocol = 0);

        /**
         * @brief 通过host地址返回对应条件的任意IPAddress
         * @param[in] host 域名,服务器名等.举例: www.sylar.top[:80] (方括号为可选内容)
         * @param[in] family 协议族(AF_INT, AF_INT6, AF_UNIX)
         * @param[in] type socketl类型SOCK_STREAM、SOCK_DGRAM 等
         * @param[in] protocol 协议,IPPROTO_TCP、IPPROTO_UDP 等
         * @return 返回满足条件的任意IPAddress,失败返回nullptr
         */
        static std::shared_ptr<IPAddress> look_up_any_ipaddress(const std::string &host,
                                                                int family = AF_INET,
                                                                int type = SOCK_STREAM,
                                                                int protocol = 0);

        /**
         * @brief 返回本机所有网卡的<网卡名, 地址, 子网掩码位数>
         * @param[out] result 保存本机所有地址
         * @param[in] family 协议族(AF_INT, AF_INT6, AF_UNIX)
         * @return 是否获取成功
         */
        static bool get_interface_address(std::multimap<std::string, std::pair<Address::ptr, uint32_t>> &result,
                                          int family = AF_INET);

        /**
         * @brief 获取指定网卡的地址和子网掩码位数
         * @param[out] result 保存指定网卡所有地址
         * @param[in] iface 网卡名称
         * @param[in] family 协议族(AF_INT, AF_INT6, AF_UNIX)
         * @return 是否获取成功
         */
        static bool get_interface_address(std::vector<std::pair<Address::ptr, uint32_t>> &result,
                                          const std::string &interface,
                                          int family = AF_INET);

        virtual ~Address() = default;

        int get_family() const;

        virtual const sockaddr *get_addr() const = 0;

        virtual sockaddr *get_addr() = 0;

        virtual const socklen_t get_addr_len() const = 0;

        virtual std::ostream &insert(std::ostream &out) = 0;

        std::string to_string();

        bool operator<(const Address &addr) const;

        bool operator==(const Address &addr) const;

        bool operator!=(const Address &addr) const;

    private:

    };

    class IPAddress : public Address {
    public:
        using ptr = std::shared_ptr<IPAddress>;

        static IPAddress::ptr create(const char *host_name, uint16_t port);

        virtual IPAddress::ptr broadcast_address(uint32_t prefix_len) = 0;

        virtual IPAddress::ptr network_address(uint32_t prefix_len) = 0;

        virtual IPAddress::ptr subnet_mask(uint32_t prefix_len) = 0;

        virtual uint32_t get_port() const = 0;

        virtual void set_port(uint16_t port) = 0;
    };

    class IPv4Address : public IPAddress {
    public:
        using ptr = std::shared_ptr<IPv4Address>;

        static IPv4Address::ptr create(const char *ip, uint16_t port);

        IPv4Address();

        IPv4Address(const sockaddr_in &address);

        IPv4Address(uint32_t ip, uint16_t port);

        const sockaddr *get_addr() const override;

        sockaddr *get_addr() override;

        const socklen_t get_addr_len() const override;

        std::ostream &insert(std::ostream &out) override;

        IPAddress::ptr broadcast_address(uint32_t prefix_len) override;

        IPAddress::ptr network_address(uint32_t prefix_len) override;

        IPAddress::ptr subnet_mask(uint32_t prefix_len) override;

        uint32_t get_port() const override;

        void set_port(uint16_t port) override;

    private:
        sockaddr_in addr_;
    };

    class IPv6Address : public IPAddress {
    public:
        using ptr = std::shared_ptr<IPv6Address>;

        static IPv6Address::ptr create(const char *ip, uint16_t port);

        IPv6Address();

        IPv6Address(const sockaddr_in6 &addr);

        IPv6Address(const uint8_t ip[16], uint16_t port);

        const sockaddr *get_addr() const override;

        sockaddr *get_addr() override;

        const socklen_t get_addr_len() const override;

        std::ostream &insert(std::ostream &out) override;

        IPAddress::ptr broadcast_address(uint32_t prefix_len) override;

        IPAddress::ptr network_address(uint32_t prefix_len) override;

        IPAddress::ptr subnet_mask(uint32_t prefix_len) override;

        uint32_t get_port() const override;

        void set_port(uint16_t port) override;

    private:
        sockaddr_in6 addr_;
    };

    class UnixAddress : public Address {
    public:
        using ptr = std::shared_ptr<UnixAddress>;

        UnixAddress();

        UnixAddress(const std::string &path);

        const sockaddr *get_addr() const override;

        sockaddr *get_addr() override;

        const socklen_t get_addr_len() const override;

        void set_addr_len(uint32_t len);

        std::string get_path() const {
            // TODO get_path
        }

        std::ostream &insert(std::ostream &out) override;

    private:
        sockaddr_un addr_;
        socklen_t len_;
    };

    class UnknownAddress : public Address {
    public:
        using ptr = std::shared_ptr<UnixAddress>;

        UnknownAddress(int family);
        UnknownAddress(const sockaddr& addr);

        const sockaddr *get_addr() const override;

        sockaddr *get_addr() override;

        const socklen_t get_addr_len() const override;

        std::ostream &insert(std::ostream &out) override;

    private:
        sockaddr addr_;
    };

}
#endif //DF_ACID_NET_ADDRESS_H_
