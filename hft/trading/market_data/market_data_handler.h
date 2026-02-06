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
#include "concurrentqueue/concurrentqueue.h"

#include "market_data_type.h"
#include "../common/types.h"
#include "../common/time_utils.h"
#include "../common/mem_pool.h"

using namespace Common;

namespace Market 
{

class PriceLevelHandler
{
public:
    PriceLevelHandler() = default;

private:
    thread_local MemPool<PriceLevel> priceLevelPool{100}; // 每个线程维护一个本地内存池，预分配100个PriceLevel对象
    moodycamel::ConcurrentQueue<PriceLevel*> priceLevelQueue; // 线程安全的队列，用于存储待处理的PriceLevel对象
};

}
