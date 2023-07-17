//
// Created by llz on 23-6-22.
//

#ifndef DF_TCP_SERVER_H
#define DF_TCP_SERVER_H

#include "address.h"
#include "socket.h"
#include "acid/common/config.h"
#include "acid/common/lexical_cast.h"
#include "acid/common/iomanager.h"

namespace acid {

struct TcpServerConf {
    std::vector<std::string> address;
    int keepalive = 0;
    uint64_t timeout = 1000 * 60 * 2;
    std::string name;
    int ssl = 0;

    std::string type = "http";

    std::string accept_worker;
    std::string io_worker;
    std::string process_worker;

    bool is_valid() const {
        return !address.empty();
    }

    bool operator==(const TcpServerConf& tsc) const {
        return address == tsc.address &&
        keepalive == tsc.keepalive &&
        timeout == tsc.timeout &&
        name == tsc.name &&
        ssl == tsc.ssl &&
        type == tsc.type &&
        accept_worker == tsc.accept_worker &&
        io_worker == tsc.io_worker &&
        process_worker == tsc.process_worker;
    }
};

template<>
struct detail::Converter<std::string, TcpServerConf> {
    TcpServerConf operator()(const std::string str) {
        YAML::Node node = YAML::Load(str);
        TcpServerConf conf;
        //conf.id = node["id"].as<std::string>(conf.id);
        conf.type = node["type"].as<std::string>(conf.type);
        conf.keepalive = node["keepalive"].as<int>(conf.keepalive);
        conf.timeout = node["timeout"].as<int>(conf.timeout);
        conf.name = node["name"].as<std::string>(conf.name);
        conf.ssl = node["ssl"].as<uint64_t>(conf.ssl);
        //conf.cert_file = node["cert_file"].as<std::string>(conf.cert_file);
        //conf.key_file = node["key_file"].as<std::string>(conf.key_file);
        conf.accept_worker = node["accept_worker"].as<std::string>();
        conf.io_worker = node["io_worker"].as<std::string>();
        conf.process_worker = node["process_worker"].as<std::string>();
        //conf.args = LexicalCast<std::string
        //    ,std::map<std::string, std::string> >()(node["args"].as<std::string>(""));
        if(node["address"].IsDefined()) {
            for(size_t i = 0; i < node["address"].size(); ++i) {
                conf.address.push_back(node["address"][i].as<std::string>());
            }
        }
        return conf;
    }
};

class Address;

class TcpServer : public std::enable_shared_from_this<TcpServer>, Noncopyable{
public:
    using ptr = std::shared_ptr<TcpServer>;

    TcpServer();

    explicit TcpServer(const std::string& server_name,
              IOManager* worker = IOManager::get_this(),
              IOManager* io_worker = IOManager::get_this(),
              IOManager* accept_worker = IOManager::get_this());

    ~TcpServer();

    virtual bool bind(Address::ptr addr, bool ssl = false);

    virtual bool bind(const std::vector<Address::ptr> &addrs,
                      std::vector<Address::ptr> &fails, bool ssl = false);

    // 阻塞线程
    virtual bool start();

    virtual void stop();

    uint64_t get_recv_timeout() const {
        return m_recv_timeout;
    }

    std::string get_name() const {
        return m_name;
    }

    void set_recv_timeout(uint64_t timeout) {
        m_recv_timeout = timeout;
    }

    virtual void set_name(std::string& name) {
        m_name = name;
    }

    bool is_stop() const {
        return m_is_stop;
    }

    std::vector<Socket::ptr> get_sockets() const {
        return m_sockets;
    }

protected:
    virtual void handle_client(Socket::ptr client) = 0;

    virtual void start_accept(Socket::ptr socket);

protected:
    std::vector<Socket::ptr> m_sockets;
    IOManager* m_worker = nullptr;
    IOManager* m_io_worker = nullptr;
    IOManager* m_accept_worker = nullptr;
    uint64_t m_recv_timeout{};
    std::string m_name;
    std::string m_type = "tcp";
    bool m_is_stop{};
    bool m_ssl = false;
};
}  // namespace acid

#endif  // DF_TCP_SERVER_H
