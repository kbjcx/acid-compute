#include "util.h"

#include <cstdint>
#include <ctime>
#include <string>
#include <sys/syscall.h>
#include <vector>
#include <sys/time.h>
#include <unistd.h>
#include <dirent.h>
#include <iostream>
#include <sys/stat.h>
#include <sys/types.h>
#include <execinfo.h>
#include <cstring>

namespace acid {

pid_t get_thread_id() {
    // 返回真实的线程ID, 每个进程的线程之间都不会重复
    return syscall(SYS_gettid);
}

uint64_t get_fiber_id() {
    // TODO get_fiber_id()
    return 0;
}

std::string get_thread_name() {
    char thread_name[16] = {0};
    pthread_getname_np(pthread_self(), thread_name, 16);
    return std::string(thread_name);
}

uint64_t get_elapsed_ms() {
    timespec ts {0};
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

void FSUtil::list_all_file(std::vector<std::string> &files,
                           const std::string &path, const std::string &subfix) {
    if (access(path.c_str(), 0) != 0) {
        return;
    }

    DIR *dir = opendir(path.c_str());
    if (dir == nullptr) {
        return;
    }

    struct dirent *dp = nullptr;
    while ((dp = readdir(dir)) != nullptr) {
        if (dp->d_type == DT_DIR) {
            if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, "..")) {
                continue;
            }
            list_all_file(files, path + "/" + dp->d_name, subfix);
        }
        else if (dp->d_type == DT_REG) {
            std::string filename(dp->d_name);
            if (subfix.empty()) {
                files.push_back(path + "/" + filename);
            }
            else {
                if (filename.size() < subfix.size()) {
                    continue;
                }
                if (filename.substr(filename.size() - subfix.size()) ==
                    subfix) {
                    files.push_back(path + "/" + filename);
                }
            }
        }
    }
    closedir(dir);
}

static int __lstat(const char *file, struct stat *st = nullptr) {
    struct stat lst;
    int ret = lstat(file, &lst);
    if (st) {
        *st = lst;
    }
    return ret;
}

static int __mkdir(const char *dirname) {
    if (access(dirname, F_OK) == 0) {
        return 0;
    }
    return mkdir(dirname, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
}

};  // namespace acid