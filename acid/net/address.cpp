#include "address.h"

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netdb.h>

#include <sstream>
#include "acid/logger/logger.h"

namespace acid {
static auto logger = GET_LOGGER_BY_NAME("system");

Address::ptr Address::create(const sockaddr* addr, socklen_t addr_len) {
    if (addr == nullptr) {
        return nullptr;
    }

    Address::ptr result;

    switch (addr->sa_family) {
        case AF_INET:
            result.reset(new IPv4Address(*(const sockaddr_in*) addr));
            break;
        case AF_INET6:
            result.reset(new IPv6Address(*(const sockaddr_in6*) addr));
            break;
        default: result.reset(new UnknownAddress(*addr)); break;
    }

    return result;
}

Address::ptr Address::look_up_any(const std::string& host,
                                  int family,
                                  int type,
                                  int protocol) {
    std::vector<Address::ptr> result;
    if (look_up(result, host, family, type, protocol)) {
        return result.at(0);
    }
    return nullptr;
}

std::shared_ptr<IPAddress> Address::look_up_any_ipaddress(
    const std::string& host,
    int family,
    int type,
    int protocol) {
    std::vector<Address::ptr> result;
    if (look_up(result, host, family, type, protocol)) {
        for (auto& addr : result) {
            IPAddress::ptr v = std::dynamic_pointer_cast<IPAddress>(addr);
            if (v) {
                return v;
            }
        }
    }

    return nullptr;
}

bool Address::look_up(std::vector<Address::ptr>& result,
                      const std::string& host,
                      int family,
                      int type,
                      int protocol) {
    addrinfo hints{}, *results, *next;
    hints.ai_flags = 0;
    hints.ai_family = family;
    hints.ai_socktype = type;
    hints.ai_protocol = protocol;
    hints.ai_addrlen = 0;
    hints.ai_canonname = nullptr;
    hints.ai_addr = nullptr;
    hints.ai_next = nullptr;

    std::string node;
    const char* service = nullptr;

    if (!host.empty() && host.at(0) == '[') {
        const char* end_ipv6 =
            (const char*) memchr(host.c_str() + 1, ']', host.size() - 1);
        if (end_ipv6) {
            if (*(end_ipv6 + 1) == ':') {
                service = end_ipv6 + 2;
            }
            node = host.substr(1, end_ipv6 - host.c_str() - 1);
        }
    }

    if (node.empty()) {
        service = (const char*) memchr(host.c_str(), ':', host.size());
        if (service) {
            if (!memchr(service + 1, ':',
                        host.size() - (service - host.c_str()) - 1)) {
                node = host.substr(0, service - host.c_str());
                ++service;
            }
        }
    }

    if (node.empty()) {
        node = host;
    }

    int error = getaddrinfo(node.c_str(), service, &hints, &results);
    if (error) {
//        LOGGER_DEBUG(
//            logger, "Address::look_up getaddress({},{},{}) err={} errstr={}",
//            host, family, type, error, gai_strerror(error));
        return false;
    }

    next = results;
    while (next) {
        result.push_back(create(next->ai_addr, (socklen_t) next->ai_addrlen));
        next = next->ai_next;
    }

    freeaddrinfo(results);
    return !result.empty();
}

bool Address::get_interface_address(
    std::multimap<std::string, std::pair<Address::ptr, uint32_t>>& result,
    int family) {
    struct ifaddrs *next, *results;
    int error = getifaddrs(&results);
    if (error) {
//        LOGGER_DEBUG(
//            logger,
//            "Address::get_interface_address getifaddrs error = {} errstr = {}",
//            error, gai_strerror(error));
        return false;
    }

    try {
        for (next = results; next; next = next->ifa_next) {
            Address::ptr addr;
            uint32_t prefix_len = ~0u;
            if (family != AF_UNSPEC && family != next->ifa_addr->sa_family) {
                continue;
            }

            switch (next->ifa_addr->sa_family) {
                case AF_INET: {
                    addr = create(next->ifa_addr, sizeof(sockaddr_in));
                    uint32_t net_mask =
                        ((sockaddr_in*) next->ifa_netmask)->sin_addr.s_addr;
                    prefix_len = count_bytes(net_mask);
                } break;
                case AF_INET6: {
                    addr = create(next->ifa_addr, sizeof(sockaddr_in6));
                    in6_addr& net_mask =
                        ((sockaddr_in6*) next->ifa_addr)->sin6_addr;
                    prefix_len = 0;
                    for (int i = 0; i < 16; ++i) {
                        prefix_len += count_bytes(net_mask.s6_addr[i]);
                    }
                } break;
                default: break;
            }

            if (addr) {
                result.emplace(next->ifa_name,
                               std::make_pair(addr, prefix_len));
            }
        }
    } catch (...) {
//        LOGGER_ERROR(logger, "Address::get_interface_address exception");
        freeifaddrs(results);
        return false;
    }
    freeifaddrs(results);
    return !result.empty();
}

bool Address::get_interface_address(
    std::vector<std::pair<Address::ptr, uint32_t>>& result,
    const std::string& interface,
    int family) {
    if (interface.empty() || interface == "*") {
        if (family == AF_INET || family == AF_UNSPEC) {
            result.emplace_back(Address::ptr(new IPv4Address()), 0u);
        }
        if (family == AF_INET6 || family == AF_UNSPEC) {
            result.emplace_back(Address::ptr(new IPv6Address()), 0u);
        }
        return true;
    }

    std::multimap<std::string, std::pair<Address::ptr, uint32_t>> results;

    if (!get_interface_address(results, family)) {
        return false;
    }

    auto [first, second] = results.equal_range(interface);
    for (; first != second; ++first) { result.push_back(first->second); }
    return !result.empty();
}

int Address::get_family() const {
    return get_addr()->sa_family;
}

std::string Address::to_string() {
    std::stringstream ss;
    insert(ss);
    return ss.str();
}

bool Address::operator<(const acid::Address& addr) const {
    socklen_t min_len = std::min(get_addr_len(), addr.get_addr_len());
    int result = memcmp(get_addr(), addr.get_addr(), min_len);
    if (result < 0) {
        return true;
    } else if (result > 0) {
        return false;
    } else if (get_addr_len() < addr.get_addr_len()) {
        return true;
    }
    return false;
}

bool Address::operator==(const acid::Address& addr) const {
    return get_addr_len() == addr.get_addr_len() &&
           memcmp(get_addr(), addr.get_addr(), get_addr_len()) == 0;
}

bool Address::operator!=(const acid::Address& addr) const {
    return !(*this == addr);
}

IPAddress::ptr IPAddress::create(const char* host_name, uint16_t port) {
    addrinfo hints{}, *results;
    memset(&hints, 0, sizeof(addrinfo));

    hints.ai_family = AF_UNSPEC;

    int error = getaddrinfo(host_name, nullptr, &hints, &results);
    if (error) {
//        LOGGER_DEBUG(logger,
//                            "IPAddress::create({}, {}) error = {} errstr = {}",
//                            host_name, port, error, gai_strerror(error));
        return nullptr;
    }

    try {
        Address::ptr address =
            Address::create(results->ai_addr, (socklen_t) results->ai_addrlen);
        IPAddress::ptr result = std::dynamic_pointer_cast<IPAddress>(address);
        if (result) {
            result->set_port(port);
        }
        freeaddrinfo(results);
        return result;
    } catch (...) {
        freeaddrinfo(results);
        return nullptr;
    }
}

IPv4Address::ptr IPv4Address::create(const char* ip, uint16_t port) {
    IPv4Address::ptr result(new IPv4Address());
    result->addr_.sin_port = host_to_network(port);
    int ret = inet_pton(AF_INET, ip, &result->addr_.sin_addr.s_addr);
    if (result <= 0) {
//        LOGGER_DEBUG(logger,
//                            "IPv4Address::create({},{}) ret = {} "
//                            "errno = {} strerr = {}",
//                            ip, port, ret, errno, gai_strerror(errno));
        return nullptr;
    }
    return result;
}

IPv4Address::IPv4Address() {
    memset(&addr_, 0, sizeof(addr_));
    addr_.sin_family = AF_INET;
}

IPv4Address::IPv4Address(const sockaddr_in& address) {
    addr_ = address;
}

IPv4Address::IPv4Address(uint32_t ip, uint16_t port) {
    memset(&addr_, 0, sizeof(addr_));
    addr_.sin_family = AF_INET;
    addr_.sin_addr.s_addr = host_to_network(ip);
    addr_.sin_port = host_to_network(port);
}

const sockaddr* IPv4Address::get_addr() const {
    return (sockaddr*) &addr_;
}

sockaddr* IPv4Address::get_addr() {
    return (sockaddr*) &addr_;
}

const socklen_t IPv4Address::get_addr_len() const {
    return sizeof(addr_);
}

std::ostream& IPv4Address::insert(std::ostream& out) {
    uint32_t addr = network_to_host(addr_.sin_addr.s_addr);
    out << ((addr >> 24) & 0xFF) << '.' << ((addr >> 16) & 0xFF) << '.'
        << ((addr >> 8) & 0xFF) << '.' << (addr & 0xFF) << ':'
        << network_to_host(addr_.sin_port);
    return out;
}

IPAddress::ptr IPv4Address::broadcast_address(uint32_t prefix_len) {
    if (prefix_len > 32) {
        return nullptr;
    }
    sockaddr_in broad_addr(addr_);
    broad_addr.sin_addr.s_addr |=
        host_to_network(create_mask<uint32_t>(prefix_len));
    IPv4Address::ptr result(new IPv4Address(broad_addr));
    return result;
}

IPAddress::ptr IPv4Address::network_address(uint32_t prefix_len) {
    if (prefix_len > 32) {
        return nullptr;
    }
    sockaddr_in network_addr(addr_);
    network_addr.sin_addr.s_addr &=
        host_to_network(create_mask<uint32_t>(prefix_len));
    IPv4Address::ptr result(new IPv4Address(network_addr));
    return result;
}

IPAddress::ptr IPv4Address::subnet_mask(uint32_t prefix_len) {
    if (prefix_len > 32) {
        return nullptr;
    }
    sockaddr_in subnet_mask{};
    memset(&subnet_mask, 0, sizeof(sockaddr_in));
    subnet_mask.sin_family = AF_INET;
    subnet_mask.sin_addr.s_addr =
        host_to_network(~create_mask<uint32_t>(prefix_len));
    IPv4Address::ptr result(new IPv4Address(subnet_mask));
    return result;
}

uint32_t IPv4Address::get_port() const {
    return network_to_host(addr_.sin_port);
}

void IPv4Address::set_port(uint16_t port) {
    addr_.sin_port = host_to_network(port);
}

// ------------------------------------------------------------------------
// IPv6Address
// ------------------------------------------------------------------------
IPv6Address::ptr IPv6Address::create(const char* ip, uint16_t port) {
    IPv6Address::ptr result(new IPv6Address);
    result->addr_.sin6_port = host_to_network(port);
    int ret = inet_pton(AF_INET6, ip, &result->addr_.sin6_addr);
    if (ret <= 0) {
//        LOGGER_DEBUG(logger,
//                            "IPv6Address::create({}, {}) ret = {}"
//                            "errno = {} strerr = {}",
//                            ip, port, ret, errno, gai_strerror(errno));
        return nullptr;
    }
    return result;
}

IPv6Address::IPv6Address() {
    memset(&addr_, 0, sizeof(addr_));
    addr_.sin6_family = AF_INET6;
}

IPv6Address::IPv6Address(const sockaddr_in6& addr) {
    addr_ = addr;
}

IPv6Address::IPv6Address(const uint8_t ip[16], uint16_t port) {
    memset(&addr_, 0, sizeof(addr_));
    addr_.sin6_family = AF_INET6;
    addr_.sin6_port = host_to_network(port);
    memcpy(&addr_.sin6_addr.s6_addr, ip, 16);
}

const sockaddr* IPv6Address::get_addr() const {
    return (sockaddr*) &addr_;
}

sockaddr* IPv6Address::get_addr() {
    return (sockaddr*) &addr_;
}

uint32_t IPv6Address::get_port() const {
    return network_to_host(addr_.sin6_port);
}

void IPv6Address::set_port(uint16_t port) {
    addr_.sin6_port = host_to_network(port);
}

const socklen_t IPv6Address::get_addr_len() const {
    return sizeof(addr_);
}

std::ostream& IPv6Address::insert(std::ostream& out) {
    // TODO IPv6地址输出
    uint16_t* addr = (uint16_t*) addr_.sin6_addr.s6_addr;
    out << '[';
    bool used_zeros = false;
    for (int i = 0; i < 8; ++i) {
        if (addr[i] == 0 && !used_zeros) {
            continue;
        }
        if (i > 0 && addr[i - 1] == 0 && !used_zeros) {
            out << ':';
            used_zeros = true;
        }
        if (i > 0) {
            out << ':';
        }
        out << std::hex << network_to_host(addr[i]) << std::dec;
    }

    if (!used_zeros && addr[7] == 0) {
        out << "::";
    }
    out << "]:" << network_to_host(addr_.sin6_port);
    return out;
}

IPAddress::ptr IPv6Address::broadcast_address(uint32_t prefix_len) {
    // TODO IPv6广播地址
}

IPAddress::ptr IPv6Address::network_address(uint32_t prefix_len) {
    // TODO IPv6网络地址
}

IPAddress::ptr IPv6Address::subnet_mask(uint32_t prefix_len) {
    // TODO IPv6子网掩码
}

/*------------------------------------------------------------------------
    * UnixAddress
    * ------------------------------------------------------------------------
    */

static constexpr uint32_t MAX_PATH_LEN =
    sizeof(((sockaddr_un*) 0)->sun_path) - 1;

UnixAddress::UnixAddress() {
    memset(&addr_, 0, sizeof(addr_));
    addr_.sun_family = AF_UNIX;
    len_ = offsetof(sockaddr_un, sun_path) + MAX_PATH_LEN;
}

UnixAddress::UnixAddress(const std::string& path) {
    memset(&addr_, 0, sizeof(addr_));
    addr_.sun_family = AF_UNIX;
    len_ = path.size() + 1;
    if (!path.empty() && path[0] == '\0') {
        --len_;
    }
    if (len_ > sizeof(addr_.sun_path)) {
        throw std::logic_error("path too long");
    }
    memcpy(addr_.sun_path, path.c_str(), len_);
    len_ += offsetof(sockaddr_un, sun_path);
}

const sockaddr* UnixAddress::get_addr() const {
    return (sockaddr*) &addr_;
}

sockaddr* UnixAddress::get_addr() {
    return (sockaddr*) &addr_;
}

const socklen_t UnixAddress::get_addr_len() const {
    return len_;
}

std::ostream& UnixAddress::insert(std::ostream& out) {
    // TODO 输出Unix地址
}

void UnixAddress::set_addr_len(uint32_t len) {
    len_ = len;
}

/*------------------------------------------------------------------------
    * UnknownAddress
    * ------------------------------------------------------------------------
    */

UnknownAddress::UnknownAddress(int family) {
    memset(&addr_, 0, sizeof(addr_));
    addr_.sa_family = family;
}

UnknownAddress::UnknownAddress(const sockaddr& addr) {
    addr_ = addr;
}

const sockaddr* UnknownAddress::get_addr() const {
    return (sockaddr*) &addr_;
}

sockaddr* UnknownAddress::get_addr() {
    return (sockaddr*) &addr_;
}

const socklen_t UnknownAddress::get_addr_len() const {
    return sizeof(addr_);
}

std::ostream& UnknownAddress::insert(std::ostream& out) {
    out << "[ UnknownAddress family=" << addr_.sa_family << " ]";
    return out;
}


}// namespace acid
