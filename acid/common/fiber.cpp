/*!
 *@file fiber.cpp
 *@brief 协程实现
 *@version 0.1
 *@date 2023-06-27
 */
#include "fiber.h"

#include "acid/logger/logger.h"
#include "config.h"
#include "scheduler.h"
#include "util.h"

#include <cassert>

namespace acid {

static Logger::ptr logger = GET_LOGGER_BY_NAME("system");
static auto root_logger = GET_ROOT_LOGGER();

// 全局静态变量, 用于生成协程ID
static std::atomic<uint64_t> s_fiber_id = 0;
// 统计当前协程数
static std::atomic<uint64_t> s_fiber_count = 0;

// 线程局部变量, 用于保存当前正在运行的协程
static thread_local Fiber* t_fiber = nullptr;

// 线程局部变量, 当前线程的主协程
static thread_local Fiber::ptr t_main_fiber = nullptr;

// 协程栈大小, 默认128K, TODO 可以通过config文件设置
// static uint32_t get_fiber_stack_size() {
//
//    return 128 * 1024;
//}
// 从config类中加载stack_size参数
static ConfigVar<uint32_t>::ptr g_fiber_stack_size =
    Config::look_up<uint32_t>("fiber.stack_size", 128 * 1024, "fiber stack size");

// 包装内存申请和释放
class MallocStackAllocator {
public:
    static void* alloc(size_t size) {
        return malloc(size);
    }

