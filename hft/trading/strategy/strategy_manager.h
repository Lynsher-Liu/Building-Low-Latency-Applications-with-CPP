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
#include "common/types.h"
#include "common/event_bus.h"
#include "common/thread_pool.h"

#include "position_keeper.h"
#include "om_order.h"

using namespace Common;

namespace Trading 
{
// 策略基类（CRTP）
template<typename Derived>
class Strategy {
public:
    void process(const PriceLevel& pl) {
        static_cast<Derived*>(this)->handle(pl);
    }
    // 子类必须提供 interests() 方法返回关注的（交易所，币对）列表
};

// 示例策略1：单交易所单币对做市
class SimpleMM : public Strategy<SimpleMM> {
public:
    SimpleMM(ExchangeName e, SymbolName s) : exch_(e), sym_(s) {}
    void handle(const PriceLevel& pl) {
        if (pl.exchange == exch_ && pl.symbol == sym_) {
            std::cout << "SimpleMM: " << static_cast<int>(exch_) 
                      << " " << static_cast<int>(sym_) << " price=" << pl.price << std::endl;
        }
    }
    std::vector<std::pair<ExchangeName, SymbolName>> interests() const {
        return {{exch_, sym_}};
    }
private:
    ExchangeName exch_;
    SymbolName sym_;
};

// 示例策略2：跨交易所套利（单币对）
class CrossExArb : public Strategy<CrossExArb> {
public:
    CrossExArb(SymbolName s) : sym_(s) {}
    void handle(const PriceLevel& pl) {
        if (pl.symbol == sym_) {
            // 注意：此策略有内部缓存，在多线程环境下需要同步
            // 为简化，假设这里只打印，不涉及缓存
            std::cout << "CrossExArb: " << static_cast<int>(sym_) 
                      << " from " << static_cast<int>(pl.exchange) << " price=" << pl.price << std::endl;
        }
    }
    std::vector<std::pair<ExchangeName, SymbolName>> interests() const {
        // 关注所有交易所的该币对
        return {{ExchangeName::EXCHANGE_BINANCE, sym_}, {ExchangeName::EXCHANGE_OKX, sym_}, {ExchangeName::EXCHANGE_BYBIT, sym_}};
    }
private:
    SymbolName sym_;
};

// 策略管理器
template<typename... Strategies>
class StrategyManager : public EventSubscriber
{
public:
    StrategyManager(EventBus& bus, Strategies&&... strategies)
        : EventSubscriber(bus, "StrategyManager"), strategies_(std::forward<Strategies>(strategies)...)
    {
        buildDispatchTable();
    }

private:
    EventBus& bus_;
    std::tuple<Strategies...> strategies_;
    // 分发表：交易所 -> 币对 -> 策略索引列表
    std::unordered_map<ExchangeName, std::unordered_map<SymbolName, std::vector<size_t>>> dispatch_table_;
    common::ThreadPool& thread_pool_ = common::ThreadPool::instance();

    void handleEvent(const Event& event) override 
    {
        std::visit([this](const auto& e) 
        {
            using T = std::decay_t<decltype(e)>;
            if constexpr (std::is_same_v<T, PriceLevel>) {
                onPriceLevel(e);
            } else if constexpr (std::is_same_v<T, Trade>) {
                //onTrade(e);
            }
        }, event);
    }

    // 处理事件：提交到线程池
    void onPriceLevel(const PriceLevel& pl) 
    {
        auto it = dispatch_table_.find(pl.exchange);
        if (it != dispatch_table_.end()) 
        {
            auto it2 = it->second.find(pl.symbol);
            if (it2 != it->second.end()) 
            {
                const auto& indices = it2->second;
                for (size_t idx : indices) 
                {
                    // 每个任务拷贝事件，独立处理
                    thread_pool_.enqueue([this, idx, pl]() {
                        callStrategy(idx, pl);
                    });
                }
            }
        }
    }
    
    void buildDispatchTable() 
    {
        forEachIndex([this](auto idx) {
            const auto& strat = std::get<idx>(strategies_);
            auto interests = strat.interests();
            for (const auto& [exch, sym] : interests) {
                dispatch_table_[exch][sym].push_back(idx);
            }
        });
    }

    template<typename F, size_t... I>
    void forEachIndexImpl(F&& f, std::index_sequence<I...>) {
        (f(std::integral_constant<size_t, I>{}), ...);
    }
    template<typename F>
    void forEachIndex(F&& f) {
        forEachIndexImpl(std::forward<F>(f), std::index_sequence_for<Strategies...>{});
    }

    void callStrategy(size_t idx, const PriceLevel& pl) {
        callStrategyImpl(idx, pl, std::index_sequence_for<Strategies...>{});
    }

    template<size_t... I>
    void callStrategyImpl(size_t idx, const PriceLevel& pl, std::index_sequence<I...>) {
        // 折叠表达式，依次比较索引并调用
        ((idx == I ? (std::get<I>(strategies_).process(pl), void()) : void()), ...);
    }
};
}
