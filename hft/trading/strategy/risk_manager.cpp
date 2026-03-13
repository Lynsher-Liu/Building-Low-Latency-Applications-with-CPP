/*
 * @Author: Lynsher xinyiliu@astri.org
 * @Date: 2025-12-01 13:52:35
 * @LastEditors: Lynsher xinyiliu@astri.org
 * @LastEditTime: 2026-03-13 19:09:17
 * @FilePath: /my_HFT/hft/trading/strategy/risk_manager.cpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include "risk_manager.h"

#include "order_manager.h"

namespace Trading {
#if 0
  RiskManager::RiskManager(Common::Logger *logger, const PositionKeeper *position_keeper, const TradeEngineCfgHashMap &ticker_cfg)
      : logger_(logger) {
    for (TickerId i = 0; i < ME_MAX_TICKERS; ++i) {
      ticker_risk_.at(i).position_info_ = position_keeper->getPositionInfo(i);
      ticker_risk_.at(i).risk_cfg_ = ticker_cfg[i].risk_cfg_;
    }
  }
#endif
}