    static void dealloc(void* p) {
        return free(p);
    }
};  // class MallocStackAllocator

using StackAllocator = MallocStackAllocator;

uint64_t Fiber::get_fiber_id() {
    if (t_fiber) {
        return t_fiber->get_id();
    }
    return 0;
}

Fiber::Fiber() : m_stack_size(0), m_ctx(), m_run_in_scheduler(false) {
    LOG_DEBUG(root_logger) << "Fiber::Fiber()";
    set_this(this);
    m_state = State::RUNNING;

    // 将当前上下文保存到m_ctx，但是并不会从这里恢复上下文，因为肯定会调用swapcontext来切换到子协程
    if (getcontext(&m_ctx)) {
        // TODO
    }

    ++s_fiber_count;
    m_id = ++s_fiber_id;
}

Fiber::Fiber(std::function<void()> callback, size_t stack_size, bool run_in_scheduler)
    : m_id(++s_fiber_id)
    , m_state(State::READY)
    , m_callback(callback)
    , m_run_in_scheduler(run_in_scheduler) {
    LOG_DEBUG(root_logger)
        << "Fiber::Fiber(std::function<void()> callback, size_t stack_size, bool run_in_scheduler)";
    ++s_fiber_count;
    m_stack_size = stack_size ? stack_size : g_fiber_stack_size->get_value();
    m_stack = StackAllocator::alloc(m_stack_size);

    if (getcontext(&m_ctx)) {
        // TODO
    }

    m_ctx.uc_link = nullptr;
    m_ctx.uc_stack.ss_sp = m_stack;
    m_ctx.uc_stack.ss_size = m_stack_size;

    // 将保存的上下文与协程入口函数绑定，恢复上下文时会执行协程入口函数
    // 当恢复上下文时回去执行绑定的函数
    makecontext(&m_ctx, &Fiber::main_func, 0);

    LOG_DEBUG(logger) << "Fiber::Fiber() id = " << m_id;
}

// 获取当前正在执行的协程，若还没有创建主协程，则创建主协程
Fiber::ptr Fiber::get_this() {
    //    printf("get this \n");
    if (t_fiber) {
        return t_fiber->shared_from_this();
    }
    Fiber::ptr main_fiber(new Fiber);
    // printf("create main fiber \n");
    // TODO SYLAR_ASSERT(t_fiber == main_fiber.get());
    t_main_fiber = main_fiber;
    return t_fiber->shared_from_this();
}

// 设置当前线程正在执行的主协程
void Fiber::set_this(acid::Fiber* f) {
    t_fiber = f;
}

// 析构时要先判断是主协程还是子协程
// 子协程要处于TERM状态才能被关闭
// 主协程处于运行状态才能被关闭
Fiber::~Fiber() {
    LOG_DEBUG(logger) << "Fiber::~Fiber() id = " << m_id;
    --s_fiber_count;
    if (m_stack) {
        // 有栈说明是子协程, 需要确保处于TERM状态
        assert(m_state == State::TERM);
        StackAllocator::dealloc(m_stack);
        LOG_DEBUG(logger) << "dealloc stack id = " << m_id;
    }
    else {
        // 说明是主协程
        assert(!m_callback);
        assert(m_state == State::RUNNING);

        Fiber* cur = t_fiber;
        if (cur == this) {
            set_this(nullptr);
        }
    }
}

/*!
 * @details
 * 这里为了简化状态管理，强制只有TERM状态的协程才可以重置，但其实刚创建好但没执行过的协程也应该允许重置的
 * @attention 只有子协程才能被重置
 */
void Fiber::reset(std::function<void()> callback) {
    assert(m_stack);
    assert(m_state == State::TERM);
    m_callback = callback;
    if (getcontext(&m_ctx)) {
        //        assert(false, "getcontext");
    }
    m_ctx.uc_link = nullptr;
    m_ctx.uc_stack.ss_sp = m_stack;
    m_ctx.uc_stack.ss_size = m_stack_size;

    makecontext(&m_ctx, &Fiber::main_func, 0);
    m_state = State::READY;
}

// resume只有在主协程或者调度协程中才会执行，用于切到子协程并保存主协程或调度协程的当前上下文
void Fiber::resume() {
    assert(m_state != State::TERM && m_state != State::RUNNING);
    set_this(this);
    m_state = State::RUNNING;
    // 如果协程参与调度器调度，那么应该和调度器的主协程进行swap，而不是线程主协程
    if (m_run_in_scheduler) {
        // swapcontext将上下文保存在第一个参数中，然后切换到第二个参数指定的上下文
        // 等待从别的地方再切回当前上下文后从保存的位置开始继续执行
        if (swapcontext(&(Scheduler::get_main_fiber()->m_ctx), &m_ctx)) {
            // TODO swap_context
        }
    }
    else {
        if (swapcontext(&(t_main_fiber->m_ctx), &m_ctx)) {
            // TODO assert
        }
    }
}

// 会在子协程中调用，用于切换到主协程或是调度协程
void Fiber::yield() {
    /// 协程运行完之后会自动yield一次，用于回到主协程，此时状态已为结束状态
    assert(m_state == State::RUNNING || m_state == State::TERM);
    set_this(t_main_fiber.get());
    if (m_state != State::TERM) {
        m_state = State::READY;
    }

    // 如果协程参与调度器调度，那么应该和调度器的主协程进行swap，而不是线程主协程
    if (m_run_in_scheduler) {
        if (swapcontext(&m_ctx, &(Scheduler::get_main_fiber()->m_ctx))) {
            // TODO swap_context
        }
    }
    else {
        if (swapcontext(&m_ctx, &(t_main_fiber->m_ctx))) {
            // TODO assert
        }
    }
}

/**
 * @details
 * 这里没有处理协程函数出现异常的情况，同样是为了简化状态管理，并且个人认为协程的异常不应该由框架处理，应该由开发者自行处理
 */
void Fiber::main_func() {
    Fiber::ptr cur = get_this();  // 引用计数 + 1
    assert(cur);
    cur->m_callback();
    cur->m_callback = nullptr;
    cur->m_state = State::TERM;

    auto raw_ptr = cur.get();
    cur.reset();  // 引用计数 - 1
    // 上面的状态以被标记为TERM，因此yield之后不会继续调度该协程
    raw_ptr->yield();
}

}  // namespace acid