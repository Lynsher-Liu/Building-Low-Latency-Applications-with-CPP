/*
 * @Author: Lynsher xinyiliu@astri.org
 * @Date: 2025-12-01 13:52:35
 * @LastEditors: Lynsher xinyiliu@astri.org
 * @LastEditTime: 2026-03-24 16:14:48
 * @FilePath: /my_HFT/hft/trading/strategy/liquidity_taker.cpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include "liquidity_taker.h"

#include "trade_engine.h"

namespace Trading {
  LiquidityTaker::LiquidityTaker(TradeEngine *trade_engine, const FeatureEngine *feature_engine,
                                 OrderManager *order_manager,
                                 const TradeEngineCfgHashMap &ticker_cfg)
      : feature_engine_(feature_engine), order_manager_(order_manager), //logger_(logger),
        ticker_cfg_(ticker_cfg) {
    trade_engine->algoOnOrderBookUpdate_ = [this](auto ticker_id, auto price, auto side, auto book) {
      onOrderBookUpdate(ticker_id, price, side, book);
    };
    trade_engine->algoOnTradeUpdate_ = [this](auto market_update, auto book) { onTradeUpdate(market_update, book); };
    trade_engine->algoOnOrderUpdate_ = [this](auto client_response) { onOrderUpdate(client_response); };
  }
}
