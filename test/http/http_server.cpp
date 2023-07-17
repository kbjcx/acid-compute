#include "acid/acid.h"

#include <iostream>

static auto logger = GET_ROOT_LOGGER();
static auto sys_logger = GET_LOGGER_BY_NAME("system");

#define XX(...) #__VA_ARGS__

acid::IOManager::ptr worker;

void run() {
    logger->set_level(acid::LogLevel::INFO);

    acid::http::HttpServer::ptr server(new acid::http::HttpServer(true));
    acid::Address::ptr addr = acid::Address::look_up_any_ipaddress("0.0.0.0:6027");
    while (!server->bind(addr)) {
        sleep(2);
    }

    auto servlet_dispatch = server->get_servlet_dispatch();
    servlet_dispatch->add_servlet(
        "/acid/xx", [](acid::http::HttpRequest::ptr request, acid::http::HttpResponse::ptr response,
                       acid::http::HttpSession::ptr session) {
            response->set_body(request->to_string());
            return 0;
        });

    servlet_dispatch->add_glob_servlet(
        "/acid/*", [](acid::http::HttpRequest::ptr request, acid::http::HttpResponse::ptr response,
                      acid::http::HttpSession::ptr session) {
            response->set_body("Glob:\r\n" + request->to_string());
            return 0;
        });

    servlet_dispatch->add_glob_servlet(
        "/acidx/*", [](acid::http::HttpRequest::ptr request, acid::http::HttpResponse::ptr response,
                       acid::http::HttpSession::ptr session) {
            // clang-format off
        response->set_body(XX(<html>
                              <head><title> 404 Not Found</title></head>
                              <body>
                              <center><h1> 404 Not Found</h1></center>
                              <hr>
                              <center>nginx/1.16.0</center >
                              </body>
                              </html>
                              < !--a padding to disable MSIE and Chrome friendly error page-- >
                              < !--a padding to disable MSIE and Chrome friendly error page-- >
                              < !--a padding to disable MSIE and Chrome friendly error page-- >
                              < !--a padding to disable MSIE and Chrome friendly error page-- >
                              < !--a padding to disable MSIE and Chrome friendly error page-- >
                              < !--a padding to disable MSIE and Chrome friendly error page-- >));
            // clang-format on
            return 0;
        });

    server->start();
}

int main_run(int argc, char** argv) {
    sys_logger->set_level(acid::LogLevel::DEBUG);
    // sys_logger->add_appender(
    // acid::LogAppender::ptr(new acid::FileLogAppender("system.log", acid::LogLevel::DEBUG)));
    sys_logger->add_appender(acid::LogAppender::ptr(new acid::StdoutLogAppender));
    LOG_INFO(sys_logger) << acid::ProcessInfoMgr::instance()->to_string();
    acid::IOManager io_manager(1, true, "main");
    worker.reset(new acid::IOManager(3, false, "worker"));
    io_manager.schedule(run);
    return 0;
}

int main(int argc, char** argv) {
    sys_logger->set_level(acid::LogLevel::DEBUG);
    sys_logger->add_appender(
        acid::LogAppender::ptr(new acid::FileLogAppender("system.log", acid::LogLevel::DEBUG)));
    // sys_logger->add_appender(acid::LogAppender::ptr(new acid::StdoutLogAppender));
    logger->add_appender(
        acid::LogAppender::ptr(new acid::FileLogAppender("root.log", acid::LogLevel::DEBUG)));
    // logger->add_appender(acid::LogAppender::ptr(new acid::StdoutLogAppender));
    return acid::start_daemon(argc, argv, main_run, true);
}
