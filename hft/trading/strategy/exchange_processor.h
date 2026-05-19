/*
 * @Author: Lynsher xinyiliu@astri.org
 * @Date: 2025-12-01 13:52:35
 * @LastEditors: Lynsher xinyiliu@astri.org
 * @LastEditTime: 2026-05-15 16:35:16
 * @FilePath: /my_HFT/hft/trading/market_data/market_update_type.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#pragma once

#include <array>
#include <sstream>
#include <string>
#include <memory>
#include <atomic>
#include <chrono>
#include <tuple>
#include <variant>

#include "concurrentqueue/concurrentqueue.h"

#include "market_order_book.h"
#include "position_keeper.h"
#include "feature_engine.h"
#include "../common/types.h"
#include "../common/event_bus.h"
#include "../common/mem_pool.h"
#include "../common/singleton.h"
#include "../common/thread_utils.h"
#include "../common/AsnLog.h"
#include "../common/affinity.h"
//#include "../common/struct_pack.hpp"

using namespace Common;

static AsnLoggerPtr loggerH = ASN_GETLOGGER("exchangeProcessor_h");

namespace Trading
{
/**
 * @brief This class maintains message queue/ memory pool/ working thread/ orderbook for one exchange
 * Act as the tradingEngine class in book's original design
 */
/**
 * @brief ExchangeProcessor is a template because OrderBook is a template. 
 * And OrderBook is a template because of OrderBookTraits. The template chain starts from OrderBookTraits.
 * 
 * 以 OKXProcessor 为例展开成员变量：
 * 
    // ExchangeProcessor<OKX, BTC_USDT, BTC_USDT_SWAP> 展开后:
    class OKXProcessor 
    {
        static constexpr size_t kNumSymbols = 2;
        static constexpr std::array<SymbolName, 2> kSymbols{SymbolName::BTC_USDT, SymbolName::BTC_USDT_SWAP};
        
        // OrderBooksTuple 展开
        std::tuple<
            OrderBook<ExchangeName::OKX, SymbolName::BTC_USDT>,
            OrderBook<ExchangeName::OKX, SymbolName::BTC_USDT_SWAP>
        > orderbooks_{};
        PositionKeeper<ExchangeName::OKX, SymbolName::BTC_USDT, SymbolName::BTC_USDT_SWAP> position_keeper_{};
        ...
    };
 */
template<ExchangeName E, SymbolName... Symbols>
class ExchangeProcessor
{
private:
    static constexpr size_t kNumSymbols = sizeof...(Symbols);
    static constexpr std::array<SymbolName, kNumSymbols> kSymbols{Symbols...};
    static constexpr size_t npos = static_cast<size_t>(-1);

    using OrderBooksTuple = std::tuple<OrderBook<E, Symbols>...>;

    // Each OrderBook<E, S> gets its own instantiation with compile-time constants (depth, tick_size) from OrderBookTraits<E, S>
    OrderBooksTuple orderbooks_{};
    PositionKeeper<E, Symbols...> position_keeper_{}; // tracks all symbols for this exchange
    FeatureEngine feature_engine_{}; // single-writer feature snapshots for this exchange processor

    const int& bindToNumaNode;
    affinity::PmrMemoryNumaAllocator numa_allocator; // NUMA-aware allocator for memory pools, allocating for shared_ptrs

    std::atomic_bool m_stop{false};
    std::thread* m_worker_thread;
    EventBus& bus_;

    moodycamel::ConcurrentQueue<shared_ptr<const PriceLevel>> priceLevelQueue; // 线程安全的队列，用于存储待处理的PriceLevel对象
    moodycamel::ConcurrentQueue<shared_ptr<const Trade>> tradeQueue; // 线程安全的队列，用于存储待处理的Trade对象

public:
    ExchangeProcessor(EventBus& bus, const int& numaNode) : bus_(bus), bindToNumaNode(numaNode), numa_allocator(numaNode) {}

    ~ExchangeProcessor()
    {
        stop();
    }

    template<typename... Args>
    void writePriceLevelMsg2Queue(Args&&... args) 
    { 
        shared_ptr<const PriceLevel> pl =numa_allocator.produceSharedPtr<PriceLevel>(std::forward<Args>(args)...); // 使用NUMA-aware allocator分配PriceLevel对象
        priceLevelQueue.enqueue(pl);
    }

    template<typename... Args>
    void writeTradeMsg2Queue(Args&&... args) 
    { 
        shared_ptr<const Trade> trade = numa_allocator.produceSharedPtr<Trade>(std::forward<Args>(args)...); // 使用NUMA-aware allocator分配Trade对象
        tradeQueue.enqueue(trade);
    }

