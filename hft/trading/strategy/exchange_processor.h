/*
 * @Author: Lynsher xinyiliu@astri.org
 * @Date: 2025-12-01 13:52:35
 * @LastEditors: Lynsher xinyiliu@astri.org
 * @LastEditTime: 2026-03-27 18:14:06
 * @FilePath: /my_HFT/hft/trading/market_data/market_update_type.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#pragma once

#include <sstream>
#include <string>
#include <memory>
#include <atomic>
#include <chrono>
#include "concurrentqueue/concurrentqueue.h"

#include "market_order_book.h"
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
template<ExchangeName E>
class ExchangeProcessor
{
private:
    /// Hash map container from TickerId -> MarketOrderBook.
    //MarketOrderBookHashMap ticker_order_book_;

    // 使用 variant 存储不同币对的订单簿（因为每个 OrderBook 类型不同）
    using OrderBookVariant = std::variant<
        OrderBook<E, SymbolName::BTC_USDT>,
        OrderBook<E, SymbolName::BTC_USDT_SWAP>
    >;
    
    // 币对 -> 订单簿 variant 的映射
    std::unordered_map<SymbolName, OrderBookVariant> orderbooks_;

    const int& bindToNumaNode;
    affinity::PmrMemoryNumaAllocator numa_allocator; // NUMA-aware allocator for memory pools, allocating for shared_ptrs

    std::atomic_bool m_stop{false};
    std::thread* m_worker_thread;
    EventBus& bus_;

    moodycamel::ConcurrentQueue<shared_ptr<const PriceLevel>> priceLevelQueue; // 线程安全的队列，用于存储待处理的PriceLevel对象
    moodycamel::ConcurrentQueue<shared_ptr<const Trade>> tradeQueue; // 线程安全的队列，用于存储待处理的Trade对象

    static constexpr size_t DEPTH = (E == ExchangeName::OKX) ? 5 : 50;
    static constexpr double TICK_SIZE = (E == ExchangeName::OKX) ? 0.01 : 0.1;

public:
    ExchangeProcessor(EventBus& bus, const int& numaNode) : bus_(bus), bindToNumaNode(numaNode), numa_allocator(numaNode)
    {
        // 初始化ticker_order_book_，为每个symbol创建一个MarketOrderBook实例
        // for (size_t i = 0; i < ME_MAX_TICKERS; ++i) {
        //     ticker_order_book_[i] = new MarketOrderBook();
        // }

        orderbooks_ = {
            {SymbolName::BTC_USDT, OrderBook<E, SymbolName::BTC_USDT>{}},
            {SymbolName::BTC_USDT_SWAP, OrderBook<E, SymbolName::BTC_USDT_SWAP>{}}
        };
    }

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

    void getOrderbook(SymbolName symbol) 
    {
        auto it = orderbooks_.find(symbol);
        if (it != orderbooks_.end()) {
            std::visit([](auto& book) {
                // 这里可以调用订单簿的接口，例如获取BBO等
                double best_bid = book.bestBid();
                double best_ask = book.bestAsk();
                ASN_INFO(loggerH, "Best Bid: " + std::to_string(best_bid) + ", Best Ask: " + std::to_string(best_ask));
            }, it->second);
        } else {
            ASN_ERROR(loggerH, "Unknown symbol: " + Common::symbolToString(symbol));
        }
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
        std::this_thread::sleep_for(100ms);

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
                ASN_INFO(loggerH, "Processing PriceLevel: " + pl->toString()); //pl->toString()

                if constexpr (E == ExchangeName::OKX) {
                    //parseBinanceMessage(raw_msg);
                    
                } else if constexpr (E == ExchangeName::BINANCE) {
                    //parseOKXMessage(raw_msg);
                    
                }

                //ticker_order_book_[pl->symbol].onPricelevelUpdate(pl);
                auto it = orderbooks_.find(pl->symbol);
                if (it != orderbooks_.end()) {
                    // 根据币对调用对应的订单簿更新
                    std::visit([&](auto& book) {
                        book.onPricelevelUpdate(pl);
                    }, it->second);
                } else {
                    ASN_ERROR(loggerH, "Unknown symbol for exchange " << static_cast<int>(E));
                }

                bus_.publish(Event(pl)); // publish to event bus
            }

            // 处理Trade对象
            shared_ptr<const Trade> trade;
            while (tradeQueue.try_dequeue(trade)) {
                // 处理交易更新逻辑，例如记录成交信息等
                ASN_INFO(loggerH, "Processing Trade: " + trade->toString());

                auto it = orderbooks_.find(trade->symbol);
                if (it != orderbooks_.end()) {
                    std::visit([&](auto& book) {
                        book.onMarketUpdate(trade);
                    }, it->second);
                } else {
                    ASN_ERROR(loggerH, "Unknown symbol for exchange " << static_cast<int>(E));
                }

                bus_.publish(Event(trade)); // publish to event bus
            }

            // 可以添加适当的睡眠以避免忙等待，或者使用条件变量来优化等待机制
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }



private:
    
};


//template <typename... Processors>
class ExchangeManager
{
private:
    //std::tuple<Processors...> processors_;
    Trading::ExchangeProcessor<ExchangeName::OKX>& okx_processor_;
    Trading::ExchangeProcessor<ExchangeName::BINANCE>& binance_processor_;
    Trading::ExchangeProcessor<ExchangeName::BYBIT>& bybit_processor_;
    Trading::ExchangeProcessor<ExchangeName::DERIBIT>& deribit_processor_;

public:
    ExchangeManager(Trading::ExchangeProcessor<ExchangeName::OKX>& okx_processor,
                    Trading::ExchangeProcessor<ExchangeName::BINANCE>& binance_processor,
                    Trading::ExchangeProcessor<ExchangeName::BYBIT>& bybit_processor,
                    Trading::ExchangeProcessor<ExchangeName::DERIBIT>& deribit_processor) : 
        okx_processor_(okx_processor),
        binance_processor_(binance_processor),
        bybit_processor_(bybit_processor),
        deribit_processor_(deribit_processor)
    {
        // 根据需要创建不同交易所的处理器实例
        // okx_processor_ = std::make_unique<ExchangeProcessor<ExchangeName::OKX>>(bus_, numaNode_);
        // binance_processor_ = std::make_unique<ExchangeProcessor<ExchangeName::BINANCE>>(bus_, numaNode_);
    }

    using ProcessorVariant = std::variant<Trading::ExchangeProcessor<ExchangeName::OKX>*,
                                      Trading::ExchangeProcessor<ExchangeName::BINANCE>*,
                                      Trading::ExchangeProcessor<ExchangeName::BYBIT>*,
                                      Trading::ExchangeProcessor<ExchangeName::DERIBIT>*>;

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

}
