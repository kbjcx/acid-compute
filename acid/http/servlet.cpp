#include "servlet.h"

#include <fnmatch.h>

namespace acid::http {

FunctionServlet::FunctionServlet(callback cb) : Servlet("FunctionServlet"), m_cb(cb) {
}

int32_t FunctionServlet::handle(HttpRequest::ptr request, HttpResponse::ptr response,
                                HttpSession::ptr session) {
    return m_cb(request, response, session);
}

ServletDispatch::ServletDispatch() : Servlet("ServletDispatch") {
    m_default.reset(new NotFoundServlet("acid"));
}

int32_t ServletDispatch::handle(HttpRequest::ptr request, HttpResponse::ptr response,
                                HttpSession::ptr session) {
    auto servlet = get_matched_servlet(request->get_path());
    if (servlet) {
        servlet->handle(request, response, session);
    }
    return 0;
}

void ServletDispatch::add_servlet(const std::string &uri, Servlet::ptr servlet) {
    WriteLockGuard lock(m_mutex);
    m_datas[uri] = std::make_shared<HoldServletCreator>(servlet);
}

void ServletDispatch::add_servlet_creator(const std::string &uri, IServletCreator::ptr creator) {
    WriteLockGuard lock(m_mutex);
    m_datas[uri] = creator;
}

void ServletDispatch::add_glob_servlet_creator(const std::string &uri,
                                               IServletCreator::ptr creator) {
    WriteLockGuard lock(m_mutex);
    for (auto it = m_globs.begin(); it != m_globs.end(); ++it) {
        if (it->first == uri) {
            m_globs.erase(it);
            break;
        }
    }
    m_globs.emplace_back(uri, creator);
}

void ServletDispatch::add_servlet(const std::string &uri, FunctionServlet::callback cb) {
    WriteLockGuard lock(m_mutex);
    m_datas[uri] = std::make_shared<HoldServletCreator>(std::make_shared<FunctionServlet>(cb));
}

void ServletDispatch::add_glob_servlet(const std::string &uri, Servlet::ptr servlet) {
    WriteLockGuard lock(m_mutex);
    for (auto it = m_globs.begin(); it != m_globs.end(); ++it) {
        if (it->first == uri) {
            m_globs.erase(it);
            break;
        }
    }
    m_globs.emplace_back(uri, std::make_shared<HoldServletCreator>(servlet));
}

void ServletDispatch::add_glob_servlet(const std::string &uri, FunctionServlet::callback cb) {
    add_glob_servlet(uri, std::make_shared<FunctionServlet>(cb));
}

void ServletDispatch::del_servlet(const std::string &uri) {
    WriteLockGuard lock(m_mutex);
    m_datas.erase(uri);
}

void ServletDispatch::del_glob_servlet(const std::string &uri) {
    WriteLockGuard lock(m_mutex);
    for (auto it = m_globs.begin(); it != m_globs.end(); ++it) {
        if (it->first == uri) {
            m_globs.erase(it);
            break;
        }
    }
}

Servlet::ptr ServletDispatch::get_servlet(const std::string &uri) {
    ReadLockGuard lock(m_mutex);
    auto it = m_datas.find(uri);
    return it == m_datas.end() ? nullptr : it->second->get();
}

Servlet::ptr ServletDispatch::get_glob_servlet(const std::string &uri) {
    ReadLockGuard lock(m_mutex);
    for (auto it = m_globs.begin(); it != m_globs.end(); ++it) {
        if (it->first == uri) {
            return it->second->get();
        }
    }
    return nullptr;
}

Servlet::ptr ServletDispatch::get_matched_servlet(const std::string &uri) {
    ReadLockGuard lock(m_mutex);
    auto data_it = m_datas.find(uri);
    if (data_it != m_datas.end()) {
        return data_it->second->get();
    }
    for (auto &i : m_globs) {
        if (fnmatch(i.first.c_str(), uri.c_str(), 0) == 0) {
            return i.second->get();
        }
    }
    return m_default;
}

void ServletDispatch::list_all_servlet_creator(std::map<std::string, IServletCreator::ptr> &infos) {
    ReadLockGuard lock(m_mutex);
    for (auto &i : m_datas) {
        infos[i.first] = i.second;
    }
}

void ServletDispatch::list_all_glob_servlet_creator(
    std::map<std::string, IServletCreator::ptr> &infos) {
    for (auto &i : m_globs) {
        infos[i.first] = i.second;
    }
}

NotFoundServlet::NotFoundServlet(const std::string &name)
    : Servlet("NotFoundServlet"), m_name(name) {
    // clang-format off
    m_content = "<html>"
                "<head>"
                "<title>404 Not Found</title>"
                "</head>"
                "<body>"
                "<center><h1>404 Not Found</h1></center>"
                "<hr>" // 分割线
                "<center>" + name + "</center>"
                "</body>"
                "</html>";
    // clang-format on
}

int32_t NotFoundServlet::handle(HttpRequest::ptr request, HttpResponse::ptr response,
                                HttpSession::ptr session) {
    response->set_status(HttpStatus::NOT_FOUND);
    response->set_header("Server", "acid/1.0");
    response->set_header("Content-Type", http_context_type_to_string(HttpContentType::TEXT_HTML));
    response->set_body(m_content);
    return 0;
}

}  // namespace acid::http