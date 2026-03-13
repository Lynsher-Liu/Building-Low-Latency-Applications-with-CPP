/*
 * @Author: Lynsher xinyiliu@astri.org
 * @Date: 2026-03-13 19:12:48
 * @LastEditors: Lynsher xinyiliu@astri.org
 * @LastEditTime: 2026-03-13 19:15:39
 * @FilePath: /my_HFT/hft/trading/strategy/strategy_manager.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#pragma once

#include "common/macros.h"
#include "common/event_bus.h"

#include "position_keeper.h"
#include "om_order.h"

using namespace Common;

namespace Trading 
{

template<typename StrategyType>
class StrategyManager : public EventSubscriber
{ 
public:
    StrategyManager(EventBus& bus, std::unique_ptr<StrategyType> strategy)
        : EventSubscriber(bus, "StrategyManager"), strategy_(std::move(strategy)) {}

protected:
    void handleEvent(const Event& event) override {
        std::visit([this](const auto& e) {
            using T = std::decay_t<decltype(e)>;
            if constexpr (std::is_same_v<T, OrderBookUpdated>) {
                strategy_->onOrderBookUpdate(e);
            } else if constexpr (std::is_same_v<T, TradeOccurred>) {
                strategy_->onTrade(e);
            }
        }, event);
    }

private:
    std::unique_ptr<StrategyType> strategy_;
};

// 辅助函数：遍历tuple，为每个策略创建订阅者并注册到EventBus
template<typename Tuple, size_t... I>
auto register_strategies_impl(EventBus& bus, Tuple&& strategies, std::index_sequence<I...>) {
    // 返回一个包含所有订阅者的tuple
    return std::make_tuple(
        (new StrategySubscriber<std::decay_t<decltype(std::get<I>(strategies))>>(
            bus, std::make_unique<std::decay_t<decltype(std::get<I>(strategies))>>(
                std::move(std::get<I>(strategies))
            )
        )...
    );
}

template<typename... StrategyTypes>
auto register_strategies(EventBus& bus, std::tuple<StrategyTypes...> strategies) {
    return register_strategies_impl(bus, std::move(strategies),
                                    std::index_sequence_for<StrategyTypes...>{});
}
}
