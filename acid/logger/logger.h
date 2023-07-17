/*!
 * @brief
 */


#ifndef DF_ACID_LOGGER_LOGGER_H_
#define DF_ACID_LOGGER_LOGGER_H_

#include "acid/common/mutex.h"
#include "acid/common/singleton.h"
#include "acid/common/util.h"

#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

/*!
 *@brief 获取默认的root日志器
 */
#define GET_ROOT_LOGGER() acid::LoggerMgr::instance()->get_root()

/**
 * @brief 通过日志器的名字获取日志器
 *
 */
#define GET_LOGGER_BY_NAME(name) acid::LoggerMgr::instance()->get_logger(name)

/**
 * @brief 使用流式输出的方法写入日志
 * @details 构建一个临时的LogEventWrap对象来写入日志, 析构时将日志用LogAppenders写出
 */
#define LOG_LEVEL(logger, level)                                                             \
    if (level <= logger->get_level())                                                        \
    acid::LogEventWrap(                                                                      \
        logger, acid::LogEvent::ptr(new acid::LogEvent(                                      \
                    logger->get_name(), level, __FILE__, __LINE__, 0, acid::get_thread_id(), \
                    acid::get_fiber_id(), time(0), acid::get_thread_name())))                \
        .get_log_event()                                                                     \
        ->ssout()

#define LOG_FATAL(logger) LOG_LEVEL(logger, acid::LogLevel::FATAL)
#define LOG_ALERT(logger) LOG_LEVEL(logger, acid::LogLevel::ALERT)
#define LOG_CRIT(logger) LOG_LEVEL(logger, acid::LogLevel::CRIT)
#define LOG_ERROR(logger) LOG_LEVEL(logger, acid::LogLevel::ERROR)
#define LOG_WARN(logger) LOG_LEVEL(logger, acid::LogLevel::WARN)
#define LOG_NOTICE(logger) LOG_LEVEL(logger, acid::LogLevel::NOTICE)
#define LOG_INFO(logger) LOG_LEVEL(logger, acid::LogLevel::INFO)
#define LOG_DEBUG(logger) LOG_LEVEL(logger, acid::LogLevel::DEBUG)

/**
 * @brief 使用c printf格式化输出日志
 * @details 构建一个临时的LogEventWrap对象来写入日志, 析构时将日志用LogAppenders写出
 */
