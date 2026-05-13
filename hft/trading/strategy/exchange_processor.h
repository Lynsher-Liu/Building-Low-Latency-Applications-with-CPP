/*
 * @Author: Lynsher xinyiliu@astri.org
 * @Date: 2025-12-01 13:52:35
 * @LastEditors: Lynsher xinyiliu@astri.org
 * @LastEditTime: 2026-05-13 19:21:41
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
 * @brief ExchangeProcessor specialized on a compile-time symbol pack.
 *
 * - Stores one concrete OrderBook per symbol in a tuple (no unordered_map).
 * - Routes runtime SymbolName updates via a tiny symbol-pack index lookup.
 * - Owns one PositionKeeper<E, Symbols...> for all symbols in this exchange.
 */
template<ExchangeName E, SymbolName... Symbols>
class ExchangeProcessor
{
private:
    static constexpr size_t kNumSymbols = sizeof...(Symbols);
    static constexpr std::array<SymbolName, kNumSymbols> kSymbols{Symbols...};
    static constexpr size_t npos = static_cast<size_t>(-1);

    using OrderBooksTuple = std::tuple<OrderBook<E, Symbols>...>;

    OrderBooksTuple orderbooks_{};
    PositionKeeper<E, Symbols...> position_keeper_{}; // tracks all symbols for this exchange

    const int& bindToNumaNode;
    affinity::PmrMemoryNumaAllocator numa_allocator; // NUMA-aware allocator for memory pools, allocating for shared_ptrs

    std::atomic_bool m_stop{false};
    std::thread* m_worker_thread;
    EventBus& bus_;

    moodycamel::ConcurrentQueue<shared_ptr<const PriceLevel>> priceLevelQueue; // 线程安全的队列，用于存储待处理的PriceLevel对象
    moodycamel::ConcurrentQueue<shared_ptr<const Trade>> tradeQueue; // 线程安全的队列，用于存储待处理的Trade对象

    //static constexpr size_t DEPTH = (E == ExchangeName::OKX) ? 5 : 50;
    //static constexpr double TICK_SIZE = (E == ExchangeName::OKX) ? 0.01 : 0.1;

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

    static inline auto tryMapTickerIdToSymbol(TickerId ticker_id, SymbolName &out_symbol) noexcept -> bool
    {
        if (UNLIKELY(ticker_id >= kNumSymbols)) {
            return false;
        }
        out_symbol = kSymbols[ticker_id];
        return true;
    }

    void getOrderbook(SymbolName symbol) 
    {
        withOrderBook(symbol, [&](auto &book) {
            const auto *bbo = book.getBBO();
            const auto best_bid = bbo ? bbo->bid_price_ : Price_INVALID;
            const auto best_ask = bbo ? bbo->ask_price_ : Price_INVALID;
            ASN_INFO(loggerH, "Best Bid: " + std::to_string(best_bid) + ", Best Ask: " + std::to_string(best_ask));
        });
    }

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

	void run()
    {
        while (!m_stop.load()) 
        {
            // 处理PriceLevel对象
            shared_ptr<const PriceLevel> pl;
            while (priceLevelQueue.try_dequeue(pl)) {
                // 处理价格更新逻辑，例如更新订单簿等
                //auto buffer = struct_pack::serialize<std::string>(*(pl.get())); TODO: check using it
                ASN_INFO(loggerH, "Processing PriceLevel: " + pl->toString()); 

                withOrderBook(pl->symbol, [&](auto &book) {
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
                    position_keeper_.updatePnlByBBO(pl->symbol, book.getBBO());
                });

                bus_.publish(Event(pl)); // publish to event bus
            }

            // 处理Trade对象
            shared_ptr<const Trade> trade;
            while (tradeQueue.try_dequeue(trade)) {
                // 处理交易更新逻辑，例如记录成交信息等
                ASN_INFO(loggerH, "Processing Trade: " + trade->toString());

                withOrderBook(trade->symbol, [&](auto &book) {
                    book.onMarketUpdate(trade);
                    position_keeper_.updatePnlByBBO(trade->symbol, book.getBBO());
                });

                

                bus_.publish(Event(trade)); // publish to event bus
            }

            // 可以添加适当的睡眠以避免忙等待，或者使用条件变量来优化等待机制
            //std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }



private:
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

    template<typename Tuple, typename Fn, size_t I = 0>
    static inline auto tupleVisitByIndex(Tuple &tuple, size_t idx, Fn &&fn) -> void
    {
        if constexpr (I < std::tuple_size_v<Tuple>) {
            if (idx == I) {
                fn(std::get<I>(tuple));
                return;
            }
            tupleVisitByIndex<Tuple, Fn, I + 1>(tuple, idx, std::forward<Fn>(fn));
        }
    }

    template<typename Fn>
    inline auto withOrderBook(SymbolName symbol, Fn &&fn) -> void
    {
        size_t idx = 0;
        if (UNLIKELY(!tryGetSymbolIndex(symbol, idx))) {
            ASN_ERROR(loggerH, "Unknown symbol: " + Common::symbolToString(symbol) + " for exchange: " + Common::exchangeToString(E));
            return;
        }
        tupleVisitByIndex(orderbooks_, idx, std::forward<Fn>(fn));
    }
};


// ExchangeManager with a shared symbol-pack across all exchanges.
template<SymbolName... Symbols>
class ExchangeManagerT
{
private:
    using OKXProc = Trading::ExchangeProcessor<ExchangeName::OKX, Symbols...>;
    using BinanceProc = Trading::ExchangeProcessor<ExchangeName::BINANCE, Symbols...>;
    using BybitProc = Trading::ExchangeProcessor<ExchangeName::BYBIT, Symbols...>;
    using DeribitProc = Trading::ExchangeProcessor<ExchangeName::DERIBIT, Symbols...>;

    OKXProc& okx_processor_;
    BinanceProc& binance_processor_;
    BybitProc& bybit_processor_;
    DeribitProc& deribit_processor_;

public:
    ExchangeManagerT(OKXProc& okx_processor,
                    BinanceProc& binance_processor,
                    BybitProc& bybit_processor,
                    DeribitProc& deribit_processor) : 
        okx_processor_(okx_processor),
        binance_processor_(binance_processor),
        bybit_processor_(bybit_processor),
        deribit_processor_(deribit_processor)
    {
        // 根据需要创建不同交易所的处理器实例
        // okx_processor_ = std::make_unique<ExchangeProcessor<ExchangeName::OKX>>(bus_, numaNode_);
        // binance_processor_ = std::make_unique<ExchangeProcessor<ExchangeName::BINANCE>>(bus_, numaNode_);
    }

    using ProcessorVariant = std::variant<OKXProc*, BinanceProc*, BybitProc*, DeribitProc*>;

    // Runtime dispatch based on ExchangeName (cannot use if constexpr with runtime value)
    // Returns variant holding pointer to appropriate processor type
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

using ExchangeManager = ExchangeManagerT<SymbolName::BTC_USDT, SymbolName::BTC_USDT_SWAP>;

}
