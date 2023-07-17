#include "acid/logger/logger.h"

int main() {
    auto root_logger = GET_ROOT_LOGGER();
    root_logger->add_appender(acid::LogAppender::ptr(new acid::FileLogAppender("./test_log.log", acid::LogLevel::DEBUG)));
    root_logger->add_appender(acid::LogAppender::ptr(new acid::StdoutLogAppender()));
    LOG_DEBUG(root_logger) << "debug";
    LOG_INFO(root_logger) << "info";
    LOG_NOTICE(root_logger) << "notice";
    LOG_WARN(root_logger) << "warning";
    LOG_ERROR(root_logger) << "error";
    LOG_CRIT(root_logger) << "critical";
    LOG_ALERT(root_logger) << "alert";
    LOG_FATAL(root_logger) << "fatal";

    auto logger = GET_LOGGER_BY_NAME("test");
    logger->add_appender(acid::FileLogAppender::create("./test_log2.log", acid::LogLevel::ERROR));
    logger->add_appender(acid::StdoutLogAppender::create(acid::LogLevel::NOTICE));
    LOG_FMT_DEBUG(logger, "%s", "debug");
    LOG_FMT_INFO(logger, "%s", "info");
    LOG_FMT_WARN(logger, "%s", "warning");
    LOG_FMT_NOTICE(logger, "%s", "notice");
    LOG_FMT_ERROR(logger, "%s", "error");
    LOG_FMT_CRIT(logger, "%s", "critical");
    LOG_FMT_ALERT(logger, "%s", "alert");
    LOG_FMT_FATAL(logger, "%s", "fatal");

    
    return 0;
}