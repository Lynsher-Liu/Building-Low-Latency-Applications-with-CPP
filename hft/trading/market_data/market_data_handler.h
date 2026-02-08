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

#include "market_data_type.h"
#include "../common/types.h"
#include "../common/time_utils.h"
#include "../common/mem_pool.h"
#include "../common/singleton.h"
#include "../common/thread_utils.h"
#include "../common/AsnLog.h"

using namespace Common;

static AsnLoggerPtr loggerH = ASN_GETLOGGER("websocketHandler_h");

namespace Market 
{

class PriceLevelHandler : Singleton<PriceLevelHandler>
{
public:
    friend class Singleton<PriceLevelHandler>;

private:
    static thread_local MemPool<PriceLevel> priceLevelPool; // 每个线程维护一个本地内存池
    moodycamel::ConcurrentQueue<PriceLevel*> priceLevelQueue; // 线程安全的队列，用于存储待处理的PriceLevel对象
    
    std::atomic_bool m_stop;
    std::thread* m_worker_thread;

    int bindToNumaNode{0}; //TODO: check binding which numa node


public:
    auto start() 
    {
		m_stop.store(false);
		m_worker_thread = Common::createAndStartThread(bindToNumaNode, "Trading/websocketHandler", [this]() { run(); });
		if (!m_worker_thread)
			ASN_ERROR(loggerH, "Failed to start websocket thread");
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

	void run();

private:
    PriceLevelHandler() = default;

};

}
