/*!
 * @file http_server.h
 * @author kbjcx(lulu5v@163.com)
 * @brief
 * @version 0.1
 * @date 2023-07-07
 *
 * @copyright Copyright (c) 2023
 */
#ifndef DF_HTTP_SERVER_H
#define DF_HTTP_SERVER_H

#include "acid/net/tcp_server.h"
#include "http_session.h"
#include "servlet.h"

namespace acid::http {

class HttpServer : public TcpServer {
public:
    using ptr = std::shared_ptr<HttpServer>;

    HttpServer(bool keep_alive = false, IOManager* worker = IOManager::get_this(),
               IOManager* io_worker = IOManager::get_this(),
               IOManager* accept_worker = IOManager::get_this());

    ServletDispatch::ptr get_servlet_dispatch() const {
        return m_servlet_dispatch;
    }

    void set_servlet_dispatch(ServletDispatch::ptr servlet_dispatch) {
        m_servlet_dispatch = servlet_dispatch;
    }

    void set_name(std::string &name) override;

protected:
    void handle_client(Socket::ptr client) override;

private:
    bool m_is_keep_alive;
    ServletDispatch::ptr m_servlet_dispatch;
};

}  // namespace acid::http

#endif  // DF_HTTP_SERVER_H
