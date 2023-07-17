//
// Created by llz on 23-7-1.
//

#include "hook.h"

#include "acid/common/fiber.h"
#include "acid/common/scheduler.h"
#include "acid/logger/logger.h"
#include "fd_manager.h"
#include "iomanager.h"
#include "timer.h"

#include <bits/types/struct_timespec.h>
#include <cstdarg>
#include <dlfcn.h>
#include <sys/types.h>
#include <unistd.h>

static auto logger = GET_LOGGER_BY_NAME("system");

namespace acid {
static uint64_t g_tcp_connect_timeout = 5000;

// 线程局部变量, 是否采用hook
static thread_local bool t_hook_enable = false;

#define HOOKFUN(XX) \
    XX(sleep);      \
    XX(usleep);     \
    XX(nanosleep);  \
    XX(socket);     \
    XX(connect);    \
    XX(accept);     \
    XX(read);       \
    XX(readv);      \
    XX(recv);       \
    XX(recvfrom);   \
    XX(recvmsg);    \
    XX(write);      \
    XX(writev);     \
    XX(send);       \
    XX(sendto);     \
    XX(sendmsg);    \
    XX(close);      \
    XX(fcntl);      \
    XX(ioctl);      \
    XX(getsockopt); \
    XX(setsockopt)

/*!
 * @brief 对各hook函数进行初始化
 * @details dlsym函数可以从共享库里获取符号的地址,
 * RTLD_NEXT表示在当前库之后以默认的方式搜索符号第一次出现的地址(不搜索当前库)
 */
void hook_init() {
    static bool is_inited = false;
    if (is_inited) {
        return;
    }
#define XX(name) name##_f = (name##_func) dlsym(RTLD_NEXT, #name);
    HOOKFUN(XX);
#undef XX
}

static uint64_t s_connect_timeout = -1;

struct _HookIniter {
    _HookIniter() {
        hook_init();
        s_connect_timeout = g_tcp_connect_timeout;
    }
};

// 全局静态变量，在main函数之前初始化，这样就会调用构造函数，保证在main开始前进行hook初始化
static _HookIniter s_hook_initer;

bool is_hook_enable() {
    return t_hook_enable;
}

void set_hook_enable(bool flag) {
    t_hook_enable = flag;
}

}  // namespace acid

// 条件
struct timer_info {
    int cancelled = 0;
};

// 进程IO操作的模板类
template <class OriginFunc, class... Args>
static ssize_t do_io(int fd, OriginFunc func, const char *hook_func_name, uint32_t event,
                     int timeout_so, Args &&...args) {
    if (!acid::t_hook_enable) {
        // 不适用hook, 直接使用原来的系统调用
        LOG_DEBUG(logger) << "do not use hook: " << hook_func_name;
        return func(fd, std::forward<Args>(args)...);
    }

    acid::FdCtx::ptr ctx = acid::FdMgr::instance()->get(fd);
    if (!ctx) {
        // 不存在当前fd的上下文
        LOG_DEBUG(logger) << "不存在当前fd = "<< fd << "的上下文";
        return func(fd, std::forward<Args>(args)...);
    }

    if (ctx->is_close()) {
        // 当前fd已处于关闭状态
        errno = EBADF;  // 表示当前fd已被关闭, 不是有效的fd
        return -1;
    }

    if (!ctx->is_socket() || ctx->get_user_nonblock()) {
        // 当fd不是socket或者用户显式的设置了非阻塞, 就可以直接使用系统调用
        // 因为hook的目的是避免阻塞, 如果设置了非阻塞就没必要通过hook调用
        return func(fd, std::forward<Args>(args)...);
    }

    // 获取对应的超时时间(读或写)
    uint64_t to = ctx->get_timeout(timeout_so);
    std::shared_ptr<timer_info> ti(new timer_info);

retry:
    // 处理非阻塞逻辑
    ssize_t n = func(fd, std::forward<Args>(args)...);
    LOG_DEBUG(logger) << hook_func_name << " return " << n << "on fd = " << fd;
    // 被信号中断, 不属于阻塞状态, 继续io
    while (n == -1 && errno == EINTR) {
        n = func(fd, std::forward<Args>(args)...);
    }

    // 返回值为-1并且errno == EAGAIN或errno == EWOULDBLOCK (EWOULDBLOCK == EAGAIN),
    // 表示缓冲区没数据可以读, 需要阻塞等待, 将其处理成异步任务
    if (n == -1 && errno == EAGAIN) {
        LOG_DEBUG(logger) << hook_func_name << " EAGAIN on fd = " << fd;
        acid::IOManager *iom = acid::IOManager::get_this();
        acid::Timer::ptr timer;
        std::weak_ptr<timer_info> weak_info(ti);

        if (to != static_cast<uint64_t>(-1)) {
            // 有设置超时时间
            timer = iom->add_condition_timer(
                to,
                [weak_info, fd, iom, event]() {
                    auto t = weak_info.lock();
                    if (!t || t->cancelled) {
                        // 如果条件不满足, 则放弃执行任务
                        return;
                    }
                    // 定时器触发，说明这个fd上的事件监听超时了，需要取消事件
                    t->cancelled = ETIMEDOUT;
                    iom->cancel_event(fd, static_cast<acid::IOManager::Event>(event));
                },
                weak_info);
        }
        // 添加监听事件, 任务处理就是当前协程
        int ret = iom->add_event(fd, static_cast<acid::IOManager::Event>(event));
        if (ret) [[unlikely]] {
            // 添加事件失败
            LOG_ERROR(logger) << hook_func_name << " add_event(" << fd << ", " << event << ")";
            if (timer) {
                // 取消上面注册的定时器
                timer->cancel();
            }
            return -1;
        }
        else {
            // 跳出协程
            acid::Fiber::get_this()->yield();
            // resume之后从此开始
            if (timer) {
                // 取消定时器
                timer->cancel();
            }
            if (ti->cancelled) {
                // 是超时后触发的回调, 则返回-1, 并将errno设置为ETIMEDOUT, 表示io超时返回
                errno = ti->cancelled;
                return -1;
            }
            // 监听到事件触发的回调, 则返回retry重新执行io事件
            goto retry;
        }
    }

    // io事件成功则直接返回
    return n;
}