    /**
     * 用于处理成交回包（MEClientResponse）中的 ticker_id，快速转换为 SymbolName
     * 假设 TickerId 与 Symbols 参数包的顺序一致（即 0 -> 第一个币对，1 -> 第二个……）

     * 直接通过数组索引访问，O(1) 且无分支预测失败（除边界检查）
     */
    static inline auto tryMapTickerIdToSymbol(TickerId ticker_id, SymbolName &out_symbol) noexcept -> bool
    {
        if (UNLIKELY(ticker_id >= kNumSymbols)) {
            return false;
        }
        out_symbol = kSymbols[ticker_id];
        return true;
    }
#if 0
    void getOrderbook(SymbolName symbol) 
    {
        withOrderBook(symbol, [&](auto &book) {
            const auto *bbo = book.getBBO();
            const auto best_bid = bbo ? bbo->bid_price_ : Price_INVALID;
            const auto best_ask = bbo ? bbo->ask_price_ : Price_INVALID;
            ASN_INFO(loggerH, "Best Bid: " + std::to_string(best_bid) + ", Best Ask: " + std::to_string(best_ask));
        });
    }
#endif

    auto start() -> void
    {
		m_stop.store(false);
		m_worker_thread = Common::createAndStartThread(bindToNumaNode, "Trading/ExchangeProcessor"+Common::exchangeToString(E), [this]() { run(); });
		if (!m_worker_thread)
			ASN_ERROR(loggerH, "Failed to start ExchangeProcessor thread for exchange: " + Common::exchangeToString(E));
    }

    auto stop() -> void 
	{
		m_stop.store(true);
		
		// 给一点时间让会话优雅关闭（可选）
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

		if (m_worker_thread && m_worker_thread->joinable())
			m_worker_thread->join();
    }

    /**
     * PriceLevel/Trade arrives with SymbolName
        → processOrderBook(symbol, lambda)
            → tryGetSymbolIndex(symbol) → size_t idx
                → symbolIndex(symbol) → linear scan over constexpr array
                    → visitOrderbook(idx, lambda)
                        → visitOrderbookImpl(idx, index_sequence, lambda)
                            → (fn(std::get<I>(orderbooks_)) for matching I)
                            
        → lambda receives OrderBook<E, SpecificSymbol>& — fully typed
     */
	void run()
    {
        while (!m_stop.load()) 
        {
            shared_ptr<const PriceLevel> pl;
            while (priceLevelQueue.try_dequeue(pl)) 
            {
                //auto buffer = struct_pack::serialize<std::string>(*(pl.get())); TODO: check using it
                ASN_INFO(loggerH, "Processing PriceLevel: " + pl->toString()); 

                processOrderBook(pl->symbol, [&](auto &book) 
                {
                    // update orderbook and BBO inside
                    book.onPricelevelUpdate(pl);

                    /**
                     * Core hot path:
                        OrderBook -> PositionKeeper -> FeatureEngine -> primary Strategy -> OrderManager/Risk
                        same thread, copied BBO value

                        Async side path:
                        EventBus -> logs / metrics / diagnostics / slow observers / secondary strategies
                    */

                    // update pnl using top-of-book snapshot
                    position_keeper_.updateFromBBO(pl->symbol, book.getBBO());
                    feature_engine_.updateFromBBO(pl->symbol, book.getBBO(), pl->last_update_time);
                });

                bus_.publish(Event(pl)); // publish to event bus
            }

            // 处理Trade对象
            shared_ptr<const Trade> trade;
            while (tradeQueue.try_dequeue(trade)) {
                // 处理交易更新逻辑，例如记录成交信息等
                ASN_INFO(loggerH, "Processing Trade: " + trade->toString());

                processOrderBook(trade->symbol, [&](auto &book) 
                {
                    book.onMarketUpdate(trade);
                    feature_engine_.updateFromTrade(*trade);
                });

                

                bus_.publish(Event(trade)); // publish to event bus
            }

            // 可以添加适当的睡眠以避免忙等待，或者使用条件变量来优化等待机制
            //std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

private:
    /**
     * Scans the compile-time constexpr std::array<SymbolName, kNumSymbols> kSymbols{Symbols...} at runtime 
     * Returns npos (i.e. (size_t)-1) if not found. 
     * 
     * This is a linear scan over a tiny array (currently 2 elements), 
     * making it effectively branch-predicted and cache-friendly. 
     * No unordered_map lookup needed
     * 
     * -  Fits in one cache line — std::array<SymbolName, 2> is just 2 bytes on the stack/static storage. No pointer chasing, no cache miss.
        - No hashing overhead — unordered_map must compute a hash, find the bucket, then walk a linked list (pointer dereferences into potentially cold memory).
        - No allocation / node overhead — each unordered_map entry is a heap-allocated node. Lookup involves at least 2 pointer dereferences (bucket + node), each a potential cache miss at ~100–200ns on L3/DRAM.
        - Branch predictor loves it — with 2 elements, the first check almost always hits, so the branch is strongly predicted.
     */
    static inline auto symbolIndex(SymbolName symbol) noexcept -> size_t
    {
        for (size_t i = 0; i < kSymbols.size(); ++i) {
            if (kSymbols[i] == symbol) {
                return i;
            }
        }
        return npos;
    }

    static inline auto tryGetSymbolIndex(SymbolName symbol, size_t &out_idx) noexcept -> bool
    {
        const auto idx = symbolIndex(symbol);
        if (UNLIKELY(idx == npos)) {
            return false;
        }
        out_idx = idx;
        return true;
    }

    template<typename Fn>
    void visitOrderbook(size_t idx, Fn &&fn) {
        visitOrderbookImpl(idx, std::index_sequence_for<OrderBooksTuple>{}, std::forward<Fn>(fn)); // 生成整数序列 0...N-1
    }

    template<size_t... I, typename Fn>
    void visitOrderbookImpl(size_t idx, std::index_sequence<I...>, Fn &&fn) {
        /**
         * 折叠表达式，依次比较索引并调用
         * 如果相等，就调用 fn(std::get<I>(orderbooks_))，并返回 void() 作为逗号表达式的结果；
         * 如果不相等，直接返回 void() 
         *  */
        ((idx == I ? (fn(std::get<I>(orderbooks_)), void()) : void()), ...);
    }

    /**
     * processOrderBook 先通过 tryGetSymbolIndex 获得索引 idx，然后调用 visitOrderbook，将 lambda 应用于idx对应的订单簿
     */
    template<typename Fn>
    inline auto processOrderBook(SymbolName symbol, Fn &&fn) -> void
    {
        size_t idx = 0;
        if (UNLIKELY(!tryGetSymbolIndex(symbol, idx))) {
            ASN_ERROR(loggerH, "Unknown symbol: " + Common::symbolToString(symbol) + " for exchange: " + Common::exchangeToString(E));
            return;
        }
        visitOrderbook(idx, std::forward<Fn>(fn));
    }
};


using OKXProcessor = Trading::ExchangeProcessor<ExchangeName::OKX, SymbolName::BTC_USDT, SymbolName::BTC_USDT_SWAP>;
using BinanceProcessor = Trading::ExchangeProcessor<ExchangeName::BINANCE, SymbolName::BTC_USDT, SymbolName::BTC_USDT_SWAP>;
using BybitProcessor = Trading::ExchangeProcessor<ExchangeName::BYBIT, SymbolName::BTC_USDT>;
using DeribitProcessor = Trading::ExchangeProcessor<ExchangeName::DERIBIT, SymbolName::BTC_USDT>;


class ExchangeManager
{
private:
    OKXProcessor& okx_processor_;
    BinanceProcessor& binance_processor_;
    BybitProcessor& bybit_processor_;
    DeribitProcessor& deribit_processor_;

public:
    ExchangeManager(OKXProcessor& okx_processor,
                    BinanceProcessor& binance_processor,
                    BybitProcessor& bybit_processor,
                    DeribitProcessor& deribit_processor) : 
        okx_processor_(okx_processor),
        binance_processor_(binance_processor),
        bybit_processor_(bybit_processor),
        deribit_processor_(deribit_processor)
    {
    
    }

    using ProcessorVariant = std::variant<OKXProcessor*, BinanceProcessor*, BybitProcessor*, DeribitProcessor*>;

    /**
     * Runtime dispatch based on ExchangeName (cannot use if constexpr with runtime value)
     * 是运行时 switch 分发，编译期不会展开成多条路径
     * 
     * 用 std::tuple 不能避免运行时分发，反而会让代码更复杂。std::variant + std::visit 是当前最合适的选择
     * std::tuple 存储的是不同类型的处理器（如 OKXProc、BinanceProc 等）
     * 要从 tuple 中取出某个特定类型的元素，你仍然需要在运行时根据 ExchangeName 决定使用哪个索引
     * */ 
    ProcessorVariant getProcessor(ExchangeName exchange) const noexcept {
        switch (exchange) {
            case ExchangeName::OKX:
                return &okx_processor_;
            case ExchangeName::BINANCE:
                return &binance_processor_;
            case ExchangeName::BYBIT:
                return &bybit_processor_;
            case ExchangeName::DERIBIT:
                return &deribit_processor_;
            default:
                throw std::runtime_error("Invalid exchange type for processor dispatch");
        }
    }
};

}
