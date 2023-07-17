#include "http_parse.h"

#include "acid/logger/logger.h"

namespace acid::http {

static auto logger = GET_LOGGER_BY_NAME("system");

static uint64_t s_http_request_buffer_size = 1024;
static uint64_t s_http_request_max_body_size = 1024;
static uint64_t s_http_response_buffer_size = 1024;
static uint64_t s_http_response_max_body_size = 1024;

uint64_t HttpRequestParser::get_http_request_buffer_size() {
    return s_http_request_buffer_size;
}

uint64_t HttpRequestParser::get_http_request_max_body_size() {
    return s_http_request_max_body_size;
}

uint64_t HttpResponseParser::get_http_response_buffer_size() {
    return s_http_response_buffer_size;
}

uint64_t HttpResponseParser::get_http_response_max_body_size() {
    return s_http_response_max_body_size;
}

static int on_request_message_begin_cb(http_parser* hp) {
    LOG_DEBUG(logger) << "on_request_message_begin_cb";
    return 0;
}

static int on_request_headers_complete_cb(http_parser* hp) {
    LOG_DEBUG(logger) << "on_request_headers_complete_cb";
    auto* parser = static_cast<HttpRequestParser*>(hp->data);
    parser->get_data()->set_version((hp->http_major << 4) | (hp->http_major));
    parser->get_data()->set_method(static_cast<HttpMethod>(hp->method));
    return 0;
}

static int on_request_message_complete_cb(http_parser* hp) {
    LOG_DEBUG(logger) << "on_request_message_complete_cb";
    auto* parser = static_cast<HttpRequestParser*>(hp->data);
    parser->set_finished(true);
    return 0;
}

static int on_request_chunk_header_cb(http_parser* hp) {
    LOG_DEBUG(logger) << "on request chunk header cb";
    return 0;
}

static int on_request_chunk_complete_cb(http_parser* hp) {
    LOG_DEBUG(logger) << "on_request_chunk_complete_cb";
    return 0;
}

static int on_request_url_cb(http_parser* hp, const char* buffer, size_t len) {
    LOG_DEBUG(logger) << "on_request_url_cb, url is: " << std::string(buffer, len);
    int ret;
    http_parser_url url_parser {};
    auto* parser = static_cast<HttpRequestParser*>(hp->data);

    http_parser_url_init(&url_parser);
    ret = http_parser_parse_url(buffer, len, 0, &url_parser);
    if (ret != 0) {
        LOG_DEBUG(logger) << "parse url fail";
        return 1;
    }
    if (url_parser.field_set & (1 << UF_PATH)) {
        parser->get_data()->set_path(std::string(buffer + url_parser.field_data[UF_PATH].off,
                                                 url_parser.field_data[UF_PATH].len));
    }
    if (url_parser.field_set & (1 << UF_QUERY)) {
        parser->get_data()->set_query(std::string(buffer + url_parser.field_data[UF_QUERY].off,
                                                  url_parser.field_data[UF_QUERY].len));
    }
    if (url_parser.field_set & (1 << UF_FRAGMENT)) {
        parser->get_data()->set_fragment(
            std::string(buffer + url_parser.field_data[UF_FRAGMENT].off,
                        url_parser.field_data[UF_FRAGMENT].len));
    }
    return 0;
}

static int on_request_header_field_cb(http_parser* hp, const char* buffer, size_t len) {
    std::string field(buffer, len);
    LOG_DEBUG(logger) << "on request header field cb, field is " << field;
    auto* parser = static_cast<HttpRequestParser*>(hp->data);
    parser->set_field(field);
    return 0;
}

static int on_request_header_value_cb(http_parser* hp, const char* buffer, size_t len) {
    std::string value(buffer, len);
    LOG_DEBUG(logger) << "on_request_header_value_cb, value is " << value;
    auto* parser = static_cast<HttpRequestParser*>(hp->data);
    parser->get_data()->set_header(parser->get_field(), value);
    return 0;
}

static int on_request_status_cb(http_parser* hp, const char* buffer, size_t len) {
    LOG_DEBUG(logger) << "on_request_status_cb, should not happen";
    return 0;
}

static int on_request_body_cb(http_parser* hp, const char* buffer, size_t len) {
    std::string body(buffer, len);
    LOG_DEBUG(logger) << "on request body cb, body is " << body;
    auto* parser = static_cast<HttpRequestParser*>(hp->data);
    parser->get_data()->set_body(body);
    return 0;
}

static http_parser_settings s_request_settings = {
    .on_message_begin = on_request_message_begin_cb,
    .on_url = on_request_url_cb,
    .on_status = on_request_status_cb,
    .on_header_field = on_request_header_field_cb,
    .on_header_value = on_request_header_value_cb,
    .on_headers_complete = on_request_headers_complete_cb,
    .on_body = on_request_body_cb,
    .on_message_complete = on_request_message_complete_cb,
    .on_chunk_header = on_request_chunk_header_cb,
    .on_chunk_complete = on_request_chunk_complete_cb,
};

HttpRequestParser::HttpRequestParser() {
    http_parser_init(&m_parser, HTTP_REQUEST);
    m_data.reset(new HttpRequest);
    m_parser.data = this;
    m_error = 0;
    m_finished = false;
}

size_t HttpRequestParser::execute(char* data, size_t len) {
    size_t nparsed = http_parser_execute(&m_parser, &s_request_settings, data, len);
    if (m_parser.upgrade) {
        // 处理新协议，暂时不处理
        LOG_DEBUG(logger) << "found upgrade, ignore";
        set_error(HPE_UNKNOWN);
    }
    else if (m_parser.http_errno != 0) {
        LOG_DEBUG(logger) << "parse request fail: " << http_errno_name(HTTP_PARSER_ERRNO(&m_parser));
        set_error(static_cast<int8_t>(m_parser.http_errno));
    }
    else {
        if (nparsed < len) {
            memmove(data, data + nparsed, (len - nparsed));
        }
    }

    return nparsed;
}

static int on_response_message_begin_cb(http_parser* hp) {
    LOG_DEBUG(logger) << "on_response_message_begin_cb";
    return 0;
}

static int on_response_headers_complete_cb(http_parser* hp) {
    LOG_DEBUG(logger) << "on_response_headers_complete_cb";
    auto* parser = static_cast<HttpResponseParser*>(hp->data);
    parser->get_data()->set_version((hp->http_major << 4) | (hp->http_major));
    parser->get_data()->set_status(static_cast<HttpStatus>(hp->status_code));
    return 0;
}

static int on_response_message_complete_cb(http_parser* hp) {
    LOG_DEBUG(logger) << "on response message complete cb";
    auto* parser = static_cast<HttpResponseParser*>(hp->data);
    parser->set_finished(true);
    return 0;
}

static int on_response_chunk_header_cb(http_parser* hp) {
    LOG_DEBUG(logger) << "on response chunk header cb";
    return 0;
}

static int on_response_chunk_complete_cb(http_parser* hp) {
    LOG_DEBUG(logger) << "on response chunk complete cb";
    return 0;
}

static int on_response_url_cb(http_parser* hp, const char* buffer, size_t len) {
    LOG_DEBUG(logger) << "on response url cb, should not happen";
    return 0;
}

static int on_response_header_field_cb(http_parser* hp, const char* buffer, size_t len) {
    std::string field(buffer, len);
    LOG_DEBUG(logger) << "on response header field cb, field id " << field;
    auto* parser = static_cast<HttpResponseParser*>(hp->data);
    parser->set_field(field);
    return 0;
}

static int on_response_header_value_cb(http_parser* hp, const char* buffer, size_t len) {
    std::string value(buffer, len);
    LOG_DEBUG(logger) << "on response header value cb, value is " << value;
    auto* parser = static_cast<HttpResponseParser*>(hp->data);
    parser->get_data()->set_header(parser->get_field(), value);
    return 0;
}

static int on_response_status_cb(http_parser* hp, const char* buffer , size_t len) {
    LOG_DEBUG(logger) << "on response status cb, status code is " << hp->status_code
    << "status message is " << std::string(buffer, len);
    auto* parser = static_cast<HttpResponseParser*>(hp->data);
    parser->get_data()->set_status(static_cast<HttpStatus>(hp->status_code));
    return 0;
}

static int on_response_body_cb(http_parser* hp, const char* buffer, size_t len) {
    std::string body(buffer, len);
    LOG_DEBUG(logger) << "on response body cb, body is " << body;
    auto* parser = static_cast<HttpResponseParser*>(hp->data);
    parser->get_data()->set_body(body);
    return 0;
}

static http_parser_settings s_response_settings = {
    .on_message_begin = on_response_message_begin_cb,
    .on_url = on_response_url_cb,
    .on_status = on_response_status_cb,
    .on_header_field = on_response_header_field_cb,
    .on_header_value = on_response_header_value_cb,
    .on_headers_complete = on_response_headers_complete_cb,
    .on_body = on_response_body_cb,
    .on_message_complete = on_response_message_complete_cb,
    .on_chunk_header = on_response_chunk_header_cb,
    .on_chunk_complete = on_response_chunk_complete_cb
};

HttpResponseParser::HttpResponseParser() {
    http_parser_init(&m_parser, HTTP_RESPONSE);
    m_data.reset(new HttpResponse);
    m_parser.data = this;
    m_error = 0;
    m_finished = false;
}

size_t HttpResponseParser::execute(char* data, size_t len) {
    size_t nparsed = http_parser_execute(&m_parser, &s_response_settings, data, len);
    if (m_parser.http_errno != 0) {
        LOG_DEBUG(logger) << "parse response fail: " << http_errno_name(HTTP_PARSER_ERRNO(&m_parser));
        set_error(static_cast<int8_t>(m_parser.http_errno));
    }
    else {
        if (nparsed < len) {
            memmove(data, data + nparsed, (len - nparsed));
        }
    }
    return nparsed;
}

}  // namespace acid::http