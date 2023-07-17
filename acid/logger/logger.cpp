#include "logger.h"

#include "async_logger.h"

#include <algorithm>
#include <cstdarg>
#include <iostream>

namespace acid {
const char *LogLevel::to_string(LogLevel::Level level) {
    switch (level) {
#define XX(name)         \
    case LogLevel::name: \
        return #name
        XX(FATAL);
        XX(ALERT);
        XX(CRIT);
        XX(ERROR);
        XX(WARN);
        XX(NOTICE);
        XX(INFO);
        XX(DEBUG);
#undef XX
        default:
            return "NOTSET";
    }
    return "NOTSET";
}

LogLevel::Level LogLevel::from_string(const std::string &str) {
    std::string upper_str(str);
    std::transform(str.begin(), str.end(), upper_str.begin(), ::toupper);
#define XX(level) \
    if (upper_str == #level) return LogLevel::level
    XX(FATAL);
    XX(ALERT);
    XX(CRIT);
    XX(ERROR);
    XX(WARN);
    XX(NOTICE);
    XX(INFO);
    XX(DEBUG);
#undef XX
    return NOTSET;
}

LogEvent::LogEvent(const std::string &logger_name, LogLevel::Level level, const char *file,
                   int32_t line, int64_t elapse, uint32_t thread_id, uint64_t fiber_id, time_t time,
                   const std::string &thread_name)
    : level_(level)
    , file_(file)
    , line_(line)
    , elapse_(elapse)
    , thread_id_(thread_id)
    , fiber_id_(fiber_id)
    , time_(time)
    , thread_name_(thread_name)
    , logger_name_(logger_name) {
}

void LogEvent::printf(const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    vprintf(format, ap);
    va_end(ap);
}

void LogEvent::vprintf(const char *format, va_list ap) {
    char *buffer = nullptr;
    int len = vasprintf(&buffer, format, ap);
    if (len != -1) {
        ss_ << std::string(buffer, len);
        free(buffer);
    }
}

class MessageFormatItem : public LogFormatter::FormatItem {
public:
    MessageFormatItem(const std::string &str) {};

    void format(std::ostream &os, LogEvent::ptr event) override {
        os << event->get_content();
    }
};

class LevelFormatItem : public LogFormatter::FormatItem {
public:
    LevelFormatItem(const std::string &str) {
    }

    void format(std::ostream &os, LogEvent::ptr event) override {
        os << LogLevel::to_string(event->get_level());
    }
};

class ElapseFormatItem : public LogFormatter::FormatItem {
public:
    ElapseFormatItem(const std::string &str) {
    }

    void format(std::ostream &os, LogEvent::ptr event) override {
        os << event->get_elapse();
    }
};

class LoggerNameFormatItem : public LogFormatter::FormatItem {
public:
    LoggerNameFormatItem(const std::string &str) {
    }

    void format(std::ostream &os, LogEvent::ptr event) override {
        os << event->get_logger_name();
    }
};

class ThreadIdFormatItem : public LogFormatter::FormatItem {
public:
    ThreadIdFormatItem(const std::string &str) {
    }

    void format(std::ostream &os, LogEvent::ptr event) override {
        os << event->get_thread_id();
    }
};

class FiberIdFormatItem : public LogFormatter::FormatItem {
public:
    FiberIdFormatItem(const std::string &str) {
    }

    void format(std::ostream &os, LogEvent::ptr event) override {
        os << event->get_fiber_id();
    }
};

class ThreadNameFormatItem : public LogFormatter::FormatItem {
public:
    ThreadNameFormatItem(const std::string &str) {
    }

    void format(std::ostream &os, LogEvent::ptr event) override {
        os << event->get_thread_name();
    }
};

class DateTimeFormatItem : public LogFormatter::FormatItem {
public:
    DateTimeFormatItem(const std::string &format = "%Y-%m-%d %H:%M:%S") : format_(format) {
    }

    void format(std::ostream &os, LogEvent::ptr event) override {
        tm struct_tm {};
        time_t time = event->get_time();
        localtime_r(&time, &struct_tm);
        char buffer[64];
        strftime(buffer, sizeof(buffer), format_.c_str(), &struct_tm);
        os << buffer;
    }

private:
    std::string format_;
};

class FileNameFormatItem : public LogFormatter::FormatItem {
public:
    FileNameFormatItem(const std::string &str) {
    }

    void format(std::ostream &os, LogEvent::ptr event) override {
        os << event->get_file();
    }
};

class LineFormatItem : public LogFormatter::FormatItem {
public:
    LineFormatItem(const std::string &str) {
    }

    void format(std::ostream &os, LogEvent::ptr event) override {
        os << event->get_line();
    }
};

class StringFormatItem : public LogFormatter::FormatItem {
public:
    StringFormatItem(const std::string &str) : string_(str) {
    }

    void format(std::ostream &os, LogEvent::ptr event) override {
        os << string_;
    }

private:
    std::string string_;
};

class TabFormatItem : public LogFormatter::FormatItem {
public:
    TabFormatItem(const std::string &str) {
    }

    void format(std::ostream &os, LogEvent::ptr event) override {
        os << '\t';
    }
};

class NewLineFormatItem : public LogFormatter::FormatItem {
public:
    NewLineFormatItem(const std::string &str) {
    }

    void format(std::ostream &os, LogEvent::ptr event) override {
        os << std::endl;
    }
};

class PercentSignFormatItem : public LogFormatter::FormatItem {
public:
    PercentSignFormatItem(const std::string &str) {
    }

    void format(std::ostream &os, LogEvent::ptr event) override {
        os << "%";
    }
};

LogFormatter::LogFormatter(const std::string &pattern) : pattern_(pattern), is_error_(false) {
    init();
}

void LogFormatter::init() {
    // 按顺序存储解析到的pattern项
    // 每个pattern包含一个int和一个字符串, int为0表示常规字符串,
    // int为1表示需要转义 日期格式单独存储
    std::vector<std::pair<int, std::string>> patterns;
    // 临时存储常规字符串
    std::string tmp;
    // 日期格式字符串, 默认把位于%d后面的大括号对里的全部字符当做格式字符,
    // 不校验格式是否合法
    std::string date_format;
    // 是否解析错误
    bool error = false;
    // 是否正在解析常规字符
    bool parse_normal_string = true;
    size_t i = 0;
    for (; i < pattern_.size();) {
        std::string c = std::string(1, pattern_.at(i));
        if (c == "%") {
            if (parse_normal_string) {
                if (!tmp.empty()) {
                    patterns.emplace_back(0, tmp);
                }
                tmp.clear();
                parse_normal_string = false;  // 在解析常规字符串时碰到%, 表示开始解析模板字符
                ++i;
                continue;
            }
            else {
                // 当前字符为%%, 解析为%
                patterns.emplace_back(1, c);
                parse_normal_string = true;
                ++i;
                continue;
            }
        }
        else {
            if (parse_normal_string) {
                tmp += c;
                ++i;
                continue;
            }
            else {
                patterns.emplace_back(1, c);
                parse_normal_string = true;

                // 对%d进行特殊处理, 如果后面为一对大括号, 则表示日期格式化
                if (c != "d") {
                    ++i;
                    continue;
                }
                ++i;
                if (i < pattern_.size() && pattern_.at(i) != '{') {
                    continue;
                }
                ++i;
                while (i < pattern_.size() && pattern_.at(i) != '}') {
                    date_format.push_back(pattern_.at(i));
                    ++i;
                }
                if (pattern_.at(i) != '}') {
                    // 右括号没有闭合
                    std::cout << "[ERROR] LogFormatter::init() "
                              << "pattern: [" << pattern_ << "] '{' not closed" << std::endl;
                    error = true;
                    break;
                }
                ++i;
                continue;
            }
        }
    }

    if (error) {
        is_error_ = true;
        return;
    }

    // 模板解析结束之后, 将剩余的tmp加入patterns
    if (!tmp.empty()) {
        patterns.emplace_back(0, tmp);
        tmp.clear();
    }
#define XX(str, C)                                                                     \
    {                                                                                  \
#str, [](const std::string &format) { return FormatItem::ptr(new C(format)); } \
    }
    static std::unordered_map<std::string, std::function<FormatItem::ptr(const std::string &)>>
        format_items = {
            XX(m, MessageFormatItem),     XX(p, LevelFormatItem),    XX(c, LoggerNameFormatItem),
            XX(r, ElapseFormatItem),      XX(f, FileNameFormatItem), XX(l, LineFormatItem),
            XX(t, ThreadIdFormatItem),    XX(F, FiberIdFormatItem),  XX(N, ThreadNameFormatItem),
            XX(%, PercentSignFormatItem), XX(T, TabFormatItem),      XX(n, NewLineFormatItem),
        };
#undef XX

    for (auto &pattern : patterns) {
        if (pattern.first == 0) {
            items_.emplace_back(new StringFormatItem(pattern.second));
        }
        else if (pattern.second == "d") {
            items_.emplace_back(new DateTimeFormatItem(date_format));
        }
        else {
            auto it = format_items.find(pattern.second);
            if (it == format_items.end()) {
                std::cout << "[ERROR] LogFormatter::init() "
                          << "pattern: [" << pattern_ << "] "
                          << "unknown format item: " << pattern.second << std::endl;
                is_error_ = true;
                break;
            }
            else {
                items_.push_back(it->second(pattern.second));
            }
        }
    }
}

std::string LogFormatter::format(LogEvent::ptr event) {
    std::stringstream ss;
    for (auto &item : items_) {
        item->format(ss, event);
    }
    return ss.str();
}

std::ostream &LogFormatter::format(std::ostream &os, LogEvent::ptr event) {
    for (auto &item : items_) {
        item->format(os, event);
    }
    return os;
}

LogAppender::LogAppender(LogLevel::Level level, LogFormatter::ptr formatter)
    : formatter_(formatter), level_(level) {
}

void LogAppender::set_formatter(LogFormatter::ptr formatter) {
    LockGuard lock(mutex_);
    formatter_ = formatter;
}

LogFormatter::ptr LogAppender::get_formatter() {
    LockGuard lock(mutex_);
    return formatter_;
}

LogLevel::Level LogAppender::get_level() {
    LockGuard lock(mutex_);
    return level_;
}

void LogAppender::set_level(LogLevel::Level level) {
    LockGuard lock(mutex_);
    level_ = level;
}

LogAppender::ptr StdoutLogAppender::create(LogLevel::Level level) {
    return std::make_shared<StdoutLogAppender>(level);
}

StdoutLogAppender::StdoutLogAppender(LogLevel::Level level)
    : LogAppender(level, LogFormatter::ptr(new LogFormatter)) {
}

void StdoutLogAppender::log(LogLevel::Level level, LogEvent::ptr event) {
    LockGuard lock(mutex_);
    if (level <= level_) {
        formatter_->format(std::cout, event);
    }
}

std::string StdoutLogAppender::to_yaml_string() {
    LockGuard lock(mutex_);
    // TODO 未完成
    return {};
}

LogAppender::ptr FileLogAppender::create(const std::string &filename, LogLevel::Level level) {
    return std::make_shared<FileLogAppender>(filename, level);
}

FileLogAppender::FileLogAppender(const std::string &filename, LogLevel::Level level)
    : filename_(filename)
    , LogAppender(level, std::make_shared<LogFormatter>())
    , m_async_logger_appender(new AsyncLogger(filename)) {
    // if (!reopen()) {
    //     std::cout << "log file open error" << std::endl;
    // }
}

FileLogAppender::~FileLogAppender() {
    // if (file_stream_) {
    //     file_stream_.close();
    // }
}

bool FileLogAppender::reopen() {
    LockGuard lock(mutex_);
    if (file_stream_) {
        file_stream_.close();
    }
    file_stream_.open(filename_, std::ios::app);

    // 将非bool类型强转为bool类型, 0为false, 其余为true
    return !file_stream_.bad();
}

void FileLogAppender::log(LogLevel::Level level, LogEvent::ptr event) {
    if (level <= level_) {
        // time_t time = event->get_time();
        // if (time > last_time_ + 3) {
        //     if (!reopen()) {
        //         std::cout << "log file open error" << std::endl;
        //     }
        //     last_time_ = time;
        // }

        // LockGuard lock(mutex_);
        // std::cout << formatter_->format(event) << std::endl;
        // file_stream_ << formatter_->format(event);
        m_async_logger_appender->append(formatter_->format(event));
    }
}

std::string FileLogAppender::to_yaml_string() {
    // TODO FileLogAppender::to_yaml_string()
    return {};
}

Logger::Logger(LogLevel::Level level, const std::string &name) : level_(level), name_(name) {
}

Logger::Logger(const std::string &name) : level_(LogLevel::DEBUG), name_(name) {
}

void Logger::log(LogLevel::Level level, LogEvent::ptr event) {
    LockGuard lock(mutex_);
    if (level <= level_) {
        for (auto &appender : appenders_) {
            appender->log(level, event);
        }
    }
}

void Logger::add_appender(LogAppender::ptr appender) {
    LockGuard lock(mutex_);
    appenders_.push_back(appender);
}

void Logger::delete_appender(LogAppender::ptr appender) {
    LockGuard lock(mutex_);
    for (auto it = appenders_.begin(); it != appenders_.end(); ++it) {
        if (*it == appender) {
            appenders_.erase(it);
            break;
        }
    }
}

void Logger::clear_appender() {
    LockGuard lock(mutex_);
    appenders_.clear();
}

LogLevel::Level Logger::get_level() {
    LockGuard lock(mutex_);
    return level_;
}

void Logger::set_level(LogLevel::Level level) {
    LockGuard lock(mutex_);
    level_ = level;
}

std::string Logger::get_name() {
    LockGuard lock(mutex_);
    return name_;
}

void Logger::set_name(std::string name) {
    LockGuard lock(mutex_);
    name_ = name;
}

LogEventWrap::LogEventWrap(Logger::ptr logger, LogEvent::ptr event)
    : logger_(logger), event_(event) {
}

LogEventWrap::~LogEventWrap() {
    logger_->log(event_->get_level(), event_);
}

LoggerManager::LoggerManager() : root_(new Logger(LogLevel::DEBUG, "root")), loggers_() {
    loggers_.insert({"root", root_});
}

Logger::ptr LoggerManager::get_logger(const std::string &name) {
    LockGuard lock(mutex_);
    auto it = loggers_.find(name);
    if (it != loggers_.end()) {
        return it->second;
    }

    // 没有的话默认创建同名日志器, 但是没有appender, 需要手动添加
    Logger::ptr logger(new Logger(name));
    loggers_.insert({name, logger});
    return logger;
}

std::string LoggerManager::to_yaml_string() {
    // TODO LoggerManager::to_yaml_string()
    return {};
}

}  // namespace acid