extern "C" {
#define XX(name) name##_func name##_f = nullptr
HOOKFUN(XX);
#undef XX
unsigned int sleep(unsigned int seconds) {
    if (!acid::t_hook_enable) {
        // 没有使用hook则直接使用原系统调用
        return sleep_f(seconds);
    }

    acid::Fiber::ptr fiber = acid::Fiber::get_this();
    acid::IOManager *iom = acid::IOManager::get_this();
    // 类成员函数绑定时需要添加this指针
    iom->add_timer(seconds * 1000, [iom, fiber]() { iom->schedule(fiber, -1); });
    acid::Fiber::get_this()->yield();
    return 0;
}

int usleep(useconds_t usec) {
    if (!acid::t_hook_enable) {
        return usleep_f(usec);
    }

    acid::Fiber::ptr fiber = acid::Fiber::get_this();
    acid::IOManager *iom = acid::IOManager::get_this();

    iom->add_timer(usec / 1000, [iom, fiber]() { iom->schedule(fiber, -1); });
    acid::Fiber::get_this()->yield();
    return 0;
}

int nanosleep(const struct timespec *req, struct timespec *rem) {
    if (!acid::t_hook_enable) {
        return nanosleep_f(req, rem);
    }

    // 精度只能达到毫秒级别
    int timeout_ms = req->tv_sec * 1000 + req->tv_nsec / 1000 / 1000;
    acid::Fiber::ptr fiber = acid::Fiber::get_this();
    acid::IOManager *iom = acid::IOManager::get_this();
    iom->add_timer(timeout_ms, [iom, fiber]() { iom->schedule(fiber, -1); });
    acid::Fiber::get_this()->yield();
    return 0;
}

int socket(int domain, int type, int protocol) {
    if (!acid::t_hook_enable) {
        return socket_f(domain, type, protocol);
    }

    int fd = socket_f(domain, type, protocol);
    if (fd == -1) {
        return fd;
    }
    // 注册到fd管理对象
    acid::FdMgr::instance()->get(fd, true);
    return fd;
}

int connect_with_timeout(int fd, const struct sockaddr *addr, socklen_t addr_len,
                         uint64_t timeout) {
    if (!acid::t_hook_enable) {
        return connect_f(fd, addr, addr_len);
    }

    // 获取fd上下文特征
    acid::FdCtx::ptr ctx = acid::FdMgr::instance()->get(fd);
    if (!ctx || ctx->is_close()) {
        // 不存在有效的fd
        errno = EBADF;
        return -1;
    }

    if (!ctx->is_socket()) {
        // 该fd不是socket
        return connect_f(fd, addr, addr_len);
    }

    if (ctx->get_user_nonblock()) {
        // 用户显式设置了非阻塞
        return connect_f(fd, addr, addr_len);
    }

    // 用户默认为阻塞, 转为非阻塞处理
    int n = connect_f(fd, addr, addr_len);

    if (n == 0) {
        // 连接成功
        return 0;
    }
    else if (n != -1 || errno != EINPROGRESS) {
        return n;
    }

    // 返回值为-1并且errno = EINPROGRESS, 表示连接还未完成, 需要处理

    acid::IOManager *iom = acid::IOManager::get_this();
    acid::Timer::ptr timer;
    std::shared_ptr<timer_info> ti(new timer_info);
    std::weak_ptr<timer_info> w_ti(ti);

    if (timeout != static_cast<uint64_t>(-1)) {
        timer = iom->add_condition_timer(
            timeout,
            [w_ti, fd, iom]() {
                auto t = w_ti.lock();
                if (!t || t->cancelled) {
                    return;
                }
                t->cancelled = ETIMEDOUT;
                iom->cancel_event(fd, acid::IOManager::WRITE);
            },
            w_ti);
    }

    int ret = iom->add_event(fd, acid::IOManager::WRITE);
    if (ret == 0) {
        // 添加成功
        acid::Fiber::get_this()->yield();
        if (timer) {
            timer->cancel();
        }
        if (ti->cancelled) {
            // 超时触发的定时器
            errno = ti->cancelled;
            return -1;
        }
    }
    else {
        // 添加失败
        if (timer) {
            timer->cancel();
        }
        LOG_DEBUG(logger) << "connect addEvent(" << fd << ", WRITE) error";
    }

    // 处理事件唤醒协程后的逻辑, 事件可写, 说明有链接, 需要判断连接是否有error
    int error = 0;
    socklen_t len = sizeof(int);
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) == -1) {
        return -1;
    }
    if (!error) {
        // 无error, 连接成功
        return 0;
    }
    else {
        // 有error, 设置错误码
        errno = error;
        return -1;
    }
}

