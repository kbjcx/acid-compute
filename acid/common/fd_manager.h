//
// Created by llz on 23-7-1.
//

#ifndef DF_FD_MANAGER_H
#define DF_FD_MANAGER_H

#include "singleton.h"
#include "thread.h"

#include <memory>
#include <vector>

namespace acid {

class FdCtx : public std::enable_shared_from_this<FdCtx> {
public:
    using ptr = std::shared_ptr<FdCtx>;

    FdCtx(int fd);

    ~FdCtx();

    /*!
     * @brief 是否初始化完成
     */
    bool is_init() const {
        return m_is_init;
    }

    /*!
     * @brief 是否是socket
     */
    bool is_socket() const {
        return m_is_socket;
    }

    /*!
     * @brief 是否已关闭
     */
    bool is_close() const {
        return m_is_closed;
    }

    /*!
     * @brief 设置用户是否设置了非阻塞
     */
    void set_user_nonblock(bool v) {
        m_user_nonblock = v;
    }

    /*!
     * @brief 获取用户是否设置了非阻塞
     */
    bool get_user_nonblock() const {
        return m_user_nonblock;
    }

    /*!
     * @brief 设置系统是否非阻塞
     */
    void set_sys_nonblock(bool v) {
        m_sys_nonblock = v;
    }

    /*!
     * @brief 获取系统是否非阻塞
     */
    bool get_sys_nonblock() const {
        return m_sys_nonblock;
    }

    /*!
     * @brief 设置超时时间
     * @param[in] type 类型SO_RCVTIMEO(读超时), SO_SNDTIMEO(写超时)
     * @param[in] ms 时间ms
     */
    void set_timeout(int type, uint64_t ms);

    /*!
     * @brief 获取超时时间
     * @param[in] type 类型SO_RCVTIMEO(读超时), SO_SNDTIMEO(写超时)
     * @return 超时时间毫秒
     */
    uint64_t get_timeout(int type);

private:
    /*!
     * @brief 初始化
     */
    bool init();

private:
    bool m_is_init : 1;
    bool m_is_socket : 1;
    bool m_sys_nonblock : 1;
    bool m_user_nonblock : 1;
    bool m_is_closed : 1;
    int m_fd;
    uint64_t m_recv_timeout;  // 读超时时间ms
    uint64_t m_send_timeout;  // 写超时时间ms
};

class FdManager {
public:
    using RWMutexType = RWMutex;
    using ReadLockGuard = ReadScopedLockImpl<RWMutexType>;
    using WriteLockGuard = WriteScopedLockImpl<RWMutexType>;

    FdManager();

    FdCtx::ptr get(int fd, bool auto_create = false);

    void del(int fd);

private:
    RWMutexType m_mutex;
    std::vector<FdCtx::ptr> m_fd_ctxs;
};

using FdMgr = Singleton<FdManager>;

}  // namespace acid

#endif  // DF_FD_MANAGER_H
