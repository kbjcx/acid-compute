/**
 * @file channel.h
 * @author kbjcx (lulu5v@163.com)
 * @brief
 * @version 0.1
 * @date 2023-07-20
 *
 * @copyright Copyright (c) 2023
 *
 */

#ifndef ACID_CHANNEL_H
#define ACID_CHANNEL_H

#include "co_mutex.h"

#include <queue>

namespace acid {

template <class T>
class ChannelImpl : Noncopyable {
public:
    using MutexType = CoMutex;
    using LockGuard = MutexType::Lock;

    ChannelImpl(size_t capacity) : m_is_close(false), m_capacity(capacity) {
    }

    ~ChannelImpl() {
        close();
    }

    /**
     * @brief 发送数据到channel
     *
     * @param[in] t 发送的数据
     * @return true
     * @return false
     */
    bool push(const T& t) {
        LockGuard lock(m_mutex);
        if (m_is_close) {
            return false;
        }

        // 如果缓冲区满了, 需要等待入队条件变量唤醒
        while (m_queue.size() >= m_capacity) {
            m_push_cond.wait(lock);
            if (m_is_close) {
                return false;
            }
        }

        m_queue.push(t);
        // 队列中有数据了, 可以唤醒出队条件变量
        m_pop_cond.notify();
        return true;
    }

    /**
     * @brief 从channel读取数据
     *
     * @param[out] t 读取到t
     * @return true
     * @return false
     */
    bool pop(T& t) {
        LockGuard lock(m_mutex);
        if (m_is_close) {
            return false;
        }

        while (m_queue.empty()) {
            m_pop_cond.wait(lock);
            if (m_is_close) {
                return false;
            }
        }

        t = m_queue.front();
        m_queue.pop();
        // 缓冲区空了位置, 通知入队条件变量
        m_push_cond.notify();
        return true;
    }

    ChannelImpl& operator>>(T& t) {
        pop(t);
        return *this;
    }

    ChannelImpl& operator<<(const T& t) {
        push(t);
        return *this;
    }

    void close() {
        LockGuard lock(m_mutex);
        if (m_is_close) {
            return;
        }
        m_is_close = true;
        // 唤醒等待的协程
        m_push_cond.notify_all();
        m_pop_cond.notify_all();
        std::queue<T> q;
        q.swap(m_queue);
    }

    operator bool() const {
        return m_is_close == false;
    }

    size_t capacity() const {
        return m_capacity;
    }

    size_t size() {
        LockGuard lock(m_mutex);
        return m_queue.size();
    }

    bool empty() {
        LockGuard lock(m_mutex);
        return m_queue.empty();
    }

private:
    bool m_is_close;
    size_t m_capacity;      // channel缓冲区大小
    MutexType m_mutex;      // 协程锁, 配合协程条件变量
    CoCond m_push_cond;     // 入队条件变量
    CoCond m_pop_cond;      // 出队条件变量
    std::queue<T> m_queue;  // 消息队列
};

/**
 * @brief Channel主要是用于协程之间的通信，属于更高级层次的抽象
 * 在类的实现上采用了 PIMPL 设计模式，将具体操作转发给实现类
 * Channel 对象可随意复制，通过智能指针指向同一个 ChannelImpl
 */
template <class T>
class Channel {
public:
    Channel(size_t capacity) {
        m_channel = std::make_shared<ChannelImpl<T> >(capacity);
    }

    Channel(const Channel& channel) {
        m_channel = channel.m_channel;
    }

    void close() {
        m_channel->close();
    }

    operator bool() const {
        return *m_channel;
    }

    bool push(const T& t) {
        return m_channel->push(t);
    }

    bool pop(T& t) {
        return m_channel->pop(t);
    }

    Channel& operator>>(T& t) {
        pop(t);
        return *this;
    }

    Channel& operator<<(const T& t) {
        push(t);
        return *this;
    }

    size_t capacity() const {
        return m_channel->capacity();
    }

    size_t size() {
        return m_channel->size();
    }

    bool empty() {
        return m_channel->empty();
    }

    bool unique() const {
        return m_channel.unique();
    }

private:
    std::shared_ptr<ChannelImpl<T> > m_channel;
};

}  // namespace acid

#endif