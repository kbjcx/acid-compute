/*!
 * @file servlet.h
 * @author kbjcx(lulu5v@163.com)
 * @brief Servlet封装
 * @version 0.1
 * @date 2023-07-07
 *
 * @copyright Copyright (c) 2023
 */
#ifndef DF_SERVLET_H
#define DF_SERVLET_H

#include "http.h"
#include "http_session.h"
#include "acid/common/util.h"

namespace acid::http {

class Servlet {
public:
    using ptr = std::shared_ptr<Servlet>;

    explicit Servlet(const std::string& name) : m_name(name) {

    }

    virtual ~Servlet() = default;

    virtual int32_t handle(HttpRequest::ptr request, HttpResponse::ptr response,
                           HttpSession::ptr session) = 0;

    const std::string& get_name() const {
        return m_name;
    }

protected:
    std::string m_name;
};

class FunctionServlet : public Servlet {
public:
    using ptr = std::shared_ptr<FunctionServlet>;
    using callback = std::function<int32_t(HttpRequest::ptr request, HttpResponse::ptr response,
                                           HttpSession::ptr session)>;

    explicit FunctionServlet(callback cb);

    int32_t handle(HttpRequest::ptr request, HttpResponse::ptr response,
                           HttpSession::ptr session) override;

private:
    callback m_cb;
};

class IServletCreator {
public:
    using ptr = std::shared_ptr<IServletCreator>;

    virtual ~IServletCreator() = default;

    virtual Servlet::ptr get() const = 0;

    virtual std::string get_name() const = 0;
};

class HoldServletCreator : public IServletCreator {
public:
    using ptr = std::shared_ptr<HoldServletCreator>;

    explicit HoldServletCreator(Servlet::ptr servlet)
    : m_servlet(servlet) {

    }

    Servlet::ptr get() const override {
        return m_servlet;
    }

    std::string get_name() const override {
        return m_servlet->get_name();
    }

private:
    Servlet::ptr m_servlet;
};

template <class T>
class ServletCreator : public IServletCreator {
    using ptr = std::shared_ptr<ServletCreator>;

    ServletCreator() {

    }

    Servlet::ptr get() const override {
        return Servlet::ptr(new T);
    }

    std::string get_name() const override {
        return type_to_name<T>();
    }
};

class ServletDispatch : public Servlet {
public:
    using ptr = std::shared_ptr<ServletDispatch>;

    using RwMutexType = RWMutex;
    using ReadLockGuard = RwMutexType::ReadLock;
    using WriteLockGuard = RwMutexType::WriteLock;

    ServletDispatch();

    int32_t handle(HttpRequest::ptr request, HttpResponse::ptr response,
                   HttpSession::ptr session) override;

    void add_servlet(const std::string& uri, Servlet::ptr servlet);

    void add_servlet(const std::string& uri, FunctionServlet::callback cb);

    void add_glob_servlet(const std::string& uri, Servlet::ptr servlet);

    void add_glob_servlet(const std::string& uri, FunctionServlet::callback cb);

    void add_servlet_creator(const std::string& uri, IServletCreator::ptr creator);

    void add_glob_servlet_creator(const std::string& uri, IServletCreator::ptr creator);

    template <class T>
    void add_servlet_creator(const std::string& uri) {
        add_servlet_creator(uri, std::make_shared<ServletCreator<T>>());
    }

    template <class T>
    void add_glob_servlet_creator(const std::string& uri) {
        add_glob_servlet_creator(uri, std::make_shared<ServletCreator<T>>());
    }

    void del_servlet(const std::string& uri);

    void del_glob_servlet(const std::string& uri);

    Servlet::ptr get_default() const {
        return m_default;
    }

    void set_default(Servlet::ptr servlet) {
        m_default = servlet;
    }

    Servlet::ptr get_servlet(const std::string& uri);

    Servlet::ptr get_glob_servlet(const std::string& uri);

    Servlet::ptr get_matched_servlet(const std::string& uri);

    void list_all_servlet_creator(std::map<std::string, IServletCreator::ptr>& infos);
    void list_all_glob_servlet_creator(std::map<std::string, IServletCreator::ptr>& infos);

private:
    RwMutexType m_mutex;
    std::unordered_map<std::string, IServletCreator::ptr> m_datas;
    std::vector<std::pair<std::string, IServletCreator::ptr>> m_globs;
    Servlet::ptr m_default;
};

class NotFoundServlet : public Servlet {
public:
    using ptr = std::shared_ptr<NotFoundServlet>;

    NotFoundServlet(const std::string& name);

    int32_t handle(HttpRequest::ptr request, HttpResponse::ptr response,
                   HttpSession::ptr session) override;

private:
    std::string m_name;
    std::string m_content;
};


}  // namespace acid::http

#endif  // DF_SERVLET_H
