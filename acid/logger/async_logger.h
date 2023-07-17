#ifndef ACID_ASYNC_LOGGER_H
#define ACID_ASYNC_LOGGER_H

#include "acid/common/mutex.h"
#include "acid/common/singleton.h"
#include "acid/common/thread.h"

#include <cstring>
#include <queue>
#include <string>

namespace acid {

class LogBuffer {
public:
    using ptr = std::shared_ptr<LogBuffer>;

    LogBuffer();
    ~LogBuffer();

    void append(const char* buffer, size_t len);

    const char* data() const {
        return m_data;
    }

    int length() const {
        return (int) (m_cur_ptr - m_data);
    }

    char* current() {
        return m_cur_ptr;
    }

    int available() const {
        return (int) (end() - m_cur_ptr);
    }

    void add(int len) {
        m_cur_ptr += len;
    }

    void reset() {
        m_cur_ptr = m_data;
    }

    void bzero() {
        memset(m_data, 0, BUFFER_SIZE);
    }


private:
    enum { BUFFER_SIZE = 1024 * 1024 };

    const char* end() const {
        return m_data + BUFFER_SIZE;
    }

private:
    char m_data[BUFFER_SIZE];
    char* m_cur_ptr;
};

class AsyncLogger {
public:
    using ptr = std::shared_ptr<AsyncLogger>;

    AsyncLogger();
    explicit AsyncLogger(std::string file);
    ~AsyncLogger();

    void append(const std::string log_line);

    void start();

    void run();

private:
    enum { BUFFER_NUM = 4 };

    Mutex mutex_;
    Cond cond_;
    std::string file_;
    FILE* file_fd;
    bool run_;
    Thread::ptr m_thread;

    LogBuffer buffers_[BUFFER_NUM];
    LogBuffer* cur_buffer_;
    std::queue<LogBuffer*> free_buffers_;
    std::queue<LogBuffer*> flush_buffers_;
};

};  // namespace acid

#endif