#define LOG_FMT_LEVEL(logger, level, fmt, ...)                                               \
    if (level <= logger->get_level())                                                        \
    acid::LogEventWrap(                                                                      \
        logger, acid::LogEvent::ptr(new acid::LogEvent(                                      \
                    logger->get_name(), level, __FILE__, __LINE__, 0, acid::get_thread_id(), \
                    acid::get_fiber_id(), time(0), acid::get_thread_name())))                \
        .get_log_event()                                                                     \
        ->printf(fmt, ##__VA_ARGS__)

#define LOG_FMT_FATAL(logger, fmt, ...) \
    LOG_FMT_LEVEL(logger, acid::LogLevel::FATAL, fmt, ##__VA_ARGS__)
#define LOG_FMT_ALERT(logger, fmt, ...) \
    LOG_FMT_LEVEL(logger, acid::LogLevel::ALERT, fmt, ##__VA_ARGS__)
#define LOG_FMT_CRIT(logger, fmt, ...) \
    LOG_FMT_LEVEL(logger, acid::LogLevel::CRIT, fmt, ##__VA_ARGS__)
#define LOG_FMT_ERROR(logger, fmt, ...) \
    LOG_FMT_LEVEL(logger, acid::LogLevel::ERROR, fmt, ##__VA_ARGS__)
#define LOG_FMT_WARN(logger, fmt, ...) \
    LOG_FMT_LEVEL(logger, acid::LogLevel::WARN, fmt, ##__VA_ARGS__)
#define LOG_FMT_NOTICE(logger, fmt, ...) \
    LOG_FMT_LEVEL(logger, acid::LogLevel::NOTICE, fmt, ##__VA_ARGS__)
#define LOG_FMT_INFO(logger, fmt, ...) \
    LOG_FMT_LEVEL(logger, acid::LogLevel::INFO, fmt, ##__VA_ARGS__)
#define LOG_FMT_DEBUG(logger, fmt, ...) \
    LOG_FMT_LEVEL(logger, acid::LogLevel::DEBUG, fmt, ##__VA_ARGS__)

namespace acid {
/*!
 * @brief 日志级别
 */
class LogLevel {
public:
    enum Level {
        // 致命情况,系统不可用
        FATAL = 0,
        // 高优先级情况, 例如数据库系统崩溃
        ALERT = 1,
        // 严重错误, 例如硬盘错误
        CRIT = 2,
        // 错误
        ERROR = 3,
        // 警告
        WARN = 4,
        // 正常, 但是需要注意
        NOTICE = 5,
        // 一般信息
        INFO = 6,
        // 调试信息
        DEBUG = 7,
        // 未设置
        NOTSET = 8
    };

    /*!
     * @brief 输出级别对应的string
     */
    static const char* to_string(LogLevel::Level level);
    /*!
     * @brief 从string转换为LogLevel的枚举级别
     */
    static LogLevel::Level from_string(const std::string& str);
};

/*!
 * @brief 日志事件, 将日志的固定信息输入到LogEvent类中,
 * 后续可以通过格式化或者流输出日志信息
 */
class LogEvent {
public:
    using ptr = std::shared_ptr<LogEvent>;

    /**
     * @brief 构造函数
     * @param[in] logger_name 日志器名称
     * @param[in] level 日志级别
     * @param[in] file 文件名
     * @param[in] line 行号
     * @param[in] elapse 从日志器创建开始到当前的累计运行毫秒
     * @param[in] thead_id 线程id
     * @param[in] fiber_id 协程id
     * @param[in] time UTC时间
     * @param[in] thread_name 线程名称
     */
    LogEvent(const std::string& logger_name, LogLevel::Level level, const char* file, int32_t line,
             int64_t elapse, uint32_t thread_id, uint64_t fiber_id, time_t time,
             const std::string& thread_name);

    LogLevel::Level get_level() const {
        return level_;
    }

    std::string get_content() const {
        // 获取日志内容
        return ss_.str();
    }

    std::string get_file() const {
        return file_;
    }

    int32_t get_line() const {
        return line_;
    }

    int64_t get_elapse() const {
        return elapse_;
    }

    uint32_t get_thread_id() const {
        return thread_id_;
    }

    uint64_t get_fiber_id() const {
        return fiber_id_;
    }

    time_t get_time() const {
        return time_;
    }

    const std::string& get_thread_name() const {
        return thread_name_;
    }

    const std::string& get_logger_name() const {
        return logger_name_;
    }

    /*!
     * @brief 使用流式输出日志信息
     */
    std::stringstream& ssout() {
        return ss_;
    }

    /*!
     * @brief C样式格式化输出日志信息
     */
    void printf(const char* format, ...);

    void vprintf(const char* format, va_list ap);

private:
    // 日志级别
    LogLevel::Level level_;
    // 日志内容, 用sstream存储, 便于流式输出
    std::stringstream ss_;
    // 文件名
    const char* file_;
    // 行号
    int32_t line_;
    // 从日志开始运行到现在的时间
    int64_t elapse_;
    // 线程ID
    uint32_t thread_id_;
    // 协程ID
    uint64_t fiber_id_;
    // UTC时间戳
    time_t time_;
    // 线程名
    std::string thread_name_;
    // 日志器名称
    std::string logger_name_;
};

/*!
 * @brief 用于解析自定义的格式化字符串
 */
class LogFormatter {
public:
    using ptr = std::shared_ptr<LogFormatter>;

    /**
     * @brief 构造函数
     * @param[in] pattern 格式模板，参考sylar与log4cpp
     * @details 模板参数说明：
     * - %%m 消息
     * - %%p 日志级别
     * - %%c 日志器名称
     * - %%d 日期时间，后面可跟一对括号指定时间格式，比如%%d{%%Y-%%m-%%d
     * %%H:%%M:%%S}，这里的格式字符与C语言strftime一致
     * - %%r 该日志器创建后的累计运行毫秒数
     * - %%f 文件名
     * - %%l 行号
     * - %%t 线程id
     * - %%F 协程id
     * - %%N 线程名称
     * - %%% 百分号
     * - %%T 制表符
     * - %%n 换行
     *
     * 默认格式：%%d{%%Y-%%m-%%d
     * %%H:%%M:%%S}%%T%%t%%T%%N%%T%%F%%T[%%p]%%T[%%c]%%T%%f:%%l%%T%%m%%n
     *
     * 默认格式描述：年-月-日 时:分:秒 [累计运行毫秒数] \\t 线程id \\t 线程名称
     * \\t 协程id \\t [日志级别] \\t [日志器名称] \\t 文件名:行号 \\t 日志消息
     * 换行符
     */
    explicit LogFormatter(const std::string& pattern =
                              "%d{%Y-%m-%d %H:%M:%S} [%rms]%T%t%T%N%T%F%T[%p]%T[%c]%T%f: %l%T%m%n");

    /*!
     * @brief 初始化, 解析日志模板字符串
     */
    void init();

    bool is_error() const {
        return is_error_;
    }

    /*!
     * @brief 返回格式化的字符串文本或流对象
     */
    std::string format(LogEvent::ptr event);

    std::ostream& format(std::ostream& os, LogEvent::ptr event);

    std::string get_pattern() const {
        return pattern_;
    }

public:
    /*!
     * @brief 字符串格式化项, 字符串格式化以%开头
     */
    class FormatItem {
    public:
        using ptr = std::shared_ptr<FormatItem>;

        virtual ~FormatItem() = default;

        virtual void format(std::ostream& os, LogEvent::ptr event) = 0;
    };

private:
    // 日志格式模板
    std::string pattern_;
    // 解析后的格式模板数组
    std::vector<FormatItem::ptr> items_;
    bool is_error_;
};

/*!
 * @brief 日志输出器
 */
class LogAppender {
public:
    using ptr = std::shared_ptr<LogAppender>;
    using MutexType = Spinlock;
    using LockGuard = MutexType::Lock;

    LogAppender(LogLevel::Level level, LogFormatter::ptr formatter);

    virtual ~LogAppender() = default;

    void set_formatter(LogFormatter::ptr formatter);

    LogFormatter::ptr get_formatter();

    void set_level(LogLevel::Level level);

    LogLevel::Level get_level();

    virtual void log(LogLevel::Level level, LogEvent::ptr event) = 0;

    virtual std::string to_yaml_string() = 0;

protected:
    MutexType mutex_;
    LogFormatter::ptr formatter_;
    LogLevel::Level level_;
};

/*!
 * @brief 输出到标准输出, 即控制台
 */
class StdoutLogAppender : public LogAppender {
public:
    static LogAppender::ptr create(LogLevel::Level level = LogLevel::DEBUG);

    StdoutLogAppender(LogLevel::Level level = LogLevel::DEBUG);

    void log(LogLevel::Level level, LogEvent::ptr event) override;

    std::string to_yaml_string() override;

};  // class StdoutLogAppender : public LogAppender

/*!
 * @brief 输出到文件
 */
class AsyncLogger;
class FileLogAppender : public LogAppender {
public:
    static LogAppender::ptr create(const std::string& filename,
                                   LogLevel::Level level = LogLevel::DEBUG);

    FileLogAppender(const std::string& filename, LogLevel::Level level);

    ~FileLogAppender();

    void log(LogLevel::Level level, LogEvent::ptr event) override;

    std::string to_yaml_string() override;

    bool reopen();

private:
    std::string filename_;
    std::ofstream file_stream_;
    uint64_t last_time_;
    std::shared_ptr<AsyncLogger> m_async_logger_appender;

};  // class FileLogAppender : public LogAppender

/*!
 * @brief 日志器, 可以同时输出日志到多个appender
 */
class Logger {
public:
    using ptr = std::shared_ptr<Logger>;
    using MutexType = Spinlock;
    using LockGuard = MutexType::Lock;

    Logger(LogLevel::Level level, const std::string& name = "root");

    Logger(const std::string& name);

    void log(LogLevel::Level level, LogEvent::ptr event);

    void add_appender(LogAppender::ptr appender);

    void delete_appender(LogAppender::ptr appender);

    void clear_appender();

    LogLevel::Level get_level();

    void set_level(LogLevel::Level level);

    std::string get_name();

    void set_name(std::string name);

    std::string to_yaml_string();

private:
    std::string name_;
    LogLevel::Level level_;
    std::vector<LogAppender::ptr> appenders_;
    MutexType mutex_;

};  // class Logger

class LogEventWrap {
public:
    LogEventWrap(Logger::ptr logger, LogEvent::ptr event);

    ~LogEventWrap();

    LogEvent::ptr get_log_event() const {
        return event_;
    }

private:
    Logger::ptr logger_;
    LogEvent::ptr event_;

};  // class LogEventWrap

/*!
 * @brief 管理多个日志器
 */
class LoggerManager {
public:
    using MutexType = Spinlock;
    using LockGuard = ScopedLockImpl<MutexType>;

    LoggerManager();

    Logger::ptr get_logger(const std::string& name);

    Logger::ptr get_root() {
        return root_;
    }

    std::string to_yaml_string();

private:
    std::map<std::string, Logger::ptr> loggers_;
    Logger::ptr root_;
    MutexType mutex_;

};  // class LogManager

/// @brief 日志管理器单例
using LoggerMgr = Singleton<LoggerManager>;

}  // namespace acid

#endif