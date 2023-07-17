#include "http.h"

#include "acid/logger/logger.h"

#include <cstring>

namespace acid::http {

static auto looger = GET_LOGGER_BY_NAME("system");

HttpMethod string_to_method(const std::string& method) {
#define XX(num, name, string) \
    if (strcmp(method.c_str(), #string) == 0) return HttpMethod::name;
    HTTP_METHOD_MAP(XX)
#undef XX
    return HttpMethod::INVALID_METHOD;
}

HttpMethod string_to_method(const char* method) {
#define XX(num, name, string) \
    if (strcmp(method, #string) == 0) return HttpMethod::name;
    HTTP_METHOD_MAP(XX)
#undef XX
    return HttpMethod::INVALID_METHOD;
}

HttpContentType string_to_content_type(const std::string& content_type) {
#define XX(name, string) \
    if (strcmp(content_type.c_str(), #string) == 0) return HttpContentType::name;
    HTTP_CONTENT_TYPE(XX)
#undef XX
    return HttpContentType::APPLICATION_URLENCODED;
}


HttpContentType string_to_content_type(const char* content_type) {
#define XX(name, string) \
    if (strcmp(content_type, #string) == 0) return HttpContentType::name;
    HTTP_CONTENT_TYPE(XX)
#undef XX
    return HttpContentType::APPLICATION_URLENCODED;
}

static const char* s_method_string[] = {
#define XX(num, name, string) #string,
    HTTP_METHOD_MAP(XX)
#undef XX
};

std::string http_method_to_string(HttpMethod method) {
    uint32_t index = static_cast<uint32_t>(method);
    if (index >= (sizeof(s_method_string) / sizeof(s_method_string[0]))) {
        return "<unknown>";
    }
    return s_method_string[index];
}

std::string http_status_to_string(HttpStatus status) {
    switch (status) {
#define XX(code, name, msg)  \
    case HttpStatus::name: { \
        return #msg;         \
    }
        HTTP_STATUS_MAP(XX)
#undef XX
        default:
            return "<unknown>";
    }
}

static const char* s_type_string[] = {
#define XX(name, string) #string,
    HTTP_CONTENT_TYPE(XX)
#undef XX
};

std::string http_context_type_to_string(HttpContentType context_type) {
    uint32_t index = static_cast<uint32_t>(context_type);
    if (index >= (sizeof(s_type_string) / sizeof(s_type_string[0]))) {
        return "text/plain";
    }
    return s_type_string[index];
}

bool CaseInsensitiveLess::operator()(const std::string& lhs, const std::string& rhs) const {
    return strcasecmp(lhs.c_str(), rhs.c_str()) < 0;
}

HttpRequest::HttpRequest(uint8_t version, bool close)
    : m_method(HttpMethod::GET)
    , m_version(version)
    , m_close(close)
    , m_websocket(false)
    , m_path("/") {
}

std::string HttpRequest::get_header(const std::string& key, const std::string& def) {
    auto it = m_headers.find(key);
    return it == m_headers.end() ? def : it->second;
}

std::string HttpRequest::get_param(const std::string& key, const std::string& def) {
    auto it = m_params.find(key);
    return it == m_params.end() ? def : it->second;
}

std::string HttpRequest::get_cookie(const std::string& key, const std::string& def) {
    auto it = m_cookies.find(key);
    return it == m_cookies.end() ? def : it->second;
}

void HttpRequest::set_header(const std::string& key, const std::string& val) {
    m_headers[key] = val;
}

void HttpRequest::set_param(const std::string& key, const std::string& val) {
    m_params[key] = val;
}

void HttpRequest::set_cookie(const std::string& key, const std::string& val) {
    m_cookies[key] = val;
}

void HttpRequest::del_header(const std::string& key) {
    m_headers.erase(key);
}

void HttpRequest::del_param(const std::string& key) {
    m_params.erase(key);
}

void HttpRequest::del_cookie(const std::string& key) {
    m_cookies.erase(key);
}

bool HttpRequest::has_header(const std::string& key, const std::string* val) {
    auto it = m_headers.find(key);
    if (it == m_headers.end()) {
        return false;
    }
    val = &(it->second);
    return true;
}

bool HttpRequest::has_param(const std::string& key, const std::string* val) {
    auto it = m_params.find(key);
    if (it == m_params.end()) {
        return false;
    }
    val = &(it->second);
    return true;
}

bool HttpRequest::has_cookie(const std::string& key, const std::string* val) {
    auto it = m_cookies.find(key);
    if (it == m_cookies.end()) {
        return false;
    }
    val = &(it->second);
    return true;
}

std::ostream& HttpRequest::dump(std::ostream& os) const {
    // GET /index.html?a=b#x HTTP/1.1
    // Host : baidu.com
    // clang-format off
    os << http_method_to_string(m_method) << " "
       << m_path
       << (m_query.empty() ? "" : "?")
       << m_query
       << (m_fragment.empty() ? "" : "#")
       << m_fragment << " "
       << "HTTP/"
       << (static_cast<uint32_t>(m_version) >> 4)
       << "."
       << (static_cast<uint32_t>(m_version) & 0xF)
       << "\r\n";
    // clang-format on
    if (!m_websocket) {
        os << "connection: " << (m_close ? "close" : "keep-alive") << "\r\n";
    }

    for (auto& header : m_headers) {
        if (!m_websocket && strcasecmp(header.first.c_str(), "connection") == 0) {
            continue;
        }
        os << header.first << ": " << header.second << "\r\n";
    }

    // TODO cookie
    if (m_body.empty()) {
        os << "\r\n";
    }
    else {
        // clang-format off
        os << "Content-Length: " << m_body.size() << "\r\n"
           << "\r\n"
           << m_body;
        // clang-format on
    }

    return os;
}

std::string HttpRequest::to_string() const {
    std::stringstream ss;
    dump(ss);
    return ss.str();
}

void HttpRequest::init() {
    std::string connection = get_header("Connection");
    if (connection.empty()) {
        if (strcasecmp(connection.c_str(), "keep-alive") == 0) {
            m_close = false;
        }
        else {
            m_close = true;
        }
    }
}

HttpResponse::HttpResponse(uint8_t version, bool close)
    : m_status(HttpStatus::OK), m_version(version), m_close(close), m_websocket(false) {
}

const std::string& HttpResponse::get_header(const std::string& key, const std::string& def) const {
    auto it = m_headers.find(key);
    return it == m_headers.end() ? def : it->second;
}

void HttpResponse::set_header(const std::string& key, const std::string& val) {
    m_headers[key] = val;
}

void HttpResponse::del_header(const std::string& key) {
    m_headers.erase(key);
}

bool HttpResponse::has_header(const std::string& key, std::string* val) {
    auto it = m_headers.find(key);
    if (it == m_headers.end()) {
        return false;
    }
    val = &(it->second);
    return true;
}
std::ostream& HttpResponse::dump(std::ostream& os) const {
    // HTTP/1.1 200 OK
    // server: acid
    // clang-format off
    os  << "HTTP/"
        << (static_cast<uint32_t>(m_version) >> 4)
        << "."
        << (static_cast<uint32_t>(m_version) & 0xF) << " "
        << static_cast<uint32_t>(m_status) << " "
        << (m_reason.empty() ? http_status_to_string(m_status) : m_reason)
        << "\r\n";
    // clang-format on
    if (!m_websocket) {
        os << "Connection: " << (m_close ? "close" : "keep-alive") << "\r\n";
    }

    for (auto& header : m_headers) {
        if (!m_websocket && strcasecmp(header.first.c_str(), "connection") == 0) {
            continue ;
        }
        os << header.first << ": " << header.second << "\r\n";
    }
    // TODO Cookies
    if (m_body.empty()) {
        os << "\r\n";
    }
    else {
        if (get_header("Content-Length", "").empty()) {
            os << "Content-Length: " << m_body.size() << "\r\n";
        }
        os << "\r\n" << m_body;
    }

    return os;
}

std::string HttpResponse::to_string() const {
    std::stringstream ss;
    dump(ss);
    return ss.str();
}

}  // namespace acid::http