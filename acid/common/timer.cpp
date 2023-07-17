#include "timer.h"

#include "acid/common/util.h"

#include <cstdint>
#include <functional>
#include <memory>

namespace acid {

bool Timer::Comparator::operator()(const Timer::ptr& lhs, const Timer::ptr& rhs) const {
    if (!lhs && !rhs) {
        return false;
    }
    if (!lhs) {
        return true;
    }
    if (!rhs) {
        return false;
    }
    if (lhs->m_next < rhs->m_next) {
        return true;
    }
    if (lhs->m_next > rhs->m_next) {
        return false;
    }
    return lhs.get() < rhs.get();
}

Timer::Timer(uint64_t ms, std::function<void()> cb, bool recurring, TimerManager* manager)
    : m_recurring(recurring), m_ms(ms), m_next(get_elapsed_ms() + ms), m_callback(cb), m_manager(manager) {
}

Timer::Timer(uint64_t next) : m_recurring(false), m_ms(0), m_next(next), m_callback(nullptr), m_manager(nullptr) {
}

bool Timer::cancel() {
    TimerManager::WriteLockGuard lock(m_manager->m_mutex);
    if (m_callback) {
        m_callback = nullptr;
        auto it = m_manager->m_timers.find(shared_from_this());
        m_manager->m_timers.erase(it);
        return true;
    }
    return false;
}

bool Timer::refresh() {
    TimerManager::WriteLockGuard lock(m_manager->m_mutex);
    if (!m_callback) {
        return false;
    }
    auto it = m_manager->m_timers.find(shared_from_this());
    if (it == m_manager->m_timers.end()) {
        return false;
    }
    m_manager->m_timers.erase(it);
    m_next = get_elapsed_ms() + m_ms;
    m_manager->m_timers.insert(shared_from_this());
    return true;
}

bool Timer::reset(uint64_t ms, bool from_now) {
    if (ms == m_ms && !from_now) {
        return true;
    }
    TimerManager::WriteLockGuard lock(m_manager->m_mutex);
    if (!m_callback) {
        return false;
    }
    auto it = m_manager->m_timers.find(shared_from_this());
    if (it == m_manager->m_timers.end()) {
        return false;
    }
    m_manager->m_timers.erase(it);
    uint64_t start;
    if (from_now) {
        start = get_elapsed_ms();
    }
    else {
        start = m_next - m_ms;
    }
    m_ms = ms;
    m_next = start + m_ms;
    m_manager->add_timer(shared_from_this(), lock);
    return true;
}

TimerManager::TimerManager() : m_ticked(false), m_previous_time(get_elapsed_ms()) {
}

TimerManager::~TimerManager() {
}

Timer::ptr TimerManager::add_timer(uint64_t ms, std::function<void()> cb, bool recurring) {
    Timer::ptr timer(new Timer(ms, cb, recurring, this));
    WriteLockGuard lock(m_mutex);
    add_timer(timer, lock);
    return timer;
}

static void on_timer(std::weak_ptr<void> weak_cond, std::function<void()> cb) {
    std::shared_ptr<void> tmp = weak_cond.lock();
    if (tmp) {
        cb();
    }
}

Timer::ptr TimerManager::add_condition_timer(uint64_t ms, std::function<void()> cb, std::weak_ptr<void> weak_cond,
                                             bool recurring) {
    return add_timer(ms, std::bind(&on_timer, weak_cond, cb), recurring);
}

uint64_t TimerManager::get_next_timer() {
    ReadLockGuard lock(m_mutex);
    m_ticked = false;
    if (m_timers.empty()) {
        return ~0ull;
    }
    const Timer::ptr& next = *(m_timers.begin());
    uint64_t now_ms = get_elapsed_ms();
    if (now_ms >= next->m_next) {
        return 0;
    }
    else {
        return next->m_next - now_ms;
    }
}

void TimerManager::list_expired_callback(std::vector<std::function<void()>>& callbacks) {
    uint64_t now_ms = get_elapsed_ms();
    std::vector<Timer::ptr> expired;
    {
        ReadLockGuard lock(m_mutex);
        if (m_timers.empty()) {
            return;
        }
    }

    WriteLockGuard lock(m_mutex);
    if (m_timers.empty()) {
        return;
    }

    bool rollover = false;
    if (detect_clock_rollover(now_ms)) [[unlikely]] {
            rollover = true;
        }
    if (!rollover && ((*(m_timers.begin()))->m_next > now_ms)) {
        return;
    }

    Timer::ptr now_timer(new Timer(now_ms));
    auto it = rollover ? m_timers.end() : m_timers.lower_bound(now_timer);
    while (it != m_timers.end() && (*it)->m_next == now_ms) {
        ++it;
    }

    expired.insert(expired.begin(), m_timers.begin(), it);
    m_timers.erase(m_timers.begin(), it);
    callbacks.reserve(expired.size());

    for (auto& timer : expired) {
        callbacks.push_back(timer->m_callback);
        if (timer->m_recurring) {
            timer->m_next = now_ms + timer->m_ms;
            m_timers.insert(timer);
        }
        else {
            timer->m_callback = nullptr;
        }
    }
}

bool TimerManager::has_timer() {
    ReadLockGuard lock(m_mutex);
    return !m_timers.empty();
}

void TimerManager::add_timer(Timer::ptr val, WriteLockGuard& lock) {
    auto it = m_timers.insert(val).first;
    bool at_front = (it == m_timers.begin()) && !m_ticked;
    if (at_front) {
        m_ticked = true;
    }
    lock.unlock();
    if (at_front) {
        on_timer_inserted_at_front();
    }
}

bool TimerManager::detect_clock_rollover(uint64_t now_ms) {
    bool rollover = false;
    if (now_ms < m_previous_time && now_ms < (m_previous_time - 60 * 60 * 1000)) {
        rollover = true;
    }
    m_previous_time = now_ms;
    return rollover;
}


}  // namespace acid