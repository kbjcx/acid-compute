/*!
 *@file fiber.h
 *@brief 实现简单的协程模块
 *@details 基于ucontext_t实现非对称协程
 *@version 0.1
 *@date 2023-06-27
 */
#ifndef DF_FIBER_H
#define DF_FIBER_H

#include <functional>
#include <memory>
#include <ucontext.h>

namespace acid {
class Fiber : public std::enable_shared_from_this<Fiber> {
public:
    using ptr = std::shared_ptr<Fiber>;

    /*!
     * @brief 协程状态
     * @details 对协程状态进行了简化
     */
    enum class State {
        READY,    // 就绪态
        RUNNING,  // 运行态
        TERM      // 结束态, 协程的回调函数执行完之后的状态
    };

private:
    /*!
     * @brief 构造函数
     * @details创建线程的第一个协程, 也就是主协程, 这个线程只能通过get_this来调用
     */
    Fiber();

public:
    /*!
     * @brief 构造函数, 用于创建用户协程
     * @param[in] callback 协程入口函数
     * @param[in] stack_size 栈大小
     * @param[in] run_in_scheduler 是否参与协程调度器调度
     */
    explicit Fiber(std::function<void(void)> callback, size_t stack_size = 0, bool run_in_scheduler = true);

    ~Fiber();

    /*!
     * @brief 重置协程状态和入口函数, 复用栈空间, 不用重新创建栈
     */
    void reset(std::function<void(void)>);

    /*!
     * @brief 将当前协程切换到运行状态
     * @details 将当前协程雨正在运行的协程进行交换, 后者变为就绪态
     */
    void resume();

    /*!
     * @brief 当前协程让出执行权
     * @details 当前协程与上一次resume时退到后台的协程进行交换
     */
    void yield();

    uint64_t get_id() const {
        return m_id;
    }

    State get_state() const {
        return m_state;
    }

public:
    /**
     * @brief 设置当前正在运行的协程，即设置线程局部变量t_fiber的值
     */
    static void set_this(Fiber* f);

    /**
     * @brief 返回当前线程正在执行的协程
     * @details 如果当前线程还未创建协程，则创建线程的第一个协程，
     * 且该协程为当前线程的主协程，其他协程都通过这个协程来调度，也就是说，其他协程
     * 结束时,都要切回到主协程，由主协程重新选择新的协程进行resume
     * @attention 线程如果要创建协程，那么应该首先执行一下Fiber::GetThis()操作，以初始化主函数协程
     */
    static Fiber::ptr get_this();

    /*!
     * @brief 获取总协程数
     */
    static uint64_t total_fibers();

    /*!
     * @brief 协程入口函数
     */
    static void main_func();

    /*!
     * @brief 获取当前协程ID
     */
    static uint64_t get_fiber_id();

private:
    uint64_t m_id; // 协程id
    uint32_t m_stack_size; // 协程栈大小
    State m_state; // 协程状态
    ucontext_t m_ctx; // 协程上下文
    void* m_stack = nullptr; // 协程栈地址
    std::function<void(void)> m_callback; // 协程入口函数
    bool m_run_in_scheduler; // 本协程是否参与调度器

};  // class Fiber : public std::enable_shared_from_this<Fiber>

}  // namespace acid

#endif  // DF_FIBER_H
