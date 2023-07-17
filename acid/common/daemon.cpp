#include "daemon.h"

#include "acid/logger/logger.h"
#include "config.h"

#include <cstring>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

namespace acid {

static auto logger = GET_LOGGER_BY_NAME("system");
static ConfigVar<uint32_t>::ptr g_daemon_restart_interval =
    Config::look_up("daemon.start_interval", static_cast<uint32_t>(5), "daemon restart interval");

std::string ProcessInfo::to_string() const {
    std::stringstream ss;
    // clang-format off
    ss  << "[ProcessInfo parent_id = " << parent_id
        << " main_id = " << main_id
        << " parent_start_time = " << parent_start_time
        << " main_start_time = " << main_start_time
        << " restart_count = " << restart_count << "]";
    // clang-format on
    return ss.str();
}

static int real_start(int argc, char** argv, std::function<int(int, char**)> main_func) {
    LOG_INFO(logger) << "process start pid = " << getpid();
    return main_func(argc, argv);
}

static int real_daemon(int argc, char** argv, std::function<int(int, char**)> main_func) {
    daemon(1, 0);
    ProcessInfoMgr::instance()->parent_id = getpid();
    ProcessInfoMgr::instance()->parent_start_time = time(0);
    while (true) {
        pid_t pid = fork();
        if (pid == 0) {
            // 子进程
            ProcessInfoMgr::instance()->main_id = getpid();
            ProcessInfoMgr::instance()->main_start_time = time(0);
            LOG_INFO(logger) << "process start pid = " << getpid();
            return real_start(argc, argv, main_func);
        }
        else if (pid < 0) {
            LOG_ERROR(logger) << "fork fail return = " << pid << "errno = " << errno
                              << "str error = " << strerror(errno);
            return -1;
        }
        else {
            // 父进程
            int status = 0;
            waitpid(pid, &status, 0);
            if (status) {
                LOG_ERROR(logger) << "child crash pid = " << pid << "status = " << status;
            }
            else {
                LOG_INFO(logger) << "child finished pid = " << pid;
                break;
            }
            ProcessInfoMgr::instance()->restart_count += 1;
            sleep(g_daemon_restart_interval->get_value());
        }
    }
    return 0;
}

int start_daemon(int argc, char** argv, std::function<int(int, char**)> main_func, bool is_daemon) {
    if (is_daemon) {
        return real_daemon(argc, argv, main_func);
    }
    return real_start(argc, argv, main_func);
}

};  // namespace acid