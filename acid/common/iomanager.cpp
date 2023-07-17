#include "iomanager.h"

#include "acid/logger/logger.h"

#include <cassert>
#include <cstring>
#include <fcntl.h>
#include <ostream>
#include <stdexcept>
#include <sys/epoll.h>

namespace acid {

static auto logger = GET_LOGGER_BY_NAME("system");

enum class EpollCtlOp {

};

static std::ostream &operator<<(std::ostream &os, const EpollCtlOp &op) {
    switch (static_cast<int>(op)) {
#define XX(ctl) \
    case ctl:   \
        return os << #ctl
        XX(EPOLL_CTL_ADD);
        XX(EPOLL_CTL_MOD);
        XX(EPOLL_CTL_DEL);
#undef XX
        default:
            return os << static_cast<int>(op);
    }
}

static std::ostream &operator<<(std::ostream &os, EPOLL_EVENTS events) {
    if (!events) {
        return os << "0";
    }
    bool first = true;
#define XX(E)                  \
    if (events & E) {          \
        if (!first) os << "|"; \
        os << #E;              \
        first = false;         \
    }

    XX(EPOLLIN)
    XX(EPOLLPRI)
    XX(EPOLLOUT)
    XX(EPOLLRDNORM)
    XX(EPOLLRDBAND)
    XX(EPOLLWRNORM)
    XX(EPOLLWRBAND)
    XX(EPOLLMSG)
    XX(EPOLLERR)
    XX(EPOLLHUP)
    XX(EPOLLRDHUP)
    XX(EPOLLONESHOT)
    XX(EPOLLET)
#undef XX
    return os;
}

IOManager::FdContext::EventContext &IOManager::FdContext::get_event_context(
    IOManager::Event event) {
    switch (event) {
        case IOManager::Event::READ:
            return read;
        case IOManager::Event::WRITE:
            return write;
        default:
            assert(false);
    }
    throw std::invalid_argument(
        "IOManager::FdContext::EventContext& "
        "IOManager::FdContext::get_event_context(IOManager::Event event)");
}

void IOManager::FdContext::reset_event_context(IOManager::FdContext::EventContext &ctx) {
    ctx.scheduler = nullptr;
    ctx.fiber.reset();
    ctx.callback = nullptr;
}

void IOManager::FdContext::trigger_event(IOManager::Event event) {
    // TODO 保持事件是否效率更高
    // 待触发的事件必须被注册过
    assert(events & event);
    // 注册的IO事件是一次性的, 如果希望一直监听该事件, 则需要在触发后重新添加
    events = static_cast<IOManager::Event>(events & ~event);
    // 调度对应的协程
    EventContext &ctx = get_event_context(event);
    if (ctx.callback) {
        ctx.scheduler->schedule(ctx.callback);
    }
    else {
        ctx.scheduler->schedule(ctx.fiber);
    }
    reset_event_context(ctx);
}

IOManager::IOManager(size_t threads, bool use_caller, const std::string &name)
    : Scheduler(threads, use_caller, name), m_epollfd(-1), m_pending_event_count(0) {
    m_epollfd = epoll_create1(EPOLL_CLOEXEC);
    assert(m_epollfd > 0);

    int ret = pipe(m_tick_fds);
    assert(ret == 0);

    // 关注管道的可读事件
    epoll_event event {};
    memset(&event, 0, sizeof(event));
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = m_tick_fds[0];

    // 设置管道非阻塞方式
    int flags = fcntl(m_tick_fds[0], F_GETFL, 0);
    ret = fcntl(m_tick_fds[0], F_SETFL, O_NONBLOCK | flags);
    assert(ret == 0);

    ret = epoll_ctl(m_epollfd, EPOLL_CTL_ADD, m_tick_fds[0], &event);
    assert(ret == 0);

    context_resize(32);

    start();
}

IOManager::~IOManager() {
    stop();
    close(m_epollfd);
    close(m_tick_fds[0]);
    close(m_tick_fds[1]);
    for (size_t i = 0; i < m_fd_contexts.size(); ++i) {
        if (m_fd_contexts[i]) {
            delete m_fd_contexts[i];
        }
    }
}

void IOManager::context_resize(size_t size) {
    m_fd_contexts.resize(size);
    for (size_t i = 0; i < m_fd_contexts.size(); ++i) {
        if (!m_fd_contexts[i]) {
            m_fd_contexts[i] = new FdContext;
            m_fd_contexts[i]->fd = i;
        }
    }
}

int IOManager::add_event(int fd, Event event, std::function<void()> cb) {
    // 找到fd对应的FdContext, 没有则分配一个
    FdContext *fd_ctx = nullptr;
    ReadLockGuard lock(m_mutex);
    if (static_cast<int>(m_fd_contexts.size()) > fd) {
        fd_ctx = m_fd_contexts[fd];
        lock.unlock();
    }
    else {
        lock.unlock();
        WriteLockGuard lock2(m_mutex);
        context_resize(static_cast<size_t>(fd * 1.5));
        fd_ctx = m_fd_contexts[fd];
    }
    // 同一个fd不允许添加相同的事件

    FdContext::LockGuard lock3(fd_ctx->mutex);
    if (fd_ctx->events & event) [[unlikely]] {
        LOG_ERROR(logger) << "IOManager::add_event assert fd = " << fd
                          << "event = " << (EPOLL_EVENTS) event
                          << "fd_ctx.events = " << (EPOLL_EVENTS) fd_ctx->events;
        assert(!(fd_ctx->events & event));
    }

    // 将新的事件加入epoll, 使用epoll_event的私有指针存储FdContext的位置
    int op = fd_ctx->events ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;
    epoll_event ev {};
    ev.events = EPOLLET | fd_ctx->events | event;
    ev.data.ptr = fd_ctx;

    int ret = epoll_ctl(m_epollfd, op, fd, &ev);
    if (ret) {
        LOG_ERROR(logger) << "epoll_ctl(" << m_epollfd << ", " << (EpollCtlOp) op << ", " << fd
                          << ", " << (EPOLL_EVENTS) ev.events << "): " << ret << " (" << errno
                          << ") (" << strerror(errno)
                          << ") fd_ctx->events = " << (EPOLL_EVENTS) fd_ctx->events;
        return -1;
    }
    // 待执行的IO事件数量+1
    ++m_pending_event_count;
    // 找到fd对应的事件上下文
    fd_ctx->events = (Event) (fd_ctx->events | event);
    FdContext::EventContext &event_ctx = fd_ctx->get_event_context(event);
    assert(!event_ctx.scheduler && !event_ctx.fiber && !event_ctx.callback);

    // 赋值scheduler和回调函数, 如果回调函数为空, 则把当前协程当成回调执行体
    event_ctx.scheduler = Scheduler::get_this();
    if (cb) {
        // 有回调就将回调加入调度
        event_ctx.callback.swap(cb);
    }
    else {
        // 没有回调则将线程主协程作为事件
        event_ctx.fiber = Fiber::get_this();
        assert(event_ctx.fiber->get_state() == Fiber::State::RUNNING);
    }
    return 0;
}

bool IOManager::del_event(int fd, acid::IOManager::Event event) {
    ReadLockGuard lock(m_mutex);
    if (static_cast<int>(m_fd_contexts.size()) <= fd) {
        return false;
    }

    FdContext *fd_ctx = m_fd_contexts[fd];
    lock.unlock();

    FdContext::LockGuard lock2(fd_ctx->mutex);
    if (!(fd_ctx->events & event)) [[unlikely]] {
        return false;
    }

    // 清除指定的事件，表示不关心这个事件了，如果清除之后结果为0，则从epoll_wait中删除该文件描述符
    Event new_events = static_cast<Event>(fd_ctx->events & ~event);
    int op = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
    epoll_event ev {};
    ev.events = EPOLLET | new_events;
    ev.data.ptr = fd_ctx;

    int ret = epoll_ctl(m_epollfd, op, fd, &ev);
    if (ret) {
        LOG_ERROR(logger) << "epoll_ctl(" << m_epollfd << ", " << (EpollCtlOp) op << ", " << fd
                          << ", " << (EPOLL_EVENTS) ev.events << "):" << ret << " (" << errno
                          << ") (" << strerror(errno) << ")";
        return false;
    }

    --m_pending_event_count;
    fd_ctx->events = new_events;
    // 把删除的事件上下文重置
    FdContext::EventContext &event_ctx = fd_ctx->get_event_context(event);
    FdContext::reset_event_context(event_ctx);
    return true;
}

bool IOManager::cancel_event(int fd, acid::IOManager::Event event) {
    ReadLockGuard lock(m_mutex);
    if (static_cast<int>(m_fd_contexts.size()) <= fd) {
        return false;
    }
    FdContext *fd_ctx = m_fd_contexts[fd];
    lock.unlock();

    FdContext::LockGuard lock2(fd_ctx->mutex);
    if (!(fd_ctx->events & event)) [[unlikely]] {
        return false;
    }

    // 删除事件
    Event new_events = static_cast<Event>(fd_ctx->events & ~event);
    int op = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
    epoll_event ev {};
    ev.events = new_events;
    ev.data.ptr = fd_ctx;

    int ret = epoll_ctl(m_epollfd, op, fd, &ev);
    if (ret) {
        LOG_ERROR(logger) << "epoll_ctl(" << m_epollfd << ", " << (EpollCtlOp) op << ", " << fd
                          << ", " << (EPOLL_EVENTS) ev.events << "):" << ret << " (" << errno
                          << ") (" << strerror(errno) << ")";
        return false;
    }

    // 删除之前触发一次事件
    fd_ctx->trigger_event(event);
    --m_pending_event_count;
    return true;
}

bool IOManager::cancel_all(int fd) {
    ReadLockGuard lock(m_mutex);
    if (static_cast<int>(m_fd_contexts.size()) <= fd) {
        return false;
    }

    FdContext *fd_ctx = m_fd_contexts[fd];
    lock.unlock();

    FdContext::LockGuard lock2(fd_ctx->mutex);
    if (!fd_ctx->events) {
        return false;
    }

    // 删除全部事件
    int op = EPOLL_CTL_DEL;
    epoll_event ev {};
    ev.events = Event::NONE;
    ev.data.ptr = fd_ctx;

    int ret = epoll_ctl(m_epollfd, op, fd, &ev);
    if (ret) {
        LOG_ERROR(logger) << "epoll_ctl(" << m_epollfd << ", " << (EpollCtlOp) op << ", " << fd
                          << ", " << (EPOLL_EVENTS) ev.events << "):" << ret << " (" << errno
                          << ") (" << strerror(errno) << ")";
        return false;
    }

    // 触发全部事件
    if (fd_ctx->events & Event::READ) {
        fd_ctx->trigger_event(Event::READ);
        --m_pending_event_count;
    }
    if (fd_ctx->events & Event::WRITE) {
        fd_ctx->trigger_event(Event::WRITE);
        --m_pending_event_count;
    }

    assert(fd_ctx->events == 0);
    return true;
}

IOManager *IOManager::get_this() {
    return dynamic_cast<IOManager *>(Scheduler::get_this());
}

void IOManager::tickle() {
    LOG_DEBUG(logger) << "IOManager::tickle()";
    if (!has_idle_thread()) {
        return;
    }
    auto ret = write(m_tick_fds[1], "T", 1);
    assert(ret == 1);
}

bool IOManager::stopping() {
    uint64_t timeout = 0;
    return stopping(timeout);
}

bool IOManager::stopping(uint64_t &timeout) {
    timeout = get_next_timer();
    return timeout == ~0ull && m_pending_event_count == 0 && Scheduler::stopping();
}

void IOManager::idle() {
    LOG_DEBUG(logger) << "IOManager::idle()";

    // 一次epoll_wait最多检测256个事件, 超过的话在下一轮再检测
    const uint64_t MAX_EVENTS = 256;
    epoll_event *events = new epoll_event[MAX_EVENTS];
    std::shared_ptr<epoll_event> shared_events(events, [](epoll_event *ptr) { delete[] ptr; });

    while (true) {
        // 获取下一个定时器的超时时间, 顺便判断调度器是否停止
        uint64_t next_timeout = 0;
        if (stopping(next_timeout)) [[unlikely]] {
            LOG_DEBUG(logger) << "name = " << get_name() << "idle stopping exit";
            break;
        }

        // 阻塞在epoll_wait上, 等待事件发生或定时器超时
        int ret = 0;
        do {
            // 默认超时时间5秒，如果下一个定时器的超时时间大于5秒，仍以5秒来计算超时，避免定时器超时时间太大时，epoll_wait一直阻塞
            static const int MAX_TIMEOUT = 5000;  // ms
            if (next_timeout != ~0ull) {
                next_timeout = std::min(static_cast<int>(next_timeout), MAX_TIMEOUT);
            }
            else {
                next_timeout = MAX_TIMEOUT;
            }

            ret = epoll_wait(m_epollfd, events, MAX_EVENTS, static_cast<int>(next_timeout));

            if (ret < 0 && errno == EINTR) {
                continue;
            }
            else {
                break;
            }
        } while (true);

        // 收集所有已经超时的事件
        std::vector<std::function<void()>> timeout_callbacks;
        list_expired_callback(timeout_callbacks);
        if (!timeout_callbacks.empty()) {
            for (const auto &cb : timeout_callbacks) {
                schedule(cb);
            }
            timeout_callbacks.clear();
            std::vector<std::function<void()>>().swap(timeout_callbacks);
        }

        // 遍历所有发生的事件，根据epoll_event的私有指针找到对应的FdContext，进行事件处理
        for (int i = 0; i < ret; ++i) {
            epoll_event &ev = events[i];
            if (ev.data.fd == m_tick_fds[0]) {
                // ticklefd[0]用于通知协程调度，这时只需要把管道里的内容读完即可
                uint8_t dummy[256];
                while (read(m_tick_fds[0], dummy, sizeof(dummy)) > 0) {
                }
                continue;
            }

            FdContext *fd_ctx = static_cast<FdContext *>(ev.data.ptr);
            FdContext::LockGuard lock(fd_ctx->mutex);

            /**
             * EPOLLERR: 出错，比如写读端已经关闭的pipe
             * EPOLLHUP: 套接字对端关闭
             * 出现这两种事件，应该同时触发fd的读和写事件，否则有可能出现注册的事件永远执行不到的情况
             */
            if (ev.events & (EPOLLERR | EPOLLHUP)) {
                ev.events |= (EPOLLIN | EPOLLOUT) & fd_ctx->events;
            }
            int real_events = Event::NONE;
            if (ev.events & EPOLLIN) {
                real_events |= Event::READ;
            }
            if (ev.events & EPOLLOUT) {
                real_events |= Event::WRITE;
            }

            if ((fd_ctx->events & real_events) == Event::NONE) {
                continue;
            }

            // TODO 可以用del_event代替
            // 剔除已经发生的事件, 将剩下的事件重新加入epoll_wait
            int left_events = (fd_ctx->events & ~real_events);
            int op = left_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
            ev.events = left_events | EPOLLET;

            int ret2 = epoll_ctl(m_epollfd, op, fd_ctx->fd, &ev);
            if (ret2) {
                LOG_ERROR(logger) << "epoll_ctl(" << m_epollfd << ", " << (EpollCtlOp) op << ", "
                                  << fd_ctx->fd << ", " << (EPOLL_EVENTS) ev.events << "):" << ret2
                                  << " (" << errno << ") (" << strerror(errno) << ")";
                continue;
            }

            // 处理已经发生的事件
            if (real_events & Event::READ) {
                fd_ctx->trigger_event(Event::READ);
                --m_pending_event_count;
            }
            if (real_events & Event::WRITE) {
                fd_ctx->trigger_event(Event::WRITE);
                --m_pending_event_count;
            }
        }  // end for

        Fiber::ptr cur = Fiber::get_this();
        auto raw_ptr = cur.get();
        cur.reset();
        // 切换到调度协程
        raw_ptr->yield();
    }
}

void IOManager::on_timer_inserted_at_front() {
    tickle();
}

}  // namespace acid