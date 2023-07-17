#include "http_server.h"

namespace acid::http {

static auto logger = GET_LOGGER_BY_NAME("system");

HttpServer::HttpServer(bool keep_alive, acid::IOManager *worker, acid::IOManager *io_worker,
                       acid::IOManager *accept_worker)
    : TcpServer("acid/1.0", worker, io_worker, accept_worker), m_is_keep_alive(keep_alive) {
    m_servlet_dispatch.reset(new ServletDispatch);
    m_type = "http";
}

void HttpServer::set_name(std::string &name) {
    TcpServer::set_name(name);
    m_servlet_dispatch->set_default(std::make_shared<NotFoundServlet>(name));
}

void HttpServer::handle_client(Socket::ptr client) {
    LOG_DEBUG(logger) << "handle client " << *client;
    HttpSession::ptr session(new HttpSession(client));
    do {
        auto request = session->recv_request();
        if (!request) {
            LOG_DEBUG(logger) << "recv http request fail, errno=" << errno
                              << " errstr=" << strerror(errno) << " cliet:" << *client
                              << " keep_alive=" << m_is_keep_alive;
            break;
        }

        HttpResponse::ptr response(new HttpResponse(request->get_version(), request->is_close() || !m_is_keep_alive));
        response->set_header("Server", get_name());
        m_servlet_dispatch->handle(request, response, session);
        session->send_response(response);

        if (!m_is_keep_alive || request->is_close()) {
            break ;
        }
    } while (true);

    session->close();
}

}  // namespace acid::http