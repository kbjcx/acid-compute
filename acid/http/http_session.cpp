#include "http_session.h"

#include "acid/logger/logger.h"
#include "http_parse.h"

namespace acid::http {

static auto logger = GET_LOGGER_BY_NAME("system");

HttpSession::HttpSession(Socket::ptr socket, bool owner) : SocketStream(socket, owner) {
}

HttpRequest::ptr HttpSession::recv_request() {
    LOG_DEBUG(logger) << "recv request from fd = " << get_socket()->get_socketfd();
    HttpRequestParser::ptr parser(new HttpRequestParser);
    uint64_t buffer_size = HttpRequestParser::get_http_request_buffer_size();
    std::shared_ptr<char> buffer(new char[buffer_size], [](char* p) { delete[] p; });

    char* data = buffer.get();
    int offset = 0;

    do {
        size_t len = read(data + offset, buffer_size - offset);
        if (len == 0) {
            LOG_DEBUG(logger) << "request len == 0 "
                              << "socket is connected: " << get_socket()->is_connected();
            close();
            return nullptr;
        }
        len += offset;
        size_t nparsed = parser->execute(data, len);
        if (parser->has_error()) {
            LOG_DEBUG(logger) << "parser has error";
            close();
            return nullptr;
        }

        offset = len - nparsed;
        if (offset == static_cast<int>(buffer_size)) {
            LOG_DEBUG(logger) << "too large request";
            close();
            return nullptr;
        }

        if (parser->is_finished()) {
            break;
        }
    } while (true);

    parser->get_data()->init();
    return parser->get_data();
}

ssize_t HttpSession::send_response(HttpResponse::ptr response) {
    std::stringstream ss;
    response->dump(ss);
    std::string data = ss.str();
    return write_fix_size(data.c_str(), data.size());
}

}  // namespace acid::http