int connect(int sockfd, const struct sockaddr *addr, socklen_t addr_len) {
    return connect_with_timeout(sockfd, addr, addr_len, acid::s_connect_timeout);
}

int accept(int s, struct sockaddr *addr, socklen_t *addr_len) {
    int fd = do_io(s, accept_f, "accept", acid::IOManager::READ, SO_RCVTIMEO, addr, addr_len);
    if (fd >= 0) {
        acid::FdMgr::instance()->get(fd, true);
    }
    return fd;
}

ssize_t read(int fd, void *buffer, size_t size) {
    return do_io(fd, read_f, "read", acid::IOManager::READ, SO_RCVTIMEO, buffer, size);
}

ssize_t readv(int fd, const struct iovec *iov, int iov_count) {
    return do_io(fd, readv_f, "readv", acid::IOManager::READ, SO_RCVTIMEO, iov, iov_count);
}

ssize_t recv(int sockfd, void *buffer, size_t len, int flags) {
    return do_io(sockfd, recv_f, "recv", acid::IOManager::READ, SO_RCVTIMEO, buffer, len, flags);
}

ssize_t recvfrom(int sockfd, void *buffer, size_t len, int flags, struct sockaddr *addr,
                 socklen_t *addr_len) {
    return do_io(sockfd, recvfrom_f, "recvfrom", acid::IOManager::READ, SO_RCVTIMEO, buffer, len,
                 flags, addr, addr_len);
}

ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags) {
    return do_io(sockfd, recvmsg_f, "recvmsg", acid::IOManager::READ, SO_RCVTIMEO, msg, flags);
}

ssize_t write(int fd, const void *buffer, size_t size) {
    return do_io(fd, write_f, "write", acid::IOManager::WRITE, SO_SNDTIMEO, buffer, size);
}

ssize_t writev(int fd, const struct iovec *iov, int iov_count) {
    return do_io(fd, writev_f, "writev", acid::IOManager::WRITE, SO_SNDTIMEO, iov, iov_count);
}

ssize_t send(int s, const void *buffer, size_t len, int flags) {
    return do_io(s, send_f, "send", acid::IOManager::WRITE, SO_SNDTIMEO, buffer, len, flags);
}

ssize_t sendto(int s, const void *buffer, size_t len, int flags, const struct sockaddr *addr,
               socklen_t addr_len) {
    return do_io(s, sendto_f, "sendto", acid::IOManager::WRITE, SO_SNDTIMEO, buffer, len, flags,
                 addr, addr_len);
}

