/**
 * @file env.h
 * @author kbjcx (leitch.lu@outlook.com)
 * @brief 管理环境变量
 * @version 0.1
 * @date 2023-06-24
 *
 * @copyright Copyright (c) 2023
 *
 */

#ifndef ACID_ENV_H
#define ACID_ENV_H

#include "mutex.h"
#include "singleton.h"

#include <map>
#include <vector>

namespace acid {

class Env {
public:
    using RWMutexType = RWMutex;
    using WriteLockGuard = RWMutexType::WriteLock;
    using ReadLockGuard = RWMutexType::ReadLock;

    /*!
     * @brief 解析命令行参数
     * @param argc
     * @param argv
     * @return
     */
    bool init(int argc, char** argv);

    /*!
     * @brief 存储选项的key-value
     * @param key
     * @param val
     */
    void add(const std::string& key, const std::string& val = "");

    bool has(const std::string& key);

    void del(const std::string& key);

    /*!
     * @brief 存在选项则返回value, 不存在则返回default_value
     * @param key
     * @param default_value
     * @return
     */
    std::string get(const std::string& key, const std::string& default_value = "");

    /*!
     * @brief 添加选项及其说明到help列表
     * @param key
     * @param description
     */
    void add_help(const std::string& key, const std::string& description);

    /*!
     * @brief 移除该选项说明
     * @param key
     */
    void remove_help(const std::string& key);

    /*!
     * @brief 打印选项说明到控制台
     */
    void print_help();

    const std::string& get_exe() const {
        return exe_;
    }

    const std::string& get_cwd() const {
        return cwd_;
    }

    bool set_env(const std::string& key, const std::string& val);

    std::string get_env(const std::string& key, const std::string& default_value = "");

    /*!
     * @brief 获取path的绝对路径
     * @param path
     * @return
     */
    std::string get_absolute_path(const std::string& path) const;

    std::string get_absolute_workpath(const std::string& path) const;

    std::string get_config_path();

private:
    RWMutexType m_mutex;
    std::map<std::string, std::string> args_;
    std::vector<std::pair<std::string, std::string>> helps_;

    std::string program_;
    std::string exe_;
    std::string cwd_;  // 绝对路径的上一级路径

};  // class Env

using EnvMgr = Singleton<Env>;

};  // namespace acid

#endif