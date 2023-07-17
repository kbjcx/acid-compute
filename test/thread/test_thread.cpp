/*!
 *@file test_thread.cpp
 *@brief
 *@version 0.1
 *@date 2023-06-28
 */

#include "acid/common/thread.h"
#include "acid/logger/logger.h"

auto logger = GET_ROOT_LOGGER();

void run() {
    for (int i = 1; i < 1000; ++i) {
        LOG_INFO(logger) << i;
    }
}

int main() {
    logger->add_appender(acid::LogAppender::ptr(new acid::StdoutLogAppender()));

    auto t = std::make_shared<acid::Thread>(run, "thread1");

    t->join();

    return 0;
}