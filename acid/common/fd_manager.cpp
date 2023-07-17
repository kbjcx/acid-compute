//
// Created by llz on 23-7-1.
//

#include "fd_manager.h"
#include "hook.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

namespace acid {

    FdCtx::FdCtx(int fd)
    : m_is_init(false)
    , m_is_socket(false)
    , m_sys_nonblock(false)
    , m_user_nonblock(false)
    , m_is_closed(false)
    , m_fd(fd)
    , m_recv_timeout(-1)
    , m_send_timeout(-1) {
        init();
    }

    FdCtx::~FdCtx() {

    }

    bool FdCtx::init() {
        if (m_is_init) {
            return true;
        }
        m_recv_timeout = -1;
        m_send_timeout = -1;

        struct stat fd_stat;
        if (fstat(m_fd, &fd_stat) == -1) {
            m_is_init = false;
            m_is_socket = false;
        }
        else {
            m_is_init = true;
            m_is_socket = S_ISSOCK(fd_stat.st_mode);
        }

        if (m_is_socket) {
            int flags = fcntl_f(m_fd, F_GETFL, 0);
            if (!(flags & O_NONBLOCK)) {
                fcntl_f(m_fd, F_SETFL, flags | O_NONBLOCK);
            }
            m_sys_nonblock = true;
        }
        else {
            m_sys_nonblock = false;
        }

        m_user_nonblock = false;
        m_is_closed = false;
        return m_is_init;
    }

    void FdCtx::set_timeout(int type, uint64_t ms) {
        if (type == SO_RCVTIMEO) {
            m_recv_timeout = ms;
        }
        else {
            m_send_timeout = ms;
        }
    }

    uint64_t FdCtx::get_timeout(int type) {
        if (type == SO_RCVTIMEO) {
            return m_recv_timeout;
        }
        else {
            return m_send_timeout;
        }
    }

    FdManager::FdManager() {
        m_fd_ctxs.resize(64);
    }

    FdCtx::ptr FdManager::get(int fd, bool auto_create) {
        if (fd == -1) {
            return nullptr;
        }
        ReadLockGuard lock(m_mutex);
        if (static_cast<int>(m_fd_ctxs.size()) <= fd) {
            if (auto_create == false) {
                return nullptr;
            }
        }
        else {
            if (m_fd_ctxs[fd] || auto_create == false) {
                return m_fd_ctxs[fd];
            }
        }
        lock.unlock();

        WriteLockGuard lock2(m_mutex);
        FdCtx::ptr ctx(new FdCtx(fd));
        if (fd >= static_cast<int>(m_fd_ctxs.size())) {
            printf("fd context resize：%d \n", fd);
            m_fd_ctxs.resize(fd * 1.5);
        }
        m_fd_ctxs[fd] = ctx;
        return ctx;
    }

    void FdManager::del(int fd) {
        WriteLockGuard lock(m_mutex);
        if (static_cast<int>(m_fd_ctxs.size()) <= fd) {
            return;
        }
        m_fd_ctxs[fd].reset();
    }

} // namespace acid