/**
 * @file timer.h
 * @author your name (you@domain.com)
 * @brief 定时器封装
 * @version 0.1
 * @date 2023-06-30
 *
 * @copyright Copyright (c) 2023
 *
 */

#ifndef DF_ACID_TIMER_H
#define DF_ACID_TIMER_H

#include "mutex.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <set>

namespace acid {

class TimerManager;

class Timer : public std::enable_shared_from_this<Timer> {
    friend class TimerManager;

public:
    using ptr = std::shared_ptr<Timer>;

    /**
     * @brief 取消定时器
     *
     * @return true
     * @return false
     */
    bool cancel();

    /**
     * @brief 刷新设置定时器的执行时间
     *
     * @return true
     * @return false
     */
    bool refresh();

    /**
     * @brief 重置定时器的时间
     *
     * @param ms 定时器执行间隔
     * @param from_now 是否从当前时间开始计算
     * @return true
     * @return false
     */
    bool reset(uint64_t ms, bool from_now);

private:
    /**
     * @brief Construct a new Timer object
     *
     * @param ms 定时器执行间隔
     * @param cb 回调函数
     * @param recurring 是否循环执行
     * @param manager 定时器管理器
     */
    Timer(uint64_t ms, std::function<void()> cb, bool recurring, TimerManager* manager);

    /**
     * @brief Construct a new Timer object
     *
     * @param next 执行的时间戳
     */
    Timer(uint64_t next);

private:
    bool m_recurring;
    uint64_t m_ms;
    uint64_t m_next;
    std::function<void()> m_callback;
    TimerManager* m_manager;

private:
    struct Comparator {
        bool operator()(const Timer::ptr& lhs, const Timer::ptr& rhs) const;
    };
};  // class Timer

class TimerManager {
    friend class Timer;

public:
    using RWMutexType = RWMutex;
    using ReadLockGuard = ReadScopedLockImpl<RWMutexType>;
    using WriteLockGuard = WriteScopedLockImpl<RWMutexType>;

    TimerManager();

    virtual ~TimerManager();

    /**
     * @brief 添加定时器
     *
     * @param ms 定时器执行间隔时间
     * @param cb 回调函数
     * @param recurring 是否循环定时器
     * @return Timer::ptr
     */
    Timer::ptr add_timer(uint64_t ms, std::function<void()> cb, bool recurring = false);

    Timer::ptr add_condition_timer(uint64_t ms, std::function<void()> cb, std::weak_ptr<void> weak_cond,
                                   bool recurring = false);

    /**
     * @brief 到最近的定时器执行的时间间隔
     *
     * @return uint64_t
     */
    uint64_t get_next_timer();

    /**
     * @brief 获取需要执行的定时器的回调函数列表
     *
     * @param[out] callbacks 回调函数数组
     */
    void list_expired_callback(std::vector<std::function<void()>>& callbacks);

    /**
     * @brief 是否有定时器
     *
     * @return true
     * @return false
     */
    bool has_timer();

protected:
    /**
     * @brief 当有新的定时器插入到定时器的首部, 执行该函数
     *
     */
    virtual void on_timer_inserted_at_front() = 0;

    /**
     * @brief 将定时器添加到管理类中
     *
     * @param val
     * @param lock
     */
    void add_timer(Timer::ptr val, WriteLockGuard& lock);

private:
    /**
     * @brief 检测服务器时间是否被调后了
     *
     * @param now_ms
     * @return true
     * @return false
     */
    bool detect_clock_rollover(uint64_t now_ms);

private:
    RWMutexType m_mutex;
    // 定时器集合
    std::set<Timer::ptr, Timer::Comparator> m_timers;
    // 是否触发了on_timer_inserted_at_front()
    bool m_ticked;
    // 上次执行时间
    uint64_t m_previous_time;
};  // class TimerManager

};  // namespace acid

#endif