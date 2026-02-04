/*
 * @Author: Lynsher xinyiliu@astri.org
 * @Date: 2025-12-01 13:52:35
 * @LastEditors: Lynsher xinyiliu@astri.org
 * @LastEditTime: 2026-02-04 15:38:12
 * @FilePath: /my_HFT/hft/trading/market_data/market_update_type.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#pragma once

#include <sstream>
#include <string>

#include "../common/types.h"
#include "../common/time_utils.h"

using namespace Common;

namespace Market 
{

// okx only provide these message info for each price level
struct PriceLevel 
{
    double price;
    double quantity;
	uint32_t order_count;
    TimeStamp last_update_time;
    
    PriceLevel() : price(0.0), quantity(0.0), order_count(1), last_update_time(0) {}
    PriceLevel(double p, double q, uint32_t c, TimeStamp ts = 0) 
        : price(p), quantity(q), order_count(c), last_update_time(ts) {}
};

// 成交记录
struct Trade 
{
	uint32_t count{1};          // 聚合的订单匹配数量
	std::string inst_id{"BTC-USDT-SWAP"}; 	//BTC-USDT-SWAP
	double price{0.0};        	// 成交价格
	uint64_t seqId{123}; 		//785659571,     // 推送的序列号
	Side side{Side::INVALID};         // 吃单方向
	int source{0};          // 订单来源, 0：普通订单, 1：流动性增强计划订单
	double quantity{0};           // 成交数量
	std::string trade_id{"123"};		// "2491342311", // 聚合的多笔交易中最新一笔交易的成交ID
	TimeStamp timestamp{0};

	auto toString() const {
      std::stringstream ss;
      ss << "Trade"
         << " ["
         << " inst_id:" << inst_id
         << " match count:" << std::to_string(count)
         << " side:" << sideToString(side)
         << " qty:" << qtyToString(quantity)
         << " price:" << priceToString(price)
         << " trade_id:" << trade_id
         << "]";
      return ss.str();
    }
};


  /// Lock free queues of matching engine market update messages and market data publisher market updates messages respectively.
  //typedef Common::LFQueue<Exchange::MEMarketUpdate> MEMarketUpdateLFQueue;
  //typedef Common::LFQueue<Exchange::MDPMarketUpdate> MDPMarketUpdateLFQueue;
}
