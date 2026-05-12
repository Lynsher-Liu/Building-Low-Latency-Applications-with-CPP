#pragma once

#include "common/macros.h"
#include "common/types.h"

#include "exchange/order_server/client_response.h"

#include "market_order_book.h"

using namespace Common;

namespace Trading 
{
	/// PositionInfo tracks the position, pnl (realized and unrealized) and volume for a single trading instrument.
	struct PositionInfo 
	{
		int32_t position_ = 0;
		double real_pnl_ = 0, unreal_pnl_ = 0, total_pnl_ = 0;
		std::array<double, sideToIndex(Side::MAX) + 1> open_vwap_;
		std::array<double, sideToIndex(Side::MAX) + 1> vwap_;
		Qty volume_ = 0; //the total quantity that has been executed
		const BBO *bbo_ = nullptr;

		auto toString() const 
		{
			std::stringstream ss;
			ss << "Position{"
				<< "pos:" << position_
				<< " u-pnl:" << unreal_pnl_
				<< " r-pnl:" << real_pnl_
				<< " t-pnl:" << total_pnl_
				<< " vol:" << qtyToString(volume_)
				<< " vwaps:[" << (position_ ? open_vwap_.at(sideToIndex(Side::BUY)) / std::abs(position_) : 0)
				<< "X" << (position_ ? open_vwap_.at(sideToIndex(Side::SELL)) / std::abs(position_) : 0)
				<< "] "
				<< (bbo_ ? bbo_->toString() : "") << "}";

			return ss.str();
		}

		// Process an execution and update the position, pnl and volume.
		// client_response is self execution result
		auto addFill(const Exchange::MEClientResponse *client_response) noexcept 
		{
			const auto old_position = position_;
			const auto side_index = sideToIndex(client_response->side_);
			const auto opp_side_index = sideToIndex(client_response->side_ == Side::BUY ? Side::SELL : Side::BUY);
			const auto side_value = sideToValue(client_response->side_);
			position_ += client_response->exec_qty_ * side_value;
			volume_ += client_response->exec_qty_;

			if (old_position * sideToValue(client_response->side_) >= 0) // opened / increased position
			{ 
				open_vwap_[side_index] += (client_response->price_ * client_response->exec_qty_);
			} 
			else 
			{ 
				/**
				 * decreased position
				 * e.g, if old_position=10, and we sell 4, then we should realize pnl for 4, and update open_vwap for the remaining 6 position
				 * 		execution side_index=SELL, opp_side_index=BUY, 
				 * 		the operation on open_vwap is to reduce the BUY side VWAP from 10 to 6 position
				 */
				const auto opp_side_vwap = open_vwap_[opp_side_index] / std::abs(old_position); // 1000 / 10 = 100
				open_vwap_[opp_side_index] = opp_side_vwap * std::abs(position_); // 100 * 6 = 600

				real_pnl_ += std::min(static_cast<int32_t>(client_response->exec_qty_), std::abs(old_position)) // min(4, 10) = 4
							* (opp_side_vwap - client_response->price_) * sideToValue(client_response->side_); // (100 - sell_price) * -1 for sell, (100 - sell_price) * 1 for buy
				
				/**
				 * e.g, if old_position=10, and we sell 14
				 * before we set open_vwap_[BUY] to 100 * abs(-4) = 400, and real_pnl_ += 10 * (100 - sell_price)*(-1)
				 */
				if (position_ * old_position < 0) 
				{ 
					open_vwap_[side_index] = (client_response->price_ * std::abs(position_));
					open_vwap_[opp_side_index] = 0;
				}
			}

			if (!position_) // flattened position, reset VWAP and unrealized pnl
			{ 
				open_vwap_[sideToIndex(Side::BUY)] = open_vwap_[sideToIndex(Side::SELL)] = 0;
				unreal_pnl_ = 0;
			} 
			else 
			{
				if (position_ > 0)
					unreal_pnl_ = (client_response->price_ - open_vwap_[sideToIndex(Side::BUY)] / std::abs(position_)) *
									std::abs(position_);
				else
					unreal_pnl_ = (open_vwap_[sideToIndex(Side::SELL)] / std::abs(position_) - client_response->price_) *
									std::abs(position_);
			}

			total_pnl_ = unreal_pnl_ + real_pnl_;
		}

		/**
		 *  Process a change from top-of-book prices (BBO)
		 *  calculate mid_price and unrealized pnl
		 * */ 
		auto updatePnlByBBO(const BBO *bbo) noexcept 
		{ 
			std::string time_str;
			bbo_ = bbo;

			if (position_ && bbo->bid_price_ != Price_INVALID && bbo->ask_price_ != Price_INVALID) 
			{
				const auto mid_price = (bbo->bid_price_ + bbo->ask_price_) * 0.5;
				if (position_ > 0)
				unreal_pnl_ =
					(mid_price - open_vwap_[sideToIndex(Side::BUY)] / std::abs(position_)) *
					std::abs(position_);
				else
				unreal_pnl_ =
					(open_vwap_[sideToIndex(Side::SELL)] / std::abs(position_) - mid_price) *
					std::abs(position_);

				const auto old_total_pnl = total_pnl_;
				total_pnl_ = unreal_pnl_ + real_pnl_;

				if (total_pnl_ != old_total_pnl)
				// logger->log("%:% %() % % %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str),
				//             toString(), bbo_->toString());
				std::cout << timer::getCurNanoTime() << " " << toString() << " " << bbo_->toString() << "\n";
			}
		}
	};

  /// Top level position keeper class to compute position, pnl and volume for all trading instruments.
  class PositionKeeper {
  public:
    PositionKeeper() //Common::Logger *logger
        //: logger_(logger) 
        {}

    /// Deleted default, copy & move constructors and assignment-operators.
    //PositionKeeper() = delete;

    PositionKeeper(const PositionKeeper &) = delete;

    PositionKeeper(const PositionKeeper &&) = delete;

    PositionKeeper &operator=(const PositionKeeper &) = delete;

    PositionKeeper &operator=(const PositionKeeper &&) = delete;

  private:
    std::string time_str_;
    //Common::Logger *logger_ = nullptr;

    /// Hash map container from TickerId -> PositionInfo.
    std::array<PositionInfo, ME_MAX_TICKERS> ticker_position_;

  public:
    auto addFill(const Exchange::MEClientResponse *client_response) noexcept {
      ticker_position_.at(client_response->ticker_id_).addFill(client_response); //, logger_
    }

    auto updatePnlByBBO(TickerId ticker_id, const BBO *bbo) noexcept {
      ticker_position_.at(ticker_id).updatePnlByBBO(bbo); //, logger_
    }

    auto getPositionInfo(TickerId ticker_id) const noexcept {
      return &(ticker_position_.at(ticker_id));
    }

    auto toString() const {
      double total_pnl = 0;
      Qty total_vol = 0;

      std::stringstream ss;
      for(TickerId i = 0; i < ticker_position_.size(); ++i) {
        ss << "TickerId:" << tickerIdToString(i) << " " << ticker_position_.at(i).toString() << "\n";

        total_pnl += ticker_position_.at(i).total_pnl_;
        total_vol += ticker_position_.at(i).volume_;
      }
      ss << "Total PnL:" << total_pnl << " Vol:" << total_vol << "\n";

      return ss.str();
    }
  };
}
