

#ifndef DF_ACID_COMMON_UTIL_H_
#define DF_ACID_COMMON_UTIL_H_

#include "cxxabi.h"

#include <bit>
#include <byteswap.h>
#include <concepts>
#include <cinttypes>
#include <string>
#include <vector>
#include <iostream>

namespace acid {

// bit
/*!
 * @brief 字节序转换
 */
template <std::integral T>
constexpr T byte_swap(T value) {
    if constexpr (sizeof(T) == sizeof(uint16_t)) {
        return (T) bswap_16((uint16_t) value);
    }
    else if constexpr (sizeof(T) == sizeof(uint32_t)) {
        return (T) bswap_32((uint32_t) value);
    }
    else if constexpr (sizeof(T) == sizeof(uint64_t)) {
        return (T) bswap_64((uint64_t) value);
    }
}

/*!
 * @brief 网络字节序与主机字节序的转换
 */
template <std::integral T>
constexpr T endian_cast(T value) {
    if constexpr (sizeof(T) == sizeof(uint8_t) || std::endian::native == std::endian::big) {
        return value;
    }
    else if constexpr (std::endian::native == std::endian::little) {
        return byte_swap(value);
    }
}

pid_t get_thread_id();

uint64_t get_fiber_id();

std::string get_thread_name();

/**
 * @brief 获取当前启动的毫秒数
 *
 * @return uint64_t
 */
uint64_t get_elapsed_ms();

template <class T>
const char* type_to_name() {
    static const char* s_name = abi::__cxa_demangle(typeid(T).name(), nullptr, nullptr, nullptr);
    return s_name;
}

/*!
 * @brief 文件操作
 */
class FSUtil {
public:
    /*!
     * @brief
     * 递归列举指定目录下所有指定后缀的常规文件，如果不指定后缀，则遍历所有文件，返回的文件名带路径
     * @param[out] files 文件列表
     * @param[in] path 路径
     * @param[in] subfix 后缀名
     */
    static void list_all_file(std::vector<std::string>& files, const std::string& path,
                              const std::string& subfix);

    /*!
     * @brief 创建路径, 相当于mkdir -p
     */
    static bool mkdir(const std::string& dirname);

    /*!
     * @brief 判断指定pid文件指定的pid是否正在运行，使用kill(pid, 0)的方式判断
     * @param[in] pid_file 保存进程号的文件
     */
    static bool is_running_pid_file(const std::string& pid_file);

    /*!
     * @brief 删除文件或路径
     */
    static bool rm(const std::string& path);

    /*!
     * @brief 移动文件或路径
     */
    static bool mv(const std::string& from, const std::string& to);

    /*!
     * @brief 返回绝对路径
     * @param[in] path
     * @param[out] return_path
     */
    static bool real_path(const std::string& path, std::string& return_path);

    /*!
     * @brief 创建符号链接
     * @param[in] from 目标
     * @param[in] to 链接路径
     */
    static bool symlink(const std::string& from, const std::string& to);

    /*!
     * @brief 删除文件
     * @param[in] filename文件名
     * @param[in] exist 是否存在
     * @note 内部仍然会判断一次文件是否真的不存在
     */
    static bool unlink(const std::string& filename, bool exist = false) {
        // TODO unlink
    }

    /*!
     * @brief 返回文件，即路径中最后一个/前面的部分，不包括/本身，如果未找到，则返回filename
     */
    static std::string dirname(const std::string& filename);

    /*!
     * @brief 返回文件名, 即最后一个/之后的部分
     */
    static std::string basename(const std::string& filename);

    /*!
     * @brief 以只读方式打开
     * @param[in] ifs 文件流
     * @param[in] filename 文件名
     * @param[in] mode 打开方式
     */
    static bool open_for_read(std::ifstream& ifs, const std::string& filename,
                              std::ios_base::openmode mode);

    /*!
     * @brief 以只写方式打开
     * @param[in] ofs 文件流
     * @param[in] filename 文件名
     * @param[in] mode 打开方式
     */
    static bool open_for_write(std::ofstream& ofs, const std::string& filename,
                               std::ios_base::openmode mode);
};

}  // namespace acid

#endif  // DF_ACID_COMMON_UTIL_H_
