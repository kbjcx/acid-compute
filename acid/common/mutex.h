/*!
 * @file m_mutex.h
 * @brief 信号量, 互斥锁, 读写锁, 范围锁, 自旋锁, 原子锁
 * @version 0.1
 * @date 2023-06-23
 */

#ifndef DF_MUTEX_H
#define DF_MUTEX_H

#include "noncopyable.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <pthread.h>
#include <semaphore>

namespace acid {

class Semaphore : public Noncopyable {
public:
    explicit Semaphore(uint32_t count);

    ~Semaphore();

    void wait();

    void notify();

private:
    sem_t m_semaphore;
};  // class Semaphore : public Noncopyable

/*!
 * @brief 局部锁的模板
 */
template <class T>
class ScopedLockImpl {
public:
    explicit ScopedLockImpl(T &mutex) : m_mutex(mutex), m_is_locked(false) {
        lock();
    }

    ~ScopedLockImpl() {
        unlock();
    }

    void lock() {
        if (!m_is_locked) {
            m_mutex.lock();
            m_is_locked = true;
        }
    }

    bool try_lock() {
        if (!m_is_locked) {
            m_is_locked = m_mutex.try_lock();
        }
        return m_is_locked;
    }

    void unlock() {
        if (m_is_locked) {
            m_mutex.unlock();
            m_is_locked = false;
        }
    }

private:
    T &m_mutex;
    bool m_is_locked;

};  // class ScopedLockImpl

/*!
 * @brief 读局部锁模板
 */
template <class T>
class ReadScopedLockImpl {
public:
    explicit ReadScopedLockImpl(T &mutex) : m_mutex(mutex), m_is_locked(false) {
        lock();
    }

    ~ReadScopedLockImpl() {
        unlock();
    }

    void lock() {
        if (!m_is_locked) {
            m_mutex.rdlock();
            m_is_locked = true;
        }
    }

    void unlock() {
        if (m_is_locked) {
            m_mutex.unlock();
            m_is_locked = false;
        }
    }

private:
    T &m_mutex;
    bool m_is_locked;

};  // class ReadScopedLockImpl


/*!
 * @brief 写局部锁模板
 */
template <class T>
class WriteScopedLockImpl {
public:
    explicit WriteScopedLockImpl(T &mutex) : m_mutex(mutex), m_is_locked(false) {
        lock();
    }

    ~WriteScopedLockImpl() {
        unlock();
    }

    void lock() {
        if (!m_is_locked) {
            m_mutex.wrlock();
            m_is_locked = true;
        }
    }

    void unlock() {
        if (m_is_locked) {
            m_mutex.unlock();
            m_is_locked = false;
        }
    }

private:
    T &m_mutex;
    bool m_is_locked;

};  // class WriteScopedLockImpl

/*!
 * @brief 空互斥量(用于调试)
 */
class NullMutex : public Noncopyable {
public:
    using Lock = ScopedLockImpl<NullMutex>;

    NullMutex() = default;

    ~NullMutex() = default;

    void lock() {
    }

    bool try_lock() {
    }

    void unlock() {
    }

};  // class NullMutex : public Noncopyable


/*!
 * @brief 互斥量
 */
class Mutex : public Noncopyable {
public:
    // 局部锁
    using Lock = ScopedLockImpl<Mutex>;

    Mutex() {
        pthread_mutex_init(&m_mutex, nullptr);
    }

    ~Mutex() {
        pthread_mutex_destroy(&m_mutex);
    }

    void lock() {
        pthread_mutex_lock(&m_mutex);
    }

    bool try_lock() {
        return pthread_mutex_trylock(&m_mutex) == 0;
    }

    void unlock() {
        pthread_mutex_unlock(&m_mutex);
    }

    pthread_mutex_t *get() {
        return &m_mutex;
    }

private:
    pthread_mutex_t m_mutex;

};  // class Mutex : public Noncopyable

class NullRWMutex : public Noncopyable {
public:
    using ReadLock = ReadScopedLockImpl<NullRWMutex>;
    using WriteLock = WriteScopedLockImpl<NullRWMutex>;

    NullRWMutex() = default;

    ~NullRWMutex() = default;

    void rdlock() {
    }

    void wrlock() {
    }

    void unlock() {
    }
};  // class NullRWMutex : public Noncopyable

class RWMutex : public Noncopyable {
public:
    using ReadLock = ReadScopedLockImpl<RWMutex>;
    using WriteLock = WriteScopedLockImpl<RWMutex>;

    RWMutex() {
        pthread_rwlock_init(&m_mutex, nullptr);
    }

    ~RWMutex() {
        pthread_rwlock_destroy(&m_mutex);
    }

    void rdlock() {
        pthread_rwlock_rdlock(&m_mutex);
    }

    void wrlock() {
        pthread_rwlock_wrlock(&m_mutex);
    }

    void unlock() {
        pthread_rwlock_unlock(&m_mutex);
    }

private:
    pthread_rwlock_t m_mutex;

};  // class RWMutex : public Noncopyable

/*!
 * @brief 自旋锁
 */
class Spinlock : public Noncopyable {
public:
    using Lock = ScopedLockImpl<Spinlock>;

    Spinlock() {
        pthread_spin_init(&m_mutex, 0);
    }

    ~Spinlock() {
        pthread_spin_destroy(&m_mutex);
    }

    void lock() {
        pthread_spin_lock(&m_mutex);
    }

    bool try_lock() {
        return pthread_spin_trylock(&m_mutex) == 0;
    }

    void unlock() {
        pthread_spin_unlock(&m_mutex);
    }

private:
    pthread_spinlock_t m_mutex;

};  // class Spinlock : public Noncopyable

class CASLock : public Noncopyable {
public:
    using Lock = ScopedLockImpl<CASLock>;

    CASLock() {
        m_mutex.clear();
    }

    ~CASLock() = default;

    void lock() {
        while (std::atomic_flag_test_and_set_explicit(&m_mutex, std::memory_order_acquire)) {
        }
    }

    void unlock() {
        std::atomic_flag_clear_explicit(&m_mutex, std::memory_order_release);
    }

private:
    volatile std::atomic_flag m_mutex;

};  // class CASLock : public Noncopyable

// TODO 针对读写锁的条件变量
class Cond {
public:
    Cond() {
        pthread_cond_init(&m_cond, nullptr);
    }
    ~Cond() {
        pthread_cond_destroy(&m_cond);
    }

    void wait(Mutex *mutex) {
        pthread_cond_wait(&m_cond, mutex->get());
    }

    bool wait_timeout(Mutex *mutex, int ms) {
        timespec abs_time {};
        timespec now {};

        clock_gettime(CLOCK_REALTIME, &now);
        abs_time.tv_sec = now.tv_sec + ms / 1000;
        abs_time.tv_nsec = now.tv_nsec + ms % 1000 * 1000 * 1000;
        if (pthread_cond_timedwait(&m_cond, mutex->get(), &abs_time) == 0) {
            return true;
        }
        return false;
    }

    void signal() {
        pthread_cond_signal(&m_cond);
    }

    void broadcast() {
        pthread_cond_broadcast(&m_cond);
    }

private:
    pthread_cond_t m_cond;
};

}  // namespace acid

#endif  // DF_MUTEX_H