ssize_t sendmsg(int s, const struct msghdr *msg, int flags) {
    return do_io(s, sendmsg_f, "sendmsg", acid::IOManager::WRITE, SO_SNDTIMEO, msg, flags);
}

int close(int fd) {
    if (!acid::t_hook_enable) {
        return close_f(fd);
    }
    // 先取消事件, 删除fd上下文再关闭文件描述符
    acid::FdCtx::ptr ctx = acid::FdMgr::instance()->get(fd);
    if (ctx) {
        auto iom = acid::IOManager::get_this();
        if (iom) {
            iom->cancel_all(fd);
        }
        acid::FdMgr::instance()->del(fd);
    }
    return close_f(fd);
}

int fcntl(int fd, int cmd, ...) {
    va_list va;
    va_start(va, cmd);
    switch (cmd) {
        case F_SETFL: {
            int arg = va_arg(va, int);
            va_end(va);
            acid::FdCtx::ptr ctx = acid::FdMgr::instance()->get(fd);
            if (!ctx || ctx->is_close() || !ctx->is_socket()) {
                return fcntl_f(fd, cmd, arg);
            }
            ctx->set_user_nonblock(arg & O_NONBLOCK);
            if (ctx->get_sys_nonblock()) {
                arg |= O_NONBLOCK;
            }
            else {
                arg &= ~O_NONBLOCK;
            }
            return fcntl_f(fd, cmd, arg);
        }
        case F_GETFL: {
            va_end(va);
            int arg = fcntl_f(fd, cmd);
            acid::FdCtx::ptr ctx = acid::FdMgr::instance()->get(fd);
            if (!ctx || ctx->is_close() || !ctx->is_socket()) {
                return arg;
            }
            if (ctx->get_user_nonblock()) {
                return arg | O_NONBLOCK;
            }
            else {
                return arg & ~O_NONBLOCK;
            }
        }
        case F_DUPFD:
        case F_DUPFD_CLOEXEC:
        case F_SETFD:
        case F_SETOWN:
        case F_SETSIG:
        case F_SETLEASE:
        case F_NOTIFY:
#ifdef F_SETPIPE_SZ
        case F_SETPIPE_SZ:
#endif
        {
            int arg = va_arg(va, int);
            va_end(va);
            return fcntl_f(fd, cmd, arg);
        }
        case F_GETFD:
        case F_GETOWN:
        case F_GETSIG:
        case F_GETLEASE:
#ifdef F_GETPIPE_SZ
        case F_GETPIPE_SZ:
#endif
        {
            va_end(va);
            return fcntl_f(fd, cmd);
        }
        case F_SETLK:
        case F_SETLKW:
        case F_GETLK: {
            struct flock *arg = va_arg(va, struct flock *);
            va_end(va);
            return fcntl_f(fd, cmd, arg);
        }
        case F_GETOWN_EX:
        case F_SETOWN_EX: {
            struct f_owner_exlock *arg = va_arg(va, struct f_owner_exlock *);
            va_end(va);
            return fcntl_f(fd, cmd, arg);
        }
        default: {
            va_end(va);
            return fcntl_f(fd, cmd);
        }
    }
}

int ioctl(int fd, unsigned long int request, ...) {
    va_list va;
    va_start(va, request);
    void *arg = va_arg(va, void *);
    va_end(va);

    if (FIONBIO == request) {
        bool user_nonblock = static_cast<bool>(*(int *) arg);
        acid::FdCtx::ptr ctx = acid::FdMgr::instance()->get(fd);
        if (!ctx || ctx->is_close() || !ctx->is_socket()) {
            return ioctl_f(fd, request, arg);
        }
        ctx->set_user_nonblock(user_nonblock);
    }
    return ioctl(fd, request, arg);
}

int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen) {
    return getsockopt_f(sockfd, level, optname, optval, optlen);
}

int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen) {
    if (!acid::t_hook_enable) {
        return setsockopt_f(sockfd, level, optname, optval, optlen);
    }

    // 捕获用户设定的读超时和写超时
    if (level == SOL_SOCKET) {
        if (optname == SO_RCVTIMEO || optname == SO_SNDTIMEO) {
            acid::FdCtx::ptr ctx = acid::FdMgr::instance()->get(sockfd);
            if (ctx) {
                const timeval *v = static_cast<const timeval *>(optval);
                ctx->set_timeout(optname, v->tv_sec * 1000 + v->tv_usec / 1000);
            }
        }
    }

    return setsockopt_f(sockfd, level, optname, optval, optlen);
}
}