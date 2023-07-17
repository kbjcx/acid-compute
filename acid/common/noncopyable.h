/*!
 *@file noncopyable.h
 *@brief 不可拷贝对象封装
 *@version 0.1
 *@date 2023-6-23
 */
#ifndef DF_NONCOPYABLE_H
#define DF_NONCOPYABLE_H

namespace acid {

    class Noncopyable {
    public:
        Noncopyable() = default;

        ~Noncopyable() = default;

        Noncopyable(const Noncopyable &) = delete;

        Noncopyable &operator=(const Noncopyable &) = delete;

    }; // class Noncopyable

} // namespace acid

#endif //DF_NONCOPYABLE_H
