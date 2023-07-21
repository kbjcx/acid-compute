/**
 * @file traits.h
 * @author kbjcx (lulu5v@163.com)
 * @brief
 * @version 0.1
 * @date 2023-07-20
 *
 * @copyright Copyright (c) 2023
 *
 */

#ifndef ACID_TRAITS_H
#define ACID_TRAITS_H

#include <functional>

namespace acid {

template <class T>
struct function_traits;

// 普通函数
template <class ReturnType, class... Args>
struct function_traits<ReturnType(Args...)> {
    enum { arity = sizeof...(Args) };                        // 参数数量
    using return_type = ReturnType;                          // 返回类型
    using function_type = ReturnType(Args...);               // 函数类型
    using stl_function_type = std::function<function_type>;  // std::function类型
    using pointer = ReturnType (*)(Args...);                 // 函数指针类型

    template <size_t I>
    struct args {
        static_assert(I < arity, "index is out of range, index must less than sizeof Args");
        // 返回第I个元素的类型
        using type = typename std::tuple_element<I, std::tuple<Args...>>::type;
    };

    // 返回去除const volatile 和引用的类型构成的tuple类型
    using tuple_type = std::tuple<std::remove_cv_t<std::remove_reference_t<Args>>...>;
    // 返回去除const和引用的类型构成的tuple类型
    using bare_tuple_type = std::tuple<std::remove_const_t<std::remove_reference_t<Args>>...>;
};

// 函数指针类型
template <class ReturnType, class... Args>
struct function_traits<ReturnType (*)(Args...)> : function_traits<ReturnType(Args...)> {};

// std::function类型
template <class ReturnType, class... Args>
struct function_traits<std::function<ReturnType(Args...)>> : function_traits<ReturnType(Args...)> {
};

// 类成员函数
#define FUNCTION_TRAITS(...)                                               \
    template <class ReturnType, class ClassType, class... Args>            \
    struct function_traits<ReturnType (ClassType::*)(Args...) __VA_ARGS__> \
        : function_traits<ReturnType(Args...)> {};

FUNCTION_TRAITS()
FUNCTION_TRAITS(const)
FUNCTION_TRAITS(volatile)
FUNCTION_TRAITS(const volatile)

// 可调用对象萃取
template <class Callable>
struct function_traits : function_traits<decltype(&Callable::operator())> {};


template <class Function>
typename function_traits<Function>::stl_function_type to_function(const Function& lambda) {
    return static_cast<typename function_traits<Function>::stl_function_type>(lambda);
}

// 转化为std::function
template <class Function>
typename function_traits<Function>::stl_function_type to_function(Function&& lambda) {
    return static_cast<typename function_traits<Function>::stl_function_type>(
        std::forward<Function>(lambda));
}

// 转化为函数指针
template <class Function>
typename function_traits<Function>::pointer to_function_pointer(const Function& lambda) {
    return static_cast<typename function_traits<Function>::pointer>(lambda);
}


}  // namespace acid

#endif