/*!
 * @file http_session.h
 * @author kbjcx(lulu5v@163.com)
 * @brief
 * @version 0.1
 * @date 2023-07-06
 *
 * @copyright Copyright (c) 2023
 */
#ifndef DF_HTTP_SESSION_H
#define DF_HTTP_SESSION_H

#include "acid/net/socket_stream.h"
#include "http.h"

namespace acid::http {

class HttpSession : public SocketStream {
public:
    using ptr = std::shared_ptr<HttpSession>;

    HttpSession(Socket::ptr socket, bool owner = true);

    HttpRequest::ptr recv_request();

    ssize_t send_response(HttpResponse::ptr response);
};

}

#endif  // DF_HTTP_SESSION_H
