/*!
 *@file test_fiber.cpp
 *@brief
 *@version 0.1
 *@date 2023-06-27
 */

#include "acid/common/fiber.h"
#include "acid/common/thread.h"
#include "acid/logger/logger.h"

#include <ctime>
#include <iostream>


auto logger = GET_ROOT_LOGGER();

void run_in_fiber2() {
    LOG_INFO(logger) << "run_in_fiber2 begin";
    LOG_INFO(logger) << "run_in_fiber2 end";
    LOG_INFO(logger) << "after run_in_fiber yield";
    LOG_INFO(logger) << "run_in_fiber end";
}

void run_in_fiber() {
    LOG_INFO(logger) << "run_in_fiber begin";
    LOG_INFO(logger) << "before run_in_fiber yield";
    acid::Fiber::get_this()->yield();
    LOG_INFO(logger) << "after run_in_fiber yield";
    LOG_INFO(logger) << "run_in_fiber end";
}

void test_fiber() {
    LOG_INFO(logger) << "test fiber begin";
    acid::Fiber::get_this();

    acid::Fiber::ptr fiber(new acid::Fiber(run_in_fiber, 0, false));
    //    LOG_INFO(logger) << "use_count: " << fiber.use_count();
    //    LOG_INFO(logger) << "before test_fiber resume";
    //    fiber->resume();
    //    LOG_INFO(logger) << "after test_fiber resume";
    //    LOG_INFO(logger) << "use_count: " << fiber.use_count();
    //    LOG_INFO(logger) << "fiber status: " << (int) fiber->get_state();
    //    LOG_INFO(logger) << "before test_fiber resume again";
    //    fiber->resume();
    //    LOG_INFO(logger) << "after test_fiber resume again";
    //
    //    LOG_INFO(logger) << "use_count: " << fiber.use_count();
    //    LOG_INFO(logger) << "fiber status: " << (int) fiber->get_state();
    //    fiber->reset(run_in_fiber2);
    //    fiber->resume();
    //    LOG_INFO(logger) << "use_count: " << fiber.use_count();
    fiber->resume();
    for (int i = 0; i < 100; ++i) {
        fiber->resume();
        fiber->reset(run_in_fiber);
        fiber->resume();
    }
    fiber->resume();

    // thread
    //    for (int i = 0; i < 100000; ++i) {
    //        run_in_fiber();
    //    }
    LOG_INFO(logger) << "test fiber end";
}

int main() {
    clock_t start, end;
    start = clock();

    logger->add_appender(
        acid::LogAppender::ptr(new acid::FileLogAppender("./test_log.log", acid::LogLevel::INFO)));
    logger->add_appender(acid::LogAppender::ptr(new acid::StdoutLogAppender()));
    LOG_INFO(logger) << "main begin";

    //    std::vector<acid::Thread::ptr> threads;
    //    for (int i = 0; i < 2; ++i) {
    //        threads.push_back(acid::Thread::ptr(new acid::Thread(test_fiber, "m_thread" +
    //        std::to_string(i))));
    //    }
    //
    //    for (auto i : threads) {
    //        i->join();
    //    }

    test_fiber();
    LOG_INFO(logger) << "main end";

    end = clock();

    std::cout << (end - start) << std::endl;
    return 0;
}