#pragma once

#include "common/macros.h"
#include "common/types.h"
#include "common/event_bus.h"

#include "position_keeper.h"
#include "om_order.h"

using namespace Common;

namespace Trading {
  class OrderManager;

  /// Enumeration that captures the result of a risk check - ALLOWED means it passed all risk checks, the other values represent the failure reason.
  enum class RiskCheckResult : int8_t {
    INVALID = 0,
    ORDER_TOO_LARGE = 1,
    POSITION_TOO_LARGE = 2,
    LOSS_TOO_LARGE = 3,
    ALLOWED = 4
  };

  inline auto riskCheckResultToString(RiskCheckResult result) {
    switch (result) {
      case RiskCheckResult::INVALID:
        return "INVALID";
      case RiskCheckResult::ORDER_TOO_LARGE:
        return "ORDER_TOO_LARGE";
      case RiskCheckResult::POSITION_TOO_LARGE:
        return "POSITION_TOO_LARGE";
      case RiskCheckResult::LOSS_TOO_LARGE:
        return "LOSS_TOO_LARGE";
      case RiskCheckResult::ALLOWED:
        return "ALLOWED";
    }

    return "";
  }

  /// Structure that represents the information needed for risk checks for a single trading instrument.
  struct RiskInfo {
    const PositionInfo *position_info_ = nullptr;

    RiskCfg risk_cfg_;

    /// Check risk to see if we are allowed to send an order of the specified quantity on the specified side.
    /// Will return a RiskCheckResult value to convey the output of the risk check.
    auto checkPreTradeRisk(Side side, Qty qty) const noexcept {
      // check order-size
      if (UNLIKELY(qty > risk_cfg_.max_order_size_))
        return RiskCheckResult::ORDER_TOO_LARGE;
      if (UNLIKELY(std::abs(position_info_->position_ + sideToValue(side) * static_cast<int32_t>(qty)) > static_cast<int32_t>(risk_cfg_.max_position_)))
        return RiskCheckResult::POSITION_TOO_LARGE;
      if (UNLIKELY(position_info_->total_pnl_ < risk_cfg_.max_loss_))
        return RiskCheckResult::LOSS_TOO_LARGE;

      return RiskCheckResult::ALLOWED;
    }

    auto toString() const {
      std::stringstream ss;
      ss << "RiskInfo" << "["
         << "pos:" << position_info_->toString() << " "
         << risk_cfg_.toString()
         << "]";

      return ss.str();
    }
  };

/// Hash map from TickerId -> RiskInfo.
typedef std::array<RiskInfo, ME_MAX_TICKERS> TickerRiskInfoHashMap;

/// Top level risk manager class to compute and check risk across all trading instruments.
class RiskManager// : public EventSubscriber
{
public:
    RiskManager(EventBus& bus, const int& numaNode, const PositionKeeperTickerId *position_keeper, const TradeEngineCfgHashMap &ticker_cfg)
        : position_keeper_(position_keeper)
    {
      for (TickerId i = 0; i < ticker_cfg.size(); ++i) {
        ticker_risk_.at(i).position_info_ = position_keeper_->getPositionInfo(i);
        ticker_risk_.at(i).risk_cfg_ = ticker_cfg.at(i).risk_cfg_;
      }
    }

    auto checkPreTradeRisk(TickerId ticker_id, Side side, Qty qty) const noexcept {
      return ticker_risk_.at(ticker_id).checkPreTradeRisk(side, qty);
    }

    /// Deleted default, copy & move constructors and assignment-operators.
    RiskManager() = delete;

    RiskManager(const RiskManager &) = delete;

    RiskManager(const RiskManager &&) = delete;

    RiskManager &operator=(const RiskManager &) = delete;

    RiskManager &operator=(const RiskManager &&) = delete;
  
protected:
    void handleEvent(const Event& event) override 
	{
        std::visit([this](const auto& e) {
            using T = std::decay_t<decltype(e)>;
      if constexpr (std::is_same_v<T, std::shared_ptr<const PriceLevel>>) {
        onPriceLevel(*e);
      } else if constexpr (std::is_same_v<T, std::shared_ptr<const Trade>>) {
        onTrade(*e);
            }
        }, event);
    }

  void onPriceLevel(const PriceLevel& /*price_level*/) const noexcept {
    // TODO: add per-symbol price movement checks if needed.
  }

  void onTrade(const Trade& /*trade*/) const noexcept {
    // TODO: add large-trade or toxic-flow checks if needed.
  }

  private:
    std::string time_str_;
    //Common::Logger *logger_ = nullptr;

    /// Hash map container from TickerId -> RiskInfo.
    TickerRiskInfoHashMap ticker_risk_;
    const PositionKeeperTickerId *position_keeper_ = nullptr;
  };
}
