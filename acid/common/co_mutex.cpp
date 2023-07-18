#include "co_mutex.h"

#include "fiber.h"
#include "iomanager.h"

namespace acid {

void CoMutex::lock() {
    // 如果本协程持有锁就退出
    if (Fiber::get_fiber_id() == m_fiber_id) {
        return;
    }

    // 第一次尝试获取锁
    while (!try_lock()) {
        Fiber::ptr self;
        {
            LockGuard lock(m_guard);
            // 由于进入等待队列和出队的代价比较大，所以再次尝试获取锁，成功获取锁将m_fiberId改成自己的id
            if (try_lock()) {
                m_fiber_id = Fiber::get_fiber_id();
                return;
            }
            // 获取所在的协程
            self = Fiber::get_this();
            // 将自己加入协程等待队列
            m_wait_queue.push(self);
        }
        self->yield();
    }
    // 成功获取锁后将m_fiber_id改成自己的id
    m_fiber_id = Fiber::get_fiber_id();
}

bool CoMutex::try_lock() {
    return m_mutex.try_lock();
}

void CoMutex::unlock() {
    Fiber::ptr fiber;
    {
        LockGuard lock(m_guard);
        m_fiber_id = 0;
        if (!m_wait_queue.empty()) {
            // 获取一个等待的协程
            fiber = m_wait_queue.front();
            m_wait_queue.pop();
        }
    }
    m_mutex.unlock();
    if (fiber) {
        IOManager::get_this()->schedule(fiber);
    }
}

void CoCond::notify() {
    Fiber::ptr fiber;
    {
        // 获取一个等待的协程
        LockGuard lock(m_mutex);
        if (m_wait_queue.empty()) {
            return;
        }

        fiber = m_wait_queue.front();
        m_wait_queue.pop();
        if (m_timer) {
            m_timer->cancel();
            m_timer.reset();
        }
    }
    // 将协程重新加入调度
    if (fiber) {
        IOManager::get_this()->schedule(fiber);
    }
}

void CoCond::notify_all() {
    LockGuard lock(m_mutex);
    // 将全部等待的协程全部加入调度
    while (!m_wait_queue.empty()) {
        Fiber::ptr fiber = m_wait_queue.front();
        m_wait_queue.pop();
        if (fiber) {
            IOManager::get_this()->schedule(fiber);
        }
    }
    // 删除定时器
    if (m_timer) {
        m_timer->cancel();
        m_timer.reset();
    }
}

void CoCond::wait() {
    Fiber::ptr self = Fiber::get_this();
    {
        LockGuard lock(m_mutex);
        // 将自己加入等待队列
        m_wait_queue.push(self);
        if (!m_timer) {
            // 加入一个空任务定时器, 不让调度器退出
            m_timer = IOManager::get_this()->add_timer(
                -1, [] {}, true);
        }
    }
    // 让出协程
    self->yield();
}

void CoCond::wait(CoMutex::Lock& lock) {
    Fiber::ptr self = Fiber::get_this();
    {
        LockGuard lock1(m_mutex);
        // 将自己加入等待队列
        m_wait_queue.push(self);
        if (!m_timer) {
            m_timer = IOManager::get_this()->add_timer(
                -1, []() {}, true);
        }
        lock.unlock();
    }

    self->yield();
    lock.lock();
}

CoSemaphore::CoSemaphore(uint32_t num) : m_num(num), m_used(0) {
}

void CoSemaphore::wait() {
    CoMutex::Lock lock(m_mutex);
    // 如果已经获取的信号量大于等于信号量数量则让出协程等待
    while (m_used >= m_num) {
        m_cond.wait(lock);
    }
    ++m_used;
}

void CoSemaphore::notify() {
    CoMutex::Lock lock(m_mutex);
    if (m_used > 0) {
        --m_used;
    }
    // 通知一个等待的协程
    m_cond.notify();
}

CoCountDownLatch::CoCountDownLatch(int count) : m_count(count) {
}

void CoCountDownLatch::wait() {
    CoMutex::Lock lock(m_mutex);
    if (!m_count) {
        return;
    }
    m_cond.wait(lock);
}

bool CoCountDownLatch::count_down() {
    CoMutex::Lock lock(m_mutex);
    if (!m_count) {
        return false;
    }

    --m_count;
    if (!m_count) {
        m_cond.notify_all();
    }
    return true;
}

int CoCountDownLatch::get_count() {
    CoMutex::Lock lock(m_mutex);
    return m_count;
}

};  // namespace acid