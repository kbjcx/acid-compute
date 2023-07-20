/**
 * @file route_strategy.h
 * @author kbjcx (lulu5v@163.com)
 * @brief
 * @version 0.1
 * @date 2023-07-20
 *
 * @copyright Copyright (c) 2023
 *
 */

#ifndef ACID_ROUTE_STRATEGY_H
#define ACID_ROUTE_STRATEGY_H

#include "acid/common/mutex.h"

#include <memory>
#include <vector>

namespace acid::rpc {

enum class Strategy {
    RANDOM,   // 随机算法
    POLLING,  // 轮询算法
    HASHIP    // 源地址hash算法
};

template <class T>
class RouteStrategy {
public:
    using ptr = std::shared_ptr<RouteStrategy>;

    virtual ~RouteStrategy() = default;

    virtual T& select(std::vector<T>& list) = 0;
};

namespace impl {

    template <class T>
    class RandomRouteStragetyImpl : public RouteStrategy<T> {
    public:
        T& select(std::vector<T>& list) override {
            srand(static_cast<unsigned int>(time(NULL)));
            return list[rand() % list.size()];
        }
    };

    template <class T>
    class PollingRouteStrategyImpl : public RouteStrategy<T> {
    public:
        T& select(std::vector<T>& list) {
            Mutex::Lock lock(m_mutex);
            if (m_index >= static_cast<int>(list.size())) {
                m_index = 0;
            }
            return list.at(m_index++);
        }

    private:
        int m_index = 0;
        Mutex m_mutex;
    };

    template <class T>
    class HashIpRouteStrategyImpl : public RouteStrategy<T> {
    public:
        T& select(std::vector<T>& list) {
            // TODO hash ip 路由选择算法
            return list.at(0);
        }
    };

}  // namespace impl

template <class T>
class RouteEngine {
public:
    static typename RouteStrategy<T>::ptr query_strategy(Strategy route_strategy) {
        switch (route_strategy) {
            case Strategy::RANDOM:
                return std::make_shared<impl::RandomRouteStragetyImpl<T> >();
                break;
            case Strategy::POLLING:
                return std::make_shared<impl::PollingRouteStrategyImpl<T> >();
                break;
            case Strategy::HASHIP:
                return std::make_shared<impl::HashIpRouteStrategyImpl<T> >();
                break;
            default:
                return std::make_shared<impl::RandomRouteStragetyImpl<T> >();
        }
    }
};


}  // namespace acid::rpc

#endif