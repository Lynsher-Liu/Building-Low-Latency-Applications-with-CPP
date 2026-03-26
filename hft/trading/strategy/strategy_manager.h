/*
 * @Author: Lynsher xinyiliu@astri.org
 * @Date: 2026-03-13 19:12:48
 * @LastEditors: Lynsher xinyiliu@astri.org
 * @LastEditTime: 2026-03-26 16:33:34
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
        return {{ExchangeName::BINANCE, sym_}, {ExchangeName::OKX, sym_}, {ExchangeName::BYBIT, sym_}};
    }
private:
    SymbolName sym_;
};

// 策略管理器, Strategies 是一个可变参数模板，接受任意数量的类型参数
template<typename... Strategies>
class StrategyManager : public EventSubscriber
{
public:
    StrategyManager(EventBus& bus, const int& numaNode, Strategies&&... strategies)
        : EventSubscriber(bus, "StrategyManager", numaNode), strategies_(std::forward<Strategies>(strategies)...)
    {
        buildDispatchTable();
        thread_pool_ = common::ThreadPool::getInstance();
    }

private:
    //EventBus& bus_;
    std::tuple<Strategies...> strategies_; //保存所有传入的策略对象
    
    /**
     * 两级哈希表：外层以交易所为键
     * 内层以币对为键，值是一个 vector<size_t>，存放该（交易所，币对）组合感兴趣的所有策略在元组中的索引
     */
    std::unordered_map<ExchangeName, std::unordered_map<SymbolName, std::vector<size_t>>> dispatch_table_;
    std::shared_ptr<common::ThreadPool> thread_pool_;

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

    /**
     * 根据事件的交易所和币对查表，获得策略索引列表
     * 对每个索引，向线程池提交一个任务，该任务会调用 callStrategy(idx, pl)
     */
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
                    thread_pool_->commit([this, idx, pl]() {
                        callStrategy(idx, pl);
                    });
                }
            }
        }
    }
    
    constexpr void buildDispatchTable() 
    {
        /**
         * auto idx 的类型实际上是一个编译期常量包装器（std::integral_constant），\
         * 代表当前策略在元组中的索引（例如 0, 1, 2...）
         */
        forEachIndex([this](auto idx) {
            const auto& strat = std::get<idx>(strategies_); //在编译期根据索引从元组中获取对应策略的引用
            auto interests = strat.interests(); //调用策略的成员函数，返回它关注的 (Exchange, Symbol) 列表
            for (const auto& [exch, sym] : interests) {
                dispatch_table_[exch][sym].push_back(idx); //将该策略的索引 idx（可隐式转换为 size_t）加入到对应 (exch, sym) 的向量中
            }
        });
    }

    //为 strategies_ 元组中的每一个策略调用一次传入的 lambda
    template<typename F>
    void forEachIndex(F&& f) {
        // std::index_sequence_for<Strategies...> 生成一个编译期整数序列 0,1,...,N-1（N 为策略个数）
        forEachIndexImpl(std::forward<F>(f), std::index_sequence_for<Strategies...>{});
    }

    /**
     * std::index_sequence<I...> 接收编译期整数序列 0,1,...,N-1（N 为策略个数）
     * 
     * std::integral_constant<size_t, I> 是一个类型，其实例可以隐式转换为 size_t 值，as idx 就是当前策略在元组中的索引
     * 
     * 折叠表达式 (f(std::integral_constant<size_t, I>{}), ...); 会将括号中的表达式依次用逗号展开，相当于依次调用：
     * f(std::integral_constant<size_t, 0>{}), f(std::integral_constant<size_t, 1>{}), ...
     */
    template<typename F, size_t... I>
    void forEachIndexImpl(F&& f, std::index_sequence<I...>) {
        (f(std::integral_constant<size_t, I>{}), ...);
    }

    void callStrategy(size_t idx, const PriceLevel& pl) {
        callStrategyImpl(idx, pl, std::index_sequence_for<Strategies...>{}); // 再次生成整数序列 0...N-1
    }

    template<size_t... I>
    void callStrategyImpl(size_t idx, const PriceLevel& pl, std::index_sequence<I...>) {
        /**
         * 折叠表达式，依次比较索引并调用
         * 如果相等，就调用 std::get<I>(strategies_).process(pl)，并返回 void() 作为逗号表达式的结果；
         * 如果不相等，直接返回 void() 
         *  */
        ((idx == I ? (std::get<I>(strategies_).process(pl), void()) : void()), ...);
    }
};
}
