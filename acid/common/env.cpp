#include "env.h"

#include "acid/logger/logger.h"

#include <cstring>
#include <iomanip>
#include <iostream>

namespace acid {

static acid::Logger::ptr logger = GET_LOGGER_BY_NAME("system");

bool Env::init(int argc, char** argv) {
    char link[1024] = {0};
    char path[1024] = {0};
    sprintf(link, "/proc/%d/exe", getpid());
    // 用来找出符号链接所指向的位置
    readlink(link, path, sizeof(path));

    exe_ = path;

    auto pos = exe_.find_last_of('/');
    cwd_ = exe_.substr(0, pos) + "/";

    program_ = argv[0];

    char* key = nullptr;
    for (int i = 0; i < argc; ++i) {
        if (argv[i][0] == '-') {
            if (strlen(argv[i]) > 1) {
                if (key) {
                    // 如果key不为空，说明上一个也是-选项，但是没有value, 直接添加选项即可
                    add(key);
                }
                key = argv[i] + 1;
            }
            else {
                LOG_ERROR(logger) << "invalid arg index = " << i << "val = " << argv[i];
                return false;
            }
        }
        else {
            if (key) {
                // 当前字符为上一选项的value
                add(key, argv[i]);
                key = nullptr;
            }
            else {
                LOG_ERROR(logger) << "invalid arg index = " << i << "val = " << argv[i];
                return false;
            }
        }
    }

    if (key) add(key);
    return true;
}

void Env::add(const std::string& key, const std::string& val) {
    WriteLockGuard lock(m_mutex);
    args_[key] = val;
}

bool Env::has(const std::string& key) {
    ReadLockGuard lock(m_mutex);
    auto it = args_.find(key);
    return it != args_.end();
}

void Env::del(const std::string& key) {
    WriteLockGuard lock(m_mutex);
    args_.erase(key);
}

std::string Env::get(const std::string& key, const std::string& default_value) {
    ReadLockGuard lock(m_mutex);
    auto it = args_.find(key);
    return it != args_.end() ? it->second : default_value;
}

void Env::add_help(const std::string& key, const std::string& description) {
    remove_help(key);
    WriteLockGuard lock(m_mutex);
    helps_.emplace_back(key, description);
}

void Env::remove_help(const std::string& key) {
    WriteLockGuard lock(m_mutex);
    for (auto it = helps_.begin(); it != helps_.end();) {
        if (it->first == key) {
            it = helps_.erase(it);
        }
        else {
            ++it;
        }
    }
}

void Env::print_help() {
    ReadLockGuard lock(m_mutex);
    std::cout << "Usage: " << program_ << " [options]" << std::endl;
    for (auto& help : helps_) {
        std::cout << std::setw(5) << "-" << help.first << " : " << help.second << std::endl;
    }
}

bool Env::set_env(const std::string& key, const std::string& val) {
    // 利用系统调用setenv来实现
    return setenv(key.c_str(), val.c_str(), 1) == 0;
}

std::string Env::get_env(const std::string& key, const std::string& default_value) {
    // 通过系统调用getenv来实现
    const char* v = getenv(key.c_str());
    if (!v) {
        return default_value;
    }
    return v;
}

std::string Env::get_absolute_path(const std::string& path) const {
    if (path.empty()) {
        return "/";
    }
    if (path[0] == '/') {
        return path;
    }
    return cwd_ + path;
}

std::string Env::get_absolute_workpath(const std::string& path) const {
    if (path.empty()) {
        return "/";
    }
    if (path[0] == '/') {
        return path;
    }

    // TODO 配置文件
    return {};
}

std::string Env::get_config_path() {
    return get_absolute_path(get("c", "conf"));
}

};  // namespace acid