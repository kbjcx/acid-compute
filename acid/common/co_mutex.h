/**
 * @file co_mutex.h
 * @author kbjcx (lulu5v@163.com)
 * @brief 协程同步
 * @version 0.1
 * @date 2023-07-18
 *
 * @copyright Copyright (c) 2023
 *
 */

#ifndef ACID_CO_MUTEX_H
#define ACID_CO_MUTEX_H

#include "mutex.h"
#include "noncopyable.h"

#include <queue>

namespace acid {

class Fiber;
class CoMutex : Noncopyable {
public:
    using Lock = ScopedLockImpl<CoMutex>;
    using LockGuard = Spinlock::Lock;

    void lock();

    bool try_lock();

    void unlock();

private:
    Spinlock m_mutex;                                 // 协程所持有的锁
    Spinlock m_guard;                                 // 保护等待队列的锁
    uint64_t m_fiber_id;                              // 持有锁的协程ID
    std::queue<std::shared_ptr<Fiber>> m_wait_queue;  // 协程等待队列
};

class Timer;
class CoCond : Noncopyable {
public:
    using MutexType = Spinlock;
    using LockGuard = MutexType::Lock;

    void notify();

    void notify_all();

    void wait();

    void wait(CoMutex::Lock& lock);

private:
    std::queue<std::shared_ptr<Fiber>> m_wait_queue;  // 协程等待队列
    MutexType m_mutex;                                // 保护协程等待队列
    std::shared_ptr<Timer> m_timer;  // 空任务定时器, 让调度器保持调度
};

class CoSemaphore : Noncopyable {
public:
    CoSemaphore(uint32_t num);

    void wait();

    void notify();

private:
    uint32_t m_num;   // 信号量的数量
    uint32_t m_used;  // 已经获取的信号量的数量
    CoCond m_cond;    // 协程条件变量
    CoMutex m_mutex;  // 协程锁
};

class CoCountDownLatch {
public:
    CoCountDownLatch(int count);

    /**
     * @brief 使当前协程挂起等待, 直到count值被减到0, 当前协程就会被唤醒
     *
     */
    void wait();

    /**
     * @brief 使latch的值-1, 如果减到了0, 则会唤醒所有等待在这个latch上的协程
     *
     * @return true
     * @return false
     */
    bool count_down();

    int get_count();

private:
    int m_count;
    CoMutex m_mutex;
    CoCond m_cond;
};

}  // namespace acid

#endif