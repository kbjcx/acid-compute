#ifndef ACID_CONFIG_H
#define ACID_CONFIG_H

#include "acid/logger/logger.h"
#include "lexical_cast.h"
#include "mutex.h"
#include "util.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <yaml-cpp/yaml.h>

namespace acid {

/**
 * @brief 配置基本信息类
 */
class ConfigVarBase {
public:
    using ptr = std::shared_ptr<ConfigVarBase>;
    explicit ConfigVarBase(const std::string& name, const std::string& description = "")
    : m_name(name)
    , m_description() {
        std::transform(m_name.begin(), m_name.end(), m_name.begin(), ::tolower);
    }

    virtual ~ConfigVarBase() = default;

    const std::string& get_name() const {
        return m_name;
    }

    const std::string& get_description() const {
        return m_description;
    }

    /*!
     * @brief 将配置转成字符串
     * @return
     */
    virtual std::string to_string() = 0;

    /*!
     * @brief 从字符串获取配置
     * @param val
     * @return
     */
    virtual bool from_string(const std::string& val) = 0;

    /*!
     * @brief 返回配置参数的类型名称
     * @return
     */
    [[nodiscard]] virtual std::string get_typename() const = 0;

protected:
    std::string m_name;
    std::string m_description;
};

/*!
 * @brief 类型转换模板类, 只能用作基本类型之间的转换
 * @tparam From 来源类型
 * @tparam To 目标类型
 */
template <class From, class To>
class LexicalCast {
public:
    To operator()(const From& from) {
        return lexical_cast<To>(from);
    }
};

/*!
 * @brief 模板偏特化，将YAML string转为vector<To>
 * @tparam To
 */
template <class To>
class LexicalCast<std::string, std::vector<To>> {
public:
    std::vector<To> operator()(const std::string& str) {
        YAML::Node node = YAML::Load(str);
        std::vector<To> result;
        std::stringstream ss;
        for (auto item : node) {
            ss.str("");
            ss << item;
            result.push_back(LexicalCast<std::string, To>()(ss.str()));
        }
        return result;
    }
};

/*!
 * @brief 将vector<From>转为YAML string类型
 * @tparam From
 */
template <class From>
class LexicalCast<std::vector<From>, std::string> {
public:
    std::string operator()(const std::vector<From>& from) {
        YAML::Node node;
        for (auto& item : from) {
            node.push_back(YAML::Load(LexicalCast<From, std::string>()(item)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

template <class To>
class LexicalCast<std::string, std::list<To>> {
public:
    std::list<To> operator()(const std::string& str) {
        YAML::Node node = YAML::Load(str);
        std::list<To> result;
        std::stringstream ss;
        for (auto item : node) {
            ss.str("");
            ss << item;
            result.push_back(LexicalCast<std::string, To>()(ss.str()));
        }
        return result;
    }
};

template <class From>
class LexicalCast<std::list<From>, std::string> {
public:
    std::string operator()(const std::list<From>& from) {
        YAML::Node node;
        for (auto& item : from) {
            node.push_back(YAML::Load(LexicalCast<From, std::string>()(item)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

template <class To>
class LexicalCast<std::string, std::set<To>> {
public:
    std::set<To> operator()(const std::string& str) {
        YAML::Node node = YAML::Load(str);
        std::set<To> result;
        std::stringstream ss;
        for (auto item : node) {
            ss.str("");
            ss << item;
            result.template emplace(LexicalCast<std::string, std::set<To>>()(ss.str()));
        }
        return result;
    }
};

template <class From>
class LexicalCast<std::set<From>, std::string> {
public:
    std::string operator()(const std::set<From>& from) {
        YAML::Node node;
        for (auto& item : from) {
            node.push_back(YAML::Load(LexicalCast<From, std::string>()(item)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

template <class To>
class LexicalCast<std::string, std::unordered_set<To>> {
public:
    std::unordered_set<To> operator()(const std::string& str) {
        YAML::Node node = YAML::Load(str);
        std::unordered_set<To> result;
        std::stringstream ss;
        for (auto item : node) {
            ss.str("");
            ss << item;
            result.push_back(LexicalCast<std::string, To>()(ss.str()));
        }
        return result;
    }
};

template <class From>
class LexicalCast<std::unordered_set<From>, std::string> {
public:
    std::string operator()(const std::unordered_set<From>& from) {
        YAML::Node node;
        for (auto& item : from) {
            node.push_back(YAML::Load(LexicalCast<From, std::string>()(item)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

template <class To>
class LexicalCast<std::string, std::map<std::string, To>> {
public:
    std::map<std::string, To> operator()(const std::string& str) {
        YAML::Node node = YAML::Load(str);
        std::map<std::string, To> result;
        std::stringstream ss;
        for (auto item : node) {
            ss.str("");
            ss << item.second;
            To value = LexicalCast<std::string, To>()(ss.str());
            result.template emplace(item.first.Scalar(), value);
        }
        return result;
    }
};

template <class From>
class LexicalCast<std::map<std::string, From>, std::string> {
public:
    std::string operator()(const std::map<std::string, From>& from) {
        YAML::Node node;
        for (auto& item : from) {
            node[item.first] = YAML::Load(LexicalCast<From, std::string>()(item.second));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

template <class To>
class LexicalCast<std::string, std::unordered_map<std::string, To>> {
public:
    std::unordered_map<std::string, To> operator()(const std::string& str) {
        YAML::Node node = YAML::Load(str);
        std::unordered_map<std::string, To> result;
        std::stringstream ss;
        for (auto item : node) {
            ss.str("");
            ss << item.second;
            To value = LexicalCast<std::string, To>()(ss.str());
            result.template emplace(item.first.Scalar(), value);
        }

        return result;
    }
};

template <class From>
class LexicalCast<std::unordered_map<std::string, From>, std::string> {
public:
    std::string operator()(const std::unordered_map<std::string, From>& from) {
        YAML::Node node;
        for (auto& item : from) {
            node[item.first] = YAML::Load(LexicalCast<From, std::string>()(item.second));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

// FromStr T operator()(const std::string&)
// ToStr  std::string operator()(const T&)
template <class T, class FromStr = LexicalCast<std::string, T>, class ToStr = LexicalCast<T, std::string>>
class ConfigVar : public ConfigVarBase {
public:
    using ptr = std::shared_ptr<ConfigVar>;
    using RWMutexType = RWMutex;
    using ReadLockGuard = ReadScopedLockImpl<RWMutexType>;
    using WriteLockGuard = WriteScopedLockImpl<RWMutexType>;
    // 配置事件变更的回调函数
    using Callback = std::function<void(const T&, const T&)>;

    ConfigVar(const std::string& name, const T& value, const std::string description = "")
        : ConfigVarBase(name, description), m_val(value) {
    }

    // 将参数转换成YAML string
    std::string to_string() override {
        try {
            ReadLockGuard lock(m_mutex);
            return ToStr()(m_val);
        }
        catch (std::exception& e) {
            LOG_ERROR(GET_ROOT_LOGGER()) << "ConfigVar::to_string exception" << e.what()
                                         << " convert: " << typeid(m_val).name() << " to string.";
        }
    }

    // 从YAML string获取参数
    bool from_string(const std::string& val) override {
        try {
            set_value(FromStr()(val));
            return true;
        }
        catch (std::exception& e) {
            LOG_ERROR(GET_ROOT_LOGGER()) << "ConfigVar::fromString exception" << e.what() << " convert: string to"
                                         << typeid(m_val).name();
        }
        return false;
    }

    const T get_value() {
        ReadLockGuard lock(m_mutex);
        return m_val;
    }

    void set_value(const T& value) {
        {
            ReadLockGuard lock(m_mutex);
            if (value == m_val) {
                return;
            }
            // 配置参数有更改就触发该参数对应的所有回调函数
            for (auto& item : m_callbacks) {
                item.second(m_val, value);
            }
        }
        // 最后将值写入
        WriteLockGuard lock(m_mutex);
        m_val = value;
    }

    [[nodiscard]] std::string get_typename() const override {
        return type_to_name<T>();
    }

    /*!
     * @brief 添加对该参数的回调函数
     * @param cb
     * @return 返回回调函数编号，方便后续对其进行操作
     */
    uint64_t add_listener(Callback cb) {
        static uint64_t index = 0;
        WriteLockGuard lock(m_mutex);
        ++index;
        m_callbacks[index] = cb;
        return index;
    }

    /*!
     * @brief 删除回调
     * TODO 被删除的索引最好被回收利用
     */
    void del_listener(uint64_t key) {
        WriteLockGuard lock(m_mutex);
        m_callbacks.erase(key);
    }

    /*!
     * @brief 清除所有回调函数
     * TODO 应该可以把索引归0
     */
    void clear_listener() {
        WriteLockGuard lock(m_mutex);
        m_callbacks.clear();
    }

    Callback get_listener(uint64_t key) {
        ReadLockGuard lock(m_mutex);
        auto it = m_callbacks.find(key);
        return it == m_callbacks.end() ? nullptr : it->second;
    }

private:
    T m_val;
    std::map<uint64_t, Callback> m_callbacks;
    RWMutexType m_mutex;
};

// 管理类，创建并管理ConfigVar
class Config {
public:
    using RWMutexType = RWMutex;
    using ReadLockGuard = ReadScopedLockImpl<RWMutexType>;
    using WriteLockGuard = WriteScopedLockImpl<RWMutexType>;

    using ConfigVarMap = std::map<std::string, ConfigVarBase::ptr>;

    template <class T>
    static typename ConfigVar<T>::ptr look_up(const std::string& name, const T& value,
                                              const std::string& description = "") {
        WriteLockGuard lock(get_mutex());
        auto tmp = look_up<T>(name);
        // 已经存在
        if (tmp) {
            return tmp;
        }

        if (name.find_first_not_of("abcdefghijklmnopqrstuvwxyz._0123456789") != std::string::npos) {
            LOG_ERROR(GET_ROOT_LOGGER()) << "Lookup name invalid " << name;
            throw std::invalid_argument(name);
        }

        // 没找到则创建配置
        typename ConfigVar<T>::ptr v(new ConfigVar<T>(name, value, description));
        get_datas()[name] = v;
        return v;
    }

    template <class T>
    static typename ConfigVar<T>::ptr look_up(const std::string& name) {
        auto it = get_datas().find(name);
        if (it != get_datas().end()) {
            auto tmp = std::dynamic_pointer_cast<ConfigVar<T>>(it->second);
            if (tmp) {
                LOG_INFO(GET_ROOT_LOGGER()) << "look up name = " << name << " exists.";
                return tmp;
            }
            else { // key一样但是value类型不一样
                LOG_ERROR(GET_ROOT_LOGGER()) << "look up name = " << name << " exists but type not same"
                << typeid(T).name() << " real type " << it->second->get_typename() << " " << it->second->to_string();
                return nullptr;
            }
        }
        return nullptr;
    }

    // 解析yaml文件
    static void load_from_yaml(const YAML::Node& root);

    // 加载conf文件夹下的配置文件
    static void load_from_conf_dir(const std::string& path, bool force = false);

    // 查看当前配置表的信息
    static void visit(std::function<void(ConfigVarBase::ptr)> cb) {
        ConfigVarMap& m = get_datas();
        std::cout << "m size: " << m.size() << std::endl;
        for (auto & it : m) {
            cb(it.second);
        }
    }

private:
    static ConfigVarBase::ptr look_up_base(const std::string& name);

private:
    static ConfigVarMap& get_datas() {
        static ConfigVarMap s_datas;
        return s_datas;
    }

    static RWMutexType& get_mutex() {
        static RWMutexType s_mutex;
        return s_mutex;
    }
};


}  // namespace acid

#endif