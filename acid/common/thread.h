/*!
 *@file thread.h
 *@brief
 *@version 0.1
 *@date 2023-06-27
 */
#ifndef DF_THREAD_H
#define DF_THREAD_H

#include "mutex.h"
#include "noncopyable.h"

#include <functional>
#include <memory>
#include <pthread.h>

namespace acid {

class Thread : public Noncopyable {
public:
    using ptr = std::shared_ptr<Thread>;

    /*!
     * @brief 构造函数
     * param[in] cb 线程执行函数
     * param[in] name 线程名字
     */
    Thread(std::function<void(void)> cb, const std::string& name);
//    Thread(void(*cb)(), const std::string& name);
    ~Thread();

    pid_t get_id() const {
        return m_id;
    }

    const std::string& get_name_() const {
        return m_name;
    }

    void join();

    static Thread* get_this();

    static const std::string& get_name();

    static void set_name(const std::string& name);

private:
    static void* run(void* arg);

private:
    pid_t m_id;
    std::string m_name;
    pthread_t m_thread;
    std::function<void()> m_callback;
//    void (*m_callback)();
    Semaphore m_semaphore;
};

}  // namespace acid

#endif  // DF_THREAD_H
