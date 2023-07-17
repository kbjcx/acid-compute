#include "config.h"
#include "env.h"

#include <sys/stat.h>

namespace acid {

static Logger::ptr logger = GET_LOGGER_BY_NAME("system");

ConfigVarBase::ptr Config::look_up_base(const std::string& name) {
    ReadLockGuard lock(get_mutex());
    auto it = get_datas().find(name);
    return it == get_datas().end() ? nullptr : it->second;
}

static void list_all_members(const std::string& prefix, const YAML::Node& node,
                             std::list<std::pair<std::string, const YAML::Node>>& output) {
    if (prefix.find_first_not_of("qazxswedcvfrtgbnhyujmkiolp._0123456789") != std::string::npos) {
        LOG_ERROR(logger) << "Config invalid name: " << prefix << " : " << node;
        return ;
    }
    output.emplace_back(prefix, node);
    if (node.IsMap()) {
        for (auto it = node.begin(); it != node.end(); ++it) {
            // 递归输出Node内容
            list_all_members(prefix.empty() ? it->first.Scalar() : prefix + "." + it->first.Scalar(), it->second, output);
        }
    }
}

void Config::load_from_yaml(const YAML::Node& root) {
    std::list<std::pair<std::string, const YAML::Node>> all_nodes;
    list_all_members("", root, all_nodes);
    for (auto& node : all_nodes) {
        std::string key = node.first;
        if (key.empty()) {
            continue ;
        }
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);
        auto var = look_up_base(key);
        if (var) {
            if (node.second.IsScalar()) {
                var->from_string(node.second.Scalar());
            }
            else {
                std::stringstream ss;
                ss << node.second;
                var->from_string(ss.str());
            }
        }
    }
}

static std::map<std::string, uint64_t> s_file_modify_time;
static Mutex s_mutex;

void Config::load_from_conf_dir(const std::string& path, bool force) {
    std::string absolute_path = EnvMgr::instance()->get_absolute_path(path);
    std::vector<std::string> files;
    FSUtil::list_all_file(files, absolute_path, ".yml");

    for (auto& file : files) {
        {
            struct stat st;
            lstat(file.c_str(), &st);
            Mutex::Lock lock(s_mutex);
            if (!force && s_file_modify_time[file] == static_cast<uint64_t>(st.st_mtime)) {
                continue;
            }
            s_file_modify_time[file] = st.st_mtime;
        }
        try {
            YAML::Node root = YAML::LoadFile(file);
            load_from_yaml(root);
            LOG_INFO(logger) << "load config file = " << file << " OK";
        }
        catch (...) {
            LOG_ERROR(logger) << "load config file = " << file << " failed";
        }
    }
}






}  // namespace acid