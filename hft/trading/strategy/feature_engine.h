/*
 * @Author: Lynsher xinyiliu@astri.org
 * @Date: 2025-12-01 13:52:35
 * @LastEditors: Lynsher xinyiliu@astri.org
 * @LastEditTime: 2026-05-19 00:00:00
 * @FilePath: /my_HFT/hft/trading/strategy/feature_engine.h
 * @Description: Single-writer, multi-reader feature snapshots for the trading hot path.
 */
#pragma once

#include <array>
#include <atomic>
#include <cmath>
#include <deque>
#include <limits>

#include "common/macros.h"
#include "common/types.h"
#include "common/timer.h"
#include "market_order_book.h"

namespace Common { class EventBus; }

using namespace Common;

namespace Trading
{
static constexpr timer::TimeStamp WINDOW_NS = timer::NANOS_TO_SECS;
static constexpr double LARGE_TRADE_THRESHOLD = 10.0;

class FeatureSnapshot
{
public:
	FeatureSnapshot() = default;

	ExchangeName exchange;
	SymbolName symbol;

	// update from BBO
	double mid_price = Feature_INVALID;
	double market_price = Feature_INVALID;
	double spread = Feature_INVALID;

	// update from trades in rolling window
	timer::TimeStamp window_size = WINDOW_NS;
	double window_vwap = Feature_INVALID;
	//double order_flow_imbalance = 0.0;
	double order_flow_imbalance_ratio = 0.0; 
	double window_volume_ = 0.0;
	uint64_t window_trade_count_ = 0;

	// update from last aggressive trade
	double agg_trade_qty_ratio = 0.0;
	double large_trade_signal = 0.0;

	// last update timestamp (from either BBO or trade)
	timer::TimeStamp ts = 0;	
};

class FeatureInfo
{
public:
	struct TradeWindowEntry
	{
		timer::TimeStamp ts = 0;
		double signed_qty = 0.0;
		double abs_qty = 0.0;
		double total = 0.0;
	};

	const ExchangeName exchange_;
	const SymbolName symbol_;

private:
	// key features for strategy
	double mid_price_ = Feature_INVALID;
	double market_price_ = Feature_INVALID;
	double spread_ = Feature_INVALID;

	/**
	 * vwap = sum(price * abs_qty) / sum(abs_qty) in window
	 * 它不是 mid price，也不是 best bid/ask，而是真实成交价的量加权平均
	 */
	double window_vwap_ = Feature_INVALID; 
	double window_volume_ = 0.0;
	uint64_t window_trade_count_ = 0;

	/**
	 * ofi = buy_qty - sell_qty in window, it reflects the net buying/selling pressure
	 * ofi_ratio = (buy_qty - sell_qty) / (buy_qty + sell_qty);
	 */
	double order_flow_imbalance_ = 0.0;
	double order_flow_imbalance_ratio_ = 0.0;
	double agg_trade_qty_ratio_ = 0.0;
	double large_trade_signal_ = 0.0;
	timer::TimeStamp ts_ = 0;

	// helper variables for rolling window calculation
	std::deque<TradeWindowEntry> trade_window_;
	double rolling_buy_qty_ = 0.0;
	double rolling_sell_qty_ = 0.0;
	double rolling_ofi_ = 0.0;
	double rolling_total_ = 0.0;
	double rolling_volume_ = 0.0;

	// for snapshot single-writer multi-reader seqlock
	mutable std::atomic<uint64_t> seq_{0};
	FeatureSnapshot snapshot_;

	const BBO* bbo_ = nullptr;

public:
	FeatureInfo() 
	{
		snapshot_.exchange = exchange_;
		snapshot_.symbol = symbol_;
	}

	auto updateFromBBO(const BBO* bbo, timer::TimeStamp ts) noexcept -> void
	{
		if (UNLIKELY(!bbo || bbo->bid_price_ == Price_INVALID || bbo->ask_price_ == Price_INVALID ||
					 bbo->bid_qty_ == Qty_INVALID || bbo->ask_qty_ == Qty_INVALID ||
					 bbo->bid_qty_ + bbo->ask_qty_ == 0)) {
			return;
		}

		const auto bid_price = static_cast<double>(bbo->bid_price_);
		const auto ask_price = static_cast<double>(bbo->ask_price_);
		const auto bid_qty = static_cast<double>(bbo->bid_qty_);
		const auto ask_qty = static_cast<double>(bbo->ask_qty_);

		mid_price_ = (bid_price + ask_price) * 0.5;
		market_price_ = (bid_price * ask_qty + ask_price * bid_qty) / (bid_qty + ask_qty);
		spread_ = ask_price - bid_price;
		ts_ = ts;

		bbo_ = bbo;

		writeSnapshot();
	}

	auto updateFromTrade(const Trade& trade) noexcept -> void
	{
		const auto now = trade.timestamp ? trade.timestamp : timer::getCurNanoTime();
		const auto signed_qty = (trade.side == Side::BUY) ? trade.quantity : -trade.quantity;
		const TradeWindowEntry entry{now, signed_qty, trade.quantity, trade.price * trade.quantity};

		trade_window_.push_back(entry);
		addEntry(entry);

		while (!trade_window_.empty() && now - trade_window_.front().ts > WINDOW_NS) {
			removeEntry(trade_window_.front());
			trade_window_.pop_front();
		}

		// update window features
		window_vwap_ = rolling_volume_ > 0.0 ? rolling_total_ / rolling_volume_ : Feature_INVALID;
		order_flow_imbalance_ = rolling_ofi_;
		order_flow_imbalance_ratio_ = rolling_volume_ > 0.0 ? rolling_ofi_ / rolling_volume_ : 0.0;
		window_volume_ = rolling_volume_;
		window_trade_count_ = static_cast<uint64_t>(trade_window_.size());
		
		// formular: aggr_trade_qty_ratio = last_aggressive_trade_qty / top_of_book_qty
		if (bbo_) 
		{
			if (trade.side == Side::BUY && bbo_->ask_qty_ != Qty_INVALID && bbo_->ask_qty_ > 0) {
				agg_trade_qty_ratio_ = trade.quantity / static_cast<double>(bbo_->ask_qty_);
			} else if (trade.side == Side::SELL && bbo_->bid_qty_ != Qty_INVALID && bbo_->bid_qty_ > 0) {
				agg_trade_qty_ratio_ = trade.quantity / static_cast<double>(bbo_->bid_qty_);
			} else {
				agg_trade_qty_ratio_ = 0.0;
			}
		} else {
			agg_trade_qty_ratio_ = 0.0;
		}
		large_trade_signal_ = std::abs(trade.quantity) >= LARGE_TRADE_THRESHOLD
								? (signed_qty > 0.0 ? 1.0 : -1.0)
								: 0.0;
		ts_ = now;

		writeSnapshot();
	}

	auto readSnapshot() const noexcept -> FeatureSnapshot
	{
		FeatureSnapshot out;

		for (;;) {
			const auto before = seq_.load(std::memory_order_acquire);
			if (before & 1U) {
				continue;
			}

			out = snapshot_;

			const auto after = seq_.load(std::memory_order_acquire);
			if (LIKELY(before == after && !(after & 1U))) {
				return out;
			}
		}
	}

	inline auto toString() const -> std::string
	{
		std::stringstream ss;
		ss << "FeatureInfo{"
		   << " mid_price:" << priceToString(mid_price_)
		   << " market_price:" << priceToString(market_price_)
		   << " spread:" << priceToString(spread_)
		   << " \n---info in trade window: " << WINDOW_NS << " ns---\n"
		   << " window_vwap:" << priceToString(window_vwap_)
		   << " order_flow_imbalance:" << order_flow_imbalance_
		   << " agg_trade_qty_ratio:" << agg_trade_qty_ratio_
		   << " large_trade_signal:" << large_trade_signal_
		   << " ts:" << ts_
		   << "}";

		return ss.str();
	}

private:
	void writeSnapshot() noexcept
	{
		seq_.fetch_add(1, std::memory_order_acq_rel); // odd = writer active

		snapshot_.mid_price = mid_price_;
		snapshot_.market_price = market_price_;
		snapshot_.spread = spread_;
		snapshot_.window_vwap = window_vwap_;
		//snapshot_.order_flow_imbalance = order_flow_imbalance_;
		snapshot_.window_volume_ = window_volume_;
		snapshot_.window_trade_count_ = window_trade_count_;
		snapshot_.order_flow_imbalance_ratio = order_flow_imbalance_ratio_;

		snapshot_.agg_trade_qty_ratio = agg_trade_qty_ratio_;
		snapshot_.large_trade_signal = large_trade_signal_;
		snapshot_.ts = ts_;

		seq_.fetch_add(1, std::memory_order_release); // even = stable
	}

	auto addEntry(const TradeWindowEntry& entry) noexcept -> void
	{
		rolling_ofi_ += entry.signed_qty;
		rolling_volume_ += entry.abs_qty;
		rolling_total_ += entry.total;

		if (entry.signed_qty > 0.0) {
			rolling_buy_qty_ += entry.signed_qty;
		} else {
			rolling_sell_qty_ -= entry.signed_qty;
		}
	}

	auto removeEntry(const TradeWindowEntry& entry) noexcept -> void
	{
		rolling_ofi_ -= entry.signed_qty;
		rolling_volume_ -= entry.abs_qty;
		rolling_total_ -= entry.total;

		if (entry.signed_qty > 0.0) {
			rolling_buy_qty_ -= entry.signed_qty;
		} else {
			rolling_sell_qty_ += entry.signed_qty;
		}
	}
};

class FeatureEngine
{
private:
	ExchangeName exchange_ = ExchangeName::OKX;
	std::array<SymbolName, ME_MAX_TICKERS> symbols_{};
	std::array<FeatureInfo, ME_MAX_TICKERS> features_{};

public:
	FeatureEngine()
	{
		for (size_t i = 0; i < features_.size(); ++i) {
			features_[i].exchange_ = exchange_;
			features_[i].symbol_ = symbols_[i];
		}
	}

	auto updateFromBBO(SymbolName symbol, const BBO* bbo,
					   timer::TimeStamp ts = timer::getCurNanoTime()) noexcept -> void
	{
		getFeatureInfo(symbol).updateFromBBO(bbo, ts);
	}

	auto updateFromTrade(const Trade& trade) noexcept -> void
	{
		getFeatureInfo(trade.symbol).updateFromTrade(trade);
	}

	auto readFeatureInfoSnapshot(SymbolName symbol) noexcept -> FeatureSnapshot
	{
		return getFeatureInfo(symbol).readSnapshot();
	}

	FeatureEngine(const FeatureEngine&) = delete;
	FeatureEngine(const FeatureEngine&&) = delete;
	FeatureEngine& operator=(const FeatureEngine&) = delete;
	FeatureEngine& operator=(const FeatureEngine&&) = delete;

	inline auto toString() const -> std::string
	{
		std::stringstream ss;
		for (size_t i = 0; i < features_.size(); ++i) {
			ss << "Exchange:" << exchangeToString(exchange_) << ", Symbol:" << symbolToString(symbols_[i])
				<< " " << features_[i].toString() << "\n";
		}
		return ss.str();
	}
private:
	auto getFeatureInfo(SymbolName symbol) noexcept -> FeatureInfo&
	{
		return features_[static_cast<size_t>(symbol)];
	}

	// auto getFeatureInfo(SymbolName symbol) const noexcept -> const FeatureInfo&
	// {
	// 	return features_[static_cast<size_t>(symbol)];
	// }
};
}
