#ifndef DF_ACID_IOMANAGER_H
#define DF_ACID_IOMANAGER_H

#include "acid/common/mutex.h"
#include "scheduler.h"
#include "timer.h"

#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>

namespace acid {

class IOManager : public Scheduler, public TimerManager {
public:
    using ptr = std::shared_ptr<IOManager>;
    using RWMutexType = RWMutex;
    using ReadLockGuard = ReadScopedLockImpl<RWMutexType>;
    using WriteLockGuard = WriteScopedLockImpl<RWMutexType>;

    // 将socket fd的所有事件都归类到读写事件
    enum Event {
        // 无事件
        NONE = 0x0,
        // 读事件
        READ = 0x1,
        // 写事件
        WRITE = 0x4
    };

private:
    /*!
     * @brief socket fd 上下文
     * @details 每个socket fd 都对应一个FdContext, 包含fd的值, fd上监听的事件以及fd上事件的事件上下文
     */
    struct FdContext {
        using MutexType = Mutex;
        using LockGuard = ScopedLockImpl<MutexType>;

        /*!
         * @brief 事件上下文
         * @details fd的每个事件都有一个事件上下文, 保存这个事件的回调函数以及执行回调函数的调度器
         */
        struct EventContext {
            Scheduler* scheduler = nullptr;
            Fiber::ptr fiber;
            std::function<void()> callback;
        };

        /**
         * @brief Get the event context object
         *
         * @param[in] event type
         * @return EventContext&
         */
        EventContext& get_event_context(Event event);

        /**
         * @brief 重置事件上下文
         *
         * @param[in, out] ec 需要重置的事件上下文
         */
        static void reset_event_context(EventContext& ec);

        /**
         * @brief 触发事件
         * @details 根据事件类型调用对应上下文结构中的调度器去调度回调协程或回调函数
         * @param[in] event 事件类型
         */
        void trigger_event(Event event);

        EventContext read;
        EventContext write;
        int fd;
        Event events;
        MutexType mutex;
    };  // struct FdContext

public:
    explicit IOManager(size_t threads = 1, bool use_caller = true, const std::string& name = "IOManager");

    ~IOManager();

    /**
     * @brief 添加事件
     *
     * @param fd 对应的文件描述符
     * @param event 文件描述符需要的事件类型
     * @param cb 事件回调函数, 如果为空, 默认把当前协程当做回调执行体
     * @return int 添加成功返回0, 失败返回-1
     */
    int add_event(int fd, Event event, std::function<void()> cb = nullptr);

    /**
     * @brief 删除事件
     *
     * @param fd 文件描述符
     * @param event 要删除的事件类型
     * @attention 不会触发事件
     * @return true 删除成功
     * @return false 删除失败
     */
    bool del_event(int fd, Event event);

    /**
     * @brief 取消事件
     *
     * @param fd 文件描述符
     * @param event 事件类型
     * @attention 如果该事件注册了回调, 则会触发一次事件
     * @return true 取消成功
     * @return false 取消失败
     */
    bool cancel_event(int fd, Event event);

    /**
     * @brief 取消所有事件
     *
     * @param fd 文件描述符
     * @attention 每个事件都会被触发一次
     * @return true 取消成功
     * @return false 取消失败
     */
    bool cancel_all(int fd);

    static IOManager* get_this();

protected:
    /**
     * @brief 通知调度器有任务需要调度
     * @details 写Pipe让idle协程从epoll_wait退出, yield idle协程之后就可以继续调度任务
     */
    void tickle() override;

    /**
     * @brief 判断是否可以停止
     * @details 条件是协程调度器可以停止并且无可调度IO事件
     * @return true
     * @return false
     */
    bool stopping() override;

    /**
     * @brief idle协程
     * @details 阻塞在epoll_wait, 由tickle或是IO事件唤醒
     */
    void idle() override;

    /**
     * @brief 判断是否可以停止，同时获取最近一个定时器的超时时间
     * @param[out] timeout 最近一个定时器的超时时间，用于idle协程的epoll_wait
     * @return 返回是否可以停止
     */
    bool stopping(uint64_t& timeout);

    /**
     * @brief 当有定时器插入到头部时,需要更新epoll_wait的超时时间
     *
     */
    void on_timer_inserted_at_front() override;

    /**
     * @brief 重置socket句柄上下文的容器大小
     *
     * @param size
     */
    void context_resize(size_t size);

private:
    // epoll_fd
    int m_epollfd;
    // pipe文件句柄, fd[0]为读端, fd[1]为写端
    int m_tick_fds[2];
    // 当前等待执行的IO事件的数量
    std::atomic<size_t> m_pending_event_count;
    RWMutexType m_mutex;
    std::vector<FdContext*> m_fd_contexts;
};

};  // namespace acid

#endif