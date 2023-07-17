/*!
 *@file scheduler.h
 *@brief 协程调度模块
 *@version 0.1
 *@date 2023-06-28
 */
#ifndef DF_SCHEDULER_H
#define DF_SCHEDULER_H
#include "fiber.h"
#include "mutex.h"
#include "thread.h"

#include <list>
#include <memory>

namespace acid {
class Scheduler {
public:
    using ptr = std::shared_ptr<Scheduler>;
    using MutexType = Mutex;
    using LockGuard = ScopedLockImpl<MutexType>;

    /*!
     * @brief 创建调度器
     * param[in] threads 线程数量
     * param[in] use_caller 是否使用当前线程作为调度线程
     * param[in] name 调度器名称
     */
    explicit Scheduler(size_t threads = 1, bool use_caller = true, const std::string& name = "Scheduler");

    virtual ~Scheduler();

    const std::string& get_name() const {
        return m_name;
    }

    /*!
     * @brief 获取当前调度器指针
     */
    static Scheduler* get_this();

    /*!
     * @brief 获取当前线程的主协程
     */
    static Fiber* get_main_fiber();

    /*!
     * @brief 添加调度任务
     * @param[in] fc 协程对象或函数指针
     * param[in] thread 指定运行该任务的线程号, -1表示任意线程
     */
    template <class FiberOrCallback>
    void schedule(FiberOrCallback fc, int thread = -1) {
        bool need_tickle = false;
        {
            LockGuard lock(m_mutex);
            need_tickle = schedule_without_lock(fc, thread);
        }

        if (need_tickle) {
            tickle();
        }
    }

    void start();

    void stop();

protected:
    /*!
     * @brief 通知调度器有任务来了
     */
    virtual void tickle();

    /*!
     * @brief 协程调度函数
     */
    void run();

    /*!
     * @brief 无任务调度时执行idle
     */
    virtual void idle();

    /*!
     * @brief 返回调度器是否可以停止
     */
    virtual bool stopping();

    void set_this();

    bool has_idle_thread() {
        return m_idle_thread_count > 0;
    }

private:
    /*!
     * @brief 添加调度任务, 无锁
     */
    template <class FiberOrCallback>
    bool schedule_without_lock(FiberOrCallback fc, int thread) {
        bool need_tickle = m_tasklist.empty();
        ScheduleTask task(fc, thread);
        if (task.fiber || task.callback) {
            m_tasklist.push_back(task);
        }
        return need_tickle;
    }

private:
    struct ScheduleTask {
        Fiber::ptr fiber;
        std::function<void()> callback;
        int thread;

        ScheduleTask(Fiber::ptr f, int thread) {
            fiber = f;
            this->thread = thread;
        }

        ScheduleTask(Fiber::ptr* f, int thread) {
            fiber.swap(*f);
            this->thread = thread;
        }

        ScheduleTask(std::function<void()> c, int thread) {
            callback = c;
            this->thread = thread;
        }

        ScheduleTask() {
            thread = -1;
        }

        void reset() {
            fiber = nullptr;
            callback = nullptr;
            thread = -1;
        }
    };

private:
    // 协程调度器名字
    std::string m_name;
    // 互斥锁
    MutexType m_mutex;
    // 线程池
    std::vector<Thread::ptr> m_threadpool;
    // 任务队列
    std::list<ScheduleTask> m_tasklist;
    // 线程ID数组
    std::vector<int> m_thread_ids;
    // 工作线程的数量, 不包含caller线程
    size_t m_thread_count;
    // 活跃线程的数量
    std::atomic<size_t> m_active_thread_count;
    // idle线程数
    std::atomic<size_t> m_idle_thread_count;

    // 是否使用主协程所在线程参与调度
    bool m_use_caller;
    Fiber::ptr m_root_fiber;
    int m_root_thread;
    // 是否正在停止调度器
    bool m_stopping;

};  // class Scheduler

}  // namespace acid

#endif  // DF_SCHEDULER_H
