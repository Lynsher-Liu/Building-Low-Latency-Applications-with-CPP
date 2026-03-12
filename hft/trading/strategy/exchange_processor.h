/*
 * @Author: Lynsher xinyiliu@astri.org
 * @Date: 2025-12-01 13:52:35
 * @LastEditors: Lynsher xinyiliu@astri.org
 * @LastEditTime: 2026-02-06 19:02:09
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

#include "../market_data/market_data_type.h"
#include "../common/types.h"
#include "../common/time_utils.h"
#include "../common/mem_pool.h"
#include "../common/singleton.h"
#include "../common/thread_utils.h"
#include "../common/AsnLog.h"
#include "../common/affinity.h"

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
    static thread_local MemPool<PriceLevel> priceLevelPool; // 每个线程维护一个本地内存池
    moodycamel::ConcurrentQueue<PriceLevel*> priceLevelQueue; // 线程安全的队列，用于存储待处理的PriceLevel对象
    
    static thread_local MemPool<Trade> tradePool; // 每个线程维护一个本地内存池
    moodycamel::ConcurrentQueue<Trade*> tradeQueue; // 线程安全的队列，用于存储待处理的Trade对象

    std::atomic_bool m_stop;
    std::thread* m_worker_thread;

    int bindToNumaNode{0}; //TODO: check binding which numa node
    affinity::PmrMemoryNumaAllocator allocator{bindToNumaNode}; // NUMA-aware allocator for memory pools, allocating for shared_ptrs

    /// Hash map container from TickerId -> MarketOrderBook.
    MarketOrderBookHashMap ticker_order_book_;

public:
    ExchangeProcessor() = default;
    ~ExchangeProcessor()
    {
        stop();
    }

    template<typename... Args>
    void writePriceLevelMsg2Queue(Args&&... args) { 
        // 从内存池分配一个新的PriceLevel对象，并将数据复制到该对象中
        PriceLevel* pl = priceLevelPool.allocate(std::forward<Args>(args)...);
        priceLevelQueue.enqueue(pl);
    }

    template<typename... Args>
    void writeTradeMsg2Queue(Args&&... args) { 
        // 从内存池分配一个新的Trade对象，并将数据复制到该对象中
        Trade* trade = tradePool.allocate(std::forward<Args>(args)...);
        tradeQueue.enqueue(trade);
    }


    auto start() -> void
    {
		m_stop.store(false);
		m_worker_thread = Common::createAndStartThread(bindToNumaNode, "Trading/ExchangeProcessor"+exchangeToString(E), [this]() { run(); });
		if (!m_worker_thread)
			ASN_ERROR(loggerH, "Failed to start ExchangeProcessor thread for exchange: " + exchangeToString(E));
    }

    auto stop() -> void 
	{
		m_stop.store(true);
		
		// 给一点时间让会话优雅关闭（可选）
        std::this_thread::sleep_for(100ms);

		//m_ioc.stop();

		if (m_worker_thread && m_worker_thread->joinable())
			m_worker_thread->join();
    }

	void run()
    {
        //TODO: check the logic by ai
        while (!m_stop.load()) {
            // 处理PriceLevel对象
            PriceLevel* pl;
            while (priceLevelQueue.try_dequeue(pl)) {
                // 处理价格更新逻辑，例如更新订单簿等
                ASN_INFO(loggerH, "Processing PriceLevel: " + pl->toString());
                
                // 处理完后将对象返回内存池
                priceLevelPool.deallocate(pl);
            }

            // 处理Trade对象
            Trade* trade;
            while (tradeQueue.try_dequeue(trade)) {
                // 处理交易更新逻辑，例如记录成交信息等
                ASN_INFO(loggerH, "Processing Trade: " + trade->toString());
                
                // 处理完后将对象返回内存池
                tradePool.deallocate(trade);
            }

            // 可以添加适当的睡眠以避免忙等待，或者使用条件变量来优化等待机制
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

private:
    

};

class ExchangeProcessorMap
{
private:
};

}
