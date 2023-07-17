/*!
 *@file thread.cpp
 *@brief
 *@version 0.1
 *@date 2023-06-27
 */
#include "thread.h"

#include "acid/logger/logger.h"

namespace acid {

// 线程局部变量， 只在当前线程可用
// 当前线程指针
static thread_local Thread* t_thread = nullptr;
// 当前运行的线程名称
static thread_local std::string t_thread_name = "UNKNOWN";

static Logger::ptr logger = GET_LOGGER_BY_NAME("system");

Thread* Thread::get_this() {
    return t_thread;
}

const std::string& Thread::get_name() {
    return t_thread_name;
}

void Thread::set_name(const std::string& name) {
    if (name.empty()) {
        return;
    }
    if (t_thread) {
        t_thread->m_name = name;
    }
    t_thread_name = name;
}

 Thread::Thread(std::function<void()> cb, const std::string& name) : m_name(name), m_callback(cb), m_semaphore(0) {
    if (m_name.empty()) {
        m_name = "UNKNOWN";
    }
    int rt = pthread_create(&m_thread, nullptr, Thread::run, this);
    if (rt) {
        LOG_ERROR(logger) << "pthread_create thread fail, rt=" << rt << " name=" << m_name;
        throw std::logic_error("pthread create error");
    }

    // 等待直到线程函数开始执行
    m_semaphore.wait();
}

Thread::~Thread() {
    if (m_thread) {
        pthread_detach(m_thread);
    }
}

void Thread::join() {
    if (m_thread) {
        int rt = pthread_join(m_thread, nullptr);
        if (rt) {
            LOG_ERROR(logger) << "pthread_join thread fail, rt=" << rt << " name=" << m_name;
            throw std::logic_error("pthread_join error");
        }
    }
}

void* Thread::run(void* arg) {
    Thread* thread = (Thread*) arg;
    t_thread = thread;
    t_thread_name = thread->m_name;
    thread->m_id = get_thread_id();
    pthread_setname_np(pthread_self(), thread->m_name.substr(0, 15).c_str());

    std::function<void()> cb;
    cb.swap(thread->m_callback);

    thread->m_semaphore.notify();
    cb();
    return nullptr;
}

}  // namespace acid
