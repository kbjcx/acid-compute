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
    sem_t semaphore_;
};  // class Semaphore : public Noncopyable

/*!
 * @brief 局部锁的模板
 */
template <class T>
class ScopedLockImpl {
public:
    explicit ScopedLockImpl(T &mutex) : mutex_(mutex), is_locked_(false) {
        lock();
    }

    ~ScopedLockImpl() {
        unlock();
    }

    void lock() {
        if (!is_locked_) {
            mutex_.lock();
            is_locked_ = true;
        }
    }

    void unlock() {
        if (is_locked_) {
            mutex_.unlock();
            is_locked_ = false;
        }
    }

private:
    T &mutex_;
    bool is_locked_;

};  // class ScopedLockImpl

/*!
 * @brief 读局部锁模板
 */
template <class T>
class ReadScopedLockImpl {
public:
    explicit ReadScopedLockImpl(T &mutex) : mutex_(mutex), is_locked_(false) {
        lock();
    }

    ~ReadScopedLockImpl() {
        unlock();
    }

    void lock() {
        if (!is_locked_) {
            mutex_.rdlock();
            is_locked_ = true;
        }
    }

    void unlock() {
        if (is_locked_) {
            mutex_.unlock();
            is_locked_ = false;
        }
    }

private:
    T &mutex_;
    bool is_locked_;

};  // class ReadScopedLockImpl


/*!
 * @brief 写局部锁模板
 */
template <class T>
class WriteScopedLockImpl {
public:
    explicit WriteScopedLockImpl(T &mutex) : mutex_(mutex), is_locked_(false) {
        lock();
    }

    ~WriteScopedLockImpl() {
        unlock();
    }

    void lock() {
        if (!is_locked_) {
            mutex_.wrlock();
            is_locked_ = true;
        }
    }

    void unlock() {
        if (is_locked_) {
            mutex_.unlock();
            is_locked_ = false;
        }
    }

private:
    T &mutex_;
    bool is_locked_;

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
        pthread_mutex_init(&mutex_, nullptr);
    }

    ~Mutex() {
        pthread_mutex_destroy(&mutex_);
    }

    void lock() {
        pthread_mutex_lock(&mutex_);
    }

    void unlock() {
        pthread_mutex_unlock(&mutex_);
    }

    pthread_mutex_t *get() {
        return &mutex_;
    }

private:
    pthread_mutex_t mutex_;

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
        pthread_rwlock_init(&mutex_, nullptr);
    }

    ~RWMutex() {
        pthread_rwlock_destroy(&mutex_);
    }

    void rdlock() {
        pthread_rwlock_rdlock(&mutex_);
    }

    void wrlock() {
        pthread_rwlock_wrlock(&mutex_);
    }

    void unlock() {
        pthread_rwlock_unlock(&mutex_);
    }

private:
    pthread_rwlock_t mutex_;

};  // class RWMutex : public Noncopyable

/*!
 * @brief 自旋锁
 */
class Spinlock : public Noncopyable {
public:
    using Lock = ScopedLockImpl<Spinlock>;

    Spinlock() {
        pthread_spin_init(&mutex_, 0);
    }

    ~Spinlock() {
        pthread_spin_destroy(&mutex_);
    }

    void lock() {
        pthread_spin_lock(&mutex_);
    }

    void unlock() {
        pthread_spin_unlock(&mutex_);
    }

private:
    pthread_spinlock_t mutex_;

};  // class Spinlock : public Noncopyable

class CASLock : public Noncopyable {
public:
    using Lock = ScopedLockImpl<CASLock>;

    CASLock() {
        mutex_.clear();
    }

    ~CASLock() = default;

    void lock() {
        while (std::atomic_flag_test_and_set_explicit(&mutex_, std::memory_order_acquire))
            ;
    }

    void unlock() {
        std::atomic_flag_clear_explicit(&mutex_, std::memory_order_release);
    }

private:
    volatile std::atomic_flag mutex_;

};  // class CASLock : public Noncopyable

// TODO 针对读写锁的条件变量
class Cond {
public:
    Cond() {
        pthread_cond_init(&cond_, nullptr);
    }
    ~Cond() {
        pthread_cond_destroy(&cond_);
    }

    void wait(Mutex *mutex) {
        pthread_cond_wait(&cond_, mutex->get());
    }

    bool wait_timeout(Mutex *mutex, int ms) {
        timespec abs_time {};
        timespec now {};

        clock_gettime(CLOCK_REALTIME, &now);
        abs_time.tv_sec = now.tv_sec + ms / 1000;
        abs_time.tv_nsec = now.tv_nsec + ms % 1000 * 1000 * 1000;
        if (pthread_cond_timedwait(&cond_, mutex->get(), &abs_time) == 0) {
            return true;
        }
        return false;
    }

    void signal() {
        pthread_cond_signal(&cond_);
    }

    void broadcast() {
        pthread_cond_broadcast(&cond_);
    }

private:
    pthread_cond_t cond_;
};

}  // namespace acid

#endif  // DF_MUTEX_H
