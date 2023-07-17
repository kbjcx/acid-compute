/*!
 * @file http_parse.h
 * @author kbjcx(lulu5v@163.com)
 * @brief
 * @version 0.1
 * @date 2023-07-05
 *
 * @copyright Copyright (c) 2023
 */
#ifndef DF_HTTP_PARSE_H
#define DF_HTTP_PARSE_H

#include "memory"
#include "http.h"
#include "third_party/parser/http_parser.h"

namespace acid::http {

    class HttpRequestParser {
    public:
        using ptr = std::shared_ptr<HttpRequestParser>;

        HttpRequestParser();

        size_t execute(char* data, size_t len);

        int is_finished() const {
            return m_finished;
        }

        void set_finished(bool finished) {
            m_finished = finished;
        }

        int has_error() const {
            return !!m_error;
        }

        void set_error(bool error) {
            m_error = error;
        }

        HttpRequest::ptr get_data() const {
            return m_data;
        }

        const http_parser get_parser() const {
            return m_parser;
        }

        const std::string& get_field() const {
            return m_field;
        }

        void set_field(const std::string& value) {
            m_field = value;
        }

    public:
        static uint64_t get_http_request_buffer_size();

        static uint64_t get_http_request_max_body_size();

    private:
        http_parser m_parser;
        HttpRequest::ptr m_data;
        int m_error;
        bool m_finished;
        std::string m_field;

    };  // class HttpRequestParser

    class HttpResponseParser {
    public:
        using ptr = std::shared_ptr<HttpResponseParser>;

        HttpResponseParser();

        size_t execute(char* data, size_t len);

        int is_finished() const {
            return m_finished;
        }

        void set_finished(bool finished) {
            m_finished = finished;
        }

        int has_error() const {
            return !!m_error;
        }

        void set_error(bool error) {
            m_error = error;
        }

        HttpResponse::ptr get_data() const {
            return m_data;
        }

        const http_parser get_parser() const {
            return m_parser;
        }

        const std::string& get_field() const {
            return m_field;
        }

        void set_field(const std::string& field) {
            m_field = field;
        }

    public:
        static uint64_t get_http_response_buffer_size();

        static uint64_t get_http_response_max_body_size();

    private:
        http_parser m_parser;
        HttpResponse::ptr m_data;
        int m_error;
        bool m_finished;
        std::string m_field;

    }; // class HttpResponseParser

}  // namespace acid

#endif  // DF_HTTP_PARSE_H
