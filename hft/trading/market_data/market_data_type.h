/*
 * @Author: Lynsher xinyiliu@astri.org
 * @Date: 2025-12-01 13:52:35
 * @LastEditors: Lynsher xinyiliu@astri.org
 * @LastEditTime: 2026-02-09 15:36:35
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

enum ExchangeName : uint8_t {
    EXCHANGE_OKX = 0,
    EXCHANGE_BINANCE = 1,
    EXCHANGE_BYBIT = 2,
    EXCHANGE_DERIBIT = 3
};

enum SymbolName : uint8_t {
    BTC_USDT = 0,
    BTC_USDT_SWAP = 1
};

// okx only provide these message info for each price level
struct PriceLevel 
{
	ExchangeName exchange;
    SymbolName symbol;
	
    double price;
    double quantity;
	uint32_t order_count;
    TimeStamp last_update_time;

    static int id;
    
    PriceLevel() : exchange(ExchangeName::EXCHANGE_OKX), symbol(SymbolName::BTC_USDT), price(0.0), quantity(0.0), order_count(1), last_update_time(0) 
    {
        id++;
    }
    PriceLevel(ExchangeName e, SymbolName s, double p, double q, uint32_t c, TimeStamp ts = 0) 
        : exchange(e), symbol(s), price(p), quantity(q), order_count(c), last_update_time(ts) 
        {
            id++;
        }
    
    auto toString() const {
      std::stringstream ss;
      ss << "PriceLevel - "
         << "ID:" << id
         << " ["
         << " exchange:" << static_cast<int>(exchange)
         << " symbol:" << static_cast<int>(symbol)
         << " qty:" << qtyToString(quantity)
         << " price:" << priceToString(price)
         << " order_count:" << order_count
         << "]";
      return ss.str();
    }
};

int PriceLevel::id = 0;

// 成交记录
struct Trade 
{
	ExchangeName exchange;     
    SymbolName symbol;

	uint32_t count{1};          // 聚合的订单匹配数量
	Side side{Side::INVALID};         // 吃单方向
	double price{0.0};        	// 成交价格
	double quantity{0};           // 成交数量
	
	uint64_t seqId{123}; 		//785659571,     // 推送的序列号	
	int source{0};          // 订单来源, 0：普通订单, 1：流动性增强计划订单
	
	char trade_id[MAX_TRADE_ID_LEN]{"123"};		// "2491342311", // 聚合的多笔交易中最新一笔交易的成交ID
	TimeStamp timestamp{0};

    static int id;

	auto toString() const {
      std::stringstream ss;
      ss << "Trade - "
         << "ID:" << id
         << " ["
         << " exchange:" << static_cast<int>(exchange)
         << " symbol:" << static_cast<int>(symbol)
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
