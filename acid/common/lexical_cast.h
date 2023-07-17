/**
 * @file lexical.h
 * @author your name (you@domain.com)
 * @brief 用于将字面量与内置类型之间的转换
 * @version 0.1
 * @date 2023-06-25
 *
 * @copyright Copyright (c) 2023
 *
 */

#ifndef ACID_LEXICAL_CAST_H
#define ACID_LEXICAL_CAST_H

#include "acid/net/address.h"

#include <bitset>
#include <cstddef>
#include <string>
#include <type_traits>

namespace acid {

namespace detail {

    template <class To, class From>
    struct Converter {};

    /**
     * @brief to numeric
     *
     * @tparam From
     */
    template <class From>
    struct Converter<int, From> {
        static int convert(const From& from) {
            return std::stoi(from);
        }
    };

    template <class From>
    struct Converter<unsigned int, From> {
        static unsigned int convert(const From& from) {
            return std::stoul(from);
        }
    };

    template <class From>
    struct Converter<long, From> {
        static long convert(const From& from) {
            return std::stol(from);
        }
    };

    template <class From>
    struct Converter<unsigned long, From> {
        static unsigned long convert(const From& from) {
            return std::stoul(from);
        }
    };

    template <class From>
    struct Converter<long long, From> {
        static long long convert(const From& from) {
            return std::stoll(from);
        }
    };

    template <class From>
    struct Converter<unsigned long long, From> {
        static unsigned long long convert(const From& from) {
            return std::stoull(from);
        }
    };

    template <class From>
    struct Converter<double, From> {
        static double convert(const From& from) {
            return std::stod(from);
        }
    };

    template <class From>
    struct Converter<float, From> {
        static float convert(const From& from) {
            return std::stof(from);
        }
    };

    /**
     * @brief to bool
     *
     * @tparam From
     */
    template <class From>
    struct Converter<bool, From> {
        // enable_if 表示满足前一个条件时类型有效
        static typename std::enable_if<std::is_integral<From>::value, bool>::type convert(From from) {
            return from;
        }
    };

    bool check_bool(const char* from, const size_t len, const char* s);

    bool convert(const char* from);

    template <>
    struct Converter<bool, std::string> {
        static bool convert(const std::string& from) {
            return detail::convert(from.c_str());
        }
    };

    template <>
    struct Converter<bool, const char*> {
        static bool convert(const char* from) {
            return detail::convert(from);
        }
    };

    template <>
    struct Converter<bool, char*> {
        static bool convert(char* from) {
            return detail::convert(from);
        }
    };

    template <unsigned int N>
    struct Converter<bool, const char[N]> {
        static bool convert(const char from[N]) {
            return detail::convert(from);
        }
    };

    template <unsigned int N>
    struct Converter<bool, char[N]> {
        static bool convert(const char from[N]) {
            return detail::convert(from);
        }
    };

    /**
     * @brief to string
     *
     */
    template <class From>
    struct Converter<std::string, From> {
        static std::string convert(const From& from) {
            return std::to_string(from);
        }
    };
}  // namespace detail

template <class To, class From>
typename std::enable_if<!std::is_same<To, From>::value, To>::type lexical_cast(const From& from) {
    return detail::Converter<To, From>::convert(from);
}

template <class To, class From>
typename std::enable_if<std::is_same<To, From>::value, To>::type lexical_cast(const From& from) {
    return from;
}

}  // namespace acid

#endif