/**
 * @file daemon.h
 * @author kbjcx (lulu5v@163.com)
 * @brief 守护进程
 * @version 0.1
 * @date 2023-07-17
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#ifndef ACID_DAEMON_H
#define ACID_DAEMON_H

#include "singleton.h"

#include <functional>
#include <unistd.h>

namespace acid {

struct ProcessInfo {
    pid_t parent_id = 0;             // 父进程ID
    pid_t main_id = 0;               // 子进程ID
    uint64_t parent_start_time = 0;  // 父进程启动时间
    uint64_t main_start_time = 0;    // 主进程启动时间
    uint32_t restart_count = 0;      // 主进程重启次数

    std::string to_string() const;
};

using ProcessInfoMgr = Singleton<ProcessInfo>;


int start_daemon(int argc, char** argv, std::function<int(int, char**)> main_func, bool is_daemon);


}  // namespace acid


#endif