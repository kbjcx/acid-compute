/*!
 *@file singleton.h
 *@brief 单例模式
 *@version 0.1
 *@date 2023-06-23
 */
#ifndef DF_SINGLETON_H
#define DF_SINGLETON_H

#include <memory>

template<class T, class X = void, int N = 0>
class Singleton {
public:
    static T *instance() {
        static T t;
        return &t;
    }

}; // class Singleton

template<class T, class X = void, int N = 0>
class SingletonPtr {
public:
    static std::shared_ptr<T> instance() {
        static auto ptr = std::make_shared<T>();
        return ptr;
    }

}; // class SingletonPtr


#endif //DF_SINGLETON_H
