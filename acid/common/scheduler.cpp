/*!
 *@file scheduler.cpp
 *@brief
 *@version 0.1
 *@date 2023-06-28
 */
#include "scheduler.h"
#include "hook.h"

#include "acid/logger/logger.h"

#include <functional>
#include <cassert>

namespace acid {

static auto logger = GET_LOGGER_BY_NAME("system");

// 当前线程的调度器, 同一个调度器下的所有线程共享一个实例
static thread_local Scheduler* t_scheduler = nullptr;
// 当前线程的调度协程, 每个线程独有一份
static thread_local Fiber* t_scheduler_fiber = nullptr;

Scheduler::Scheduler(size_t threads, bool use_caller, const std::string& name)
    : m_name(name)
    , m_thread_count(0)
    , m_active_thread_count(0)
    , m_idle_thread_count(0)
    , m_use_caller(use_caller)
    , m_root_thread(0)
    , m_stopping(false) {
    assert(threads > 0);

    if (m_use_caller) {
        --threads;
        // 创建线程主协程, 将主协程保存为线程主协程
        Fiber::get_this();
        assert(get_this() == nullptr); // 当前线程不存在调度器才能创建
        t_scheduler = this;

        // 将调度协程先保存起来，用于在调度结束后回到主协程
        m_root_fiber.reset(new Fiber([this] { run(); }, 0, false));

        Thread::set_name(m_name);
        t_scheduler_fiber = m_root_fiber.get();
        // 保存调度协程的线程号
        m_root_thread = get_thread_id();
        m_thread_ids.push_back(m_root_thread);
    }
    else {
        m_root_thread = -1;
    }
    m_thread_count = threads;
}

Scheduler* Scheduler::get_this() {
    return t_scheduler;
}

Fiber* Scheduler::get_main_fiber() {
    // 获取调度协程
    return t_scheduler_fiber;
}

void Scheduler::set_this() {
    // 设置当前线程的调度协程
    t_scheduler = this;
}

Scheduler::~Scheduler() {
    LOG_DEBUG(logger) << "Scheduler::~Scheduler()";
    assert(m_stopping);
    if (get_this() == this) {
        t_scheduler = nullptr;
    }
}

// 创建线程池
void Scheduler::start() {
    LOG_DEBUG(logger) << "Scheduler::start()";
    LockGuard lock(m_mutex);
    if (m_stopping) {
        LOG_ERROR(logger) << "Scheduler is stopped";
        return;
    }
    assert(m_threadpool.empty());
    m_threadpool.resize(m_thread_count);
    for (size_t i = 0; i < m_thread_count; ++i) {
        m_threadpool.at(i).reset(new Thread([this] { run(); }, m_name + "_" + std::to_string(i)));
        m_thread_ids.push_back(m_threadpool.at(i)->get_id());
    }
}

bool Scheduler::stopping() {
    LockGuard lock(m_mutex);
    return m_stopping && m_tasklist.empty() && m_active_thread_count == 0;
}

void Scheduler::tickle() {
    LOG_DEBUG(logger) << "Scheduler::tickle()";
}

// 任务队列无任务，利用idle协程空转等待任务
void Scheduler::idle() {
    LOG_DEBUG(logger) << "Scheduler::idle()";
    while (m_stopping) {
        Fiber::get_this()->yield();
    }
}

void Scheduler::stop() {

    LOG_DEBUG(logger) << "Scheduler::stop()";
    if (stopping()) {
        return;
    }

    m_stopping = true;

    // 如果use caller 只能由caller线程发起stop
    // TODO get_this应该都一样，改为判断线程号吧
    if (m_use_caller) {
        assert(get_this() == this);
    }
    else {
        assert(get_this() != this);
    }

    // 通知线程把剩余的任务做完
    for (size_t i = 0; i < m_thread_count; ++i) {
        tickle();
    }

    if (m_root_fiber) {
        tickle();
    }

    // 在use caller 情况下, 调度协程结束后应该返回主协程
    if (m_root_fiber) {
        // 保存当前的上下文到主协程，并执行m_root_fiber协程执行调度
        // 执行完caller线程的调度器后会跳回caller主协程，即当前点
        m_root_fiber->resume();
        LOG_DEBUG(logger) << "m_root_fiber end";
    }

    std::vector<Thread::ptr> thrs;
    {
        LockGuard lock(m_mutex);
        thrs.swap(m_threadpool);
    }

    for (auto& item : thrs) {
        item->join();
    }
}

void Scheduler::run() {
    LOG_DEBUG(logger) << "Scheduler::run()";
    set_hook_enable(true);
    // 设置调度器
    set_this();
    // 设置调度协程，除了caller线程外，其余线程的调度协程都存储在线程主协程中
    if (get_thread_id() != m_root_thread) {
        t_scheduler_fiber = Fiber::get_this().get();
    }

    Fiber::ptr idle_fiber(new Fiber([this] { idle(); }));
    Fiber::ptr callback_fiber;

    ScheduleTask task;
    while (true) {
        task.reset();
        bool tickle_me = false; // 是否tickle其他线程进行任务调度
        {
            LockGuard lock(m_mutex);
            auto it = m_tasklist.begin();
            while (it != m_tasklist.end()) {
                if (it->thread != -1 && it->thread != get_thread_id()) {
                    // 指定了调度线程, 但是指定的不是当前线程, 标记一下需要通知其他线程进行调度, 然后跳过这个任务, 进行下一个
                    ++it;
                    tickle_me = true;
                    continue;
                }

                // 找到的不是指定线程的任务或是指定当前线程执行的任务
                assert(it->fiber || it->callback);

                // [BUG FIX]: hook IO相关的系统调用时，在检测到IO未就绪的情况下，会先添加对应的读写事件，再yield当前协程，等IO就绪后再resume当前协程
                // 多线程高并发情境下，有可能发生刚添加事件就被触发的情况，如果此时当前协程还未来得及yield，则这里就有可能出现协程状态仍为RUNNING的情况
                // 这里简单地跳过这种情况，以损失一点性能为代价，否则整个协程框架都要大改
                if (it->fiber && it->fiber->get_state() == Fiber::State::RUNNING) {
                    ++it;
                    continue;
                }

                // 当前调度线程找到一个任务, 准备开始调度, 将其从任务队列中删除, 活动线程数+1
                task = *it;
                it = m_tasklist.erase(it);
                ++m_active_thread_count;
                break;
            }
            tickle_me |= (it != m_tasklist.end());
        }

        if (tickle_me) tickle();

        // task是协程类型或是回调函数类型
        if (task.fiber) {
            // resume唤醒协称, 不管是协称yield或是执行完毕都视为完成了当前任务
            task.fiber->resume();
            --m_active_thread_count;
            task.reset();
        }
        else if (task.callback) {
            // 将回调函数包装成协程调用
            if (callback_fiber) {
                callback_fiber->reset(task.callback);
            }
            else {
                callback_fiber.reset(new Fiber(task.callback));
            }
            task.reset();
            callback_fiber->resume();
            --m_active_thread_count;
            callback_fiber.reset();
        }
        else {
            // 任务队列为空
            if (idle_fiber->get_state() == Fiber::State::TERM) {
                // 如果调度器没有调度任务，那么idle协程会不停地resume/yield，不会结束，如果idle协程结束了，那一定是调度器停止了
                LOG_DEBUG(logger) << "idle fiber term";
                break;
            }
            ++m_idle_thread_count;
            // 执行完idle协程后返回当前点继续协程调度
            idle_fiber->resume();
            --m_idle_thread_count;
        }
    }

    LOG_DEBUG(logger) << "Scheduler::run() exit";
}

}  // namespace acid
