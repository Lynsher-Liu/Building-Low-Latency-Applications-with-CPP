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
constexpr auto Feature_INVALID = std::numeric_limits<double>::quiet_NaN();

struct FeatureSnapshot
{
	ExchangeName exchange = ExchangeName::OKX;
	SymbolName symbol = SymbolName::BTC_USDT;

	double mid_price = Feature_INVALID;
	double market_price = Feature_INVALID;
	double spread = Feature_INVALID;

	double open_vwap_1s = Feature_INVALID;
	double order_flow_imbalance_1s = 0.0;
	double agg_trade_qty_ratio_1s = 0.0;
	double large_trade_signal = 0.0;

	timer::TimeStamp ts = 0;
};

class FeatureInfo
{
private:
	struct TradeWindowEntry
	{
		timer::TimeStamp ts = 0;
		double signed_qty = 0.0;
		double abs_qty = 0.0;
		double price_qty = 0.0;
	};

	ExchangeName exchange_ = ExchangeName::OKX;
	SymbolName symbol_ = SymbolName::BTC_USDT;

	double mid_price_ = Feature_INVALID;
	double market_price_ = Feature_INVALID;
	double spread_ = Feature_INVALID;
	double open_vwap_1s_ = Feature_INVALID;
	double order_flow_imbalance_1s_ = 0.0;
	double agg_trade_qty_ratio_1s_ = 0.0;
	double large_trade_signal_ = 0.0;
	timer::TimeStamp ts_ = 0;

	std::deque<TradeWindowEntry> trade_window_;
	double rolling_buy_qty_ = 0.0;
	double rolling_sell_qty_ = 0.0;
	double rolling_ofi_ = 0.0;
	double rolling_price_qty_ = 0.0;
	double rolling_volume_ = 0.0;

	mutable std::atomic<uint64_t> seq_{0};
	FeatureSnapshot snapshot_{};

	static constexpr timer::TimeStamp WINDOW_NS = timer::NANOS_TO_SECS;
	static constexpr double LARGE_TRADE_THRESHOLD = 10.0;

public:
	FeatureInfo() = default;

	auto configure(ExchangeName exchange, SymbolName symbol) noexcept -> void
	{
		exchange_ = exchange;
		symbol_ = symbol;
		writeSnapshot();
	}

	auto updateFromBBO(const BBO& bbo, timer::TimeStamp ts) noexcept -> void
	{
		if (UNLIKELY(bbo.bid_price_ == Price_INVALID || bbo.ask_price_ == Price_INVALID ||
					 bbo.bid_qty_ == Qty_INVALID || bbo.ask_qty_ == Qty_INVALID ||
					 bbo.bid_qty_ + bbo.ask_qty_ == 0)) {
			return;
		}

		const auto bid_price = static_cast<double>(bbo.bid_price_);
		const auto ask_price = static_cast<double>(bbo.ask_price_);
		const auto bid_qty = static_cast<double>(bbo.bid_qty_);
		const auto ask_qty = static_cast<double>(bbo.ask_qty_);

		mid_price_ = (bid_price + ask_price) * 0.5;
		market_price_ = (bid_price * ask_qty + ask_price * bid_qty) / (bid_qty + ask_qty);
		spread_ = ask_price - bid_price;
		ts_ = ts;

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

		open_vwap_1s_ = rolling_volume_ > 0.0 ? rolling_price_qty_ / rolling_volume_ : Feature_INVALID;
		order_flow_imbalance_1s_ = rolling_ofi_;
		agg_trade_qty_ratio_1s_ = rolling_buy_qty_ / (rolling_sell_qty_ + 1e-9);
		large_trade_signal_ = std::abs(trade.quantity) >= LARGE_TRADE_THRESHOLD
								? (signed_qty > 0.0 ? 1.0 : -1.0)
								: 0.0;
		ts_ = now;

		writeSnapshot();
	}

	void writeSnapshot() noexcept
	{
		seq_.fetch_add(1, std::memory_order_acq_rel); // odd = writer active

		snapshot_.exchange = exchange_;
		snapshot_.symbol = symbol_;
		snapshot_.mid_price = mid_price_;
		snapshot_.market_price = market_price_;
		snapshot_.spread = spread_;
		snapshot_.open_vwap_1s = open_vwap_1s_;
		snapshot_.order_flow_imbalance_1s = order_flow_imbalance_1s_;
		snapshot_.agg_trade_qty_ratio_1s = agg_trade_qty_ratio_1s_;
		snapshot_.large_trade_signal = large_trade_signal_;
		snapshot_.ts = ts_;

		seq_.fetch_add(1, std::memory_order_release); // even = stable
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

	auto getMidPrice() const noexcept -> double
	{
		return readSnapshot().mid_price;
	}

	auto getMarketPrice() const noexcept -> double
	{
		return readSnapshot().market_price;
	}

	auto getAggTradeQtyRatio() const noexcept -> double
	{
		return readSnapshot().agg_trade_qty_ratio_1s;
	}

private:
	auto addEntry(const TradeWindowEntry& entry) noexcept -> void
	{
		rolling_ofi_ += entry.signed_qty;
		rolling_volume_ += entry.abs_qty;
		rolling_price_qty_ += entry.price_qty;

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
		rolling_price_qty_ -= entry.price_qty;

		if (entry.signed_qty > 0.0) {
			rolling_buy_qty_ -= entry.signed_qty;
		} else {
			rolling_sell_qty_ += entry.signed_qty;
		}
	}
};

class FeatureEngine
{
public:
	FeatureEngine()
	{
		initFeatures();
	}

	FeatureEngine(EventBus&, const int&)
	{
		initFeatures();
	}

	auto updateFromBBO(ExchangeName exchange, SymbolName symbol, const BBO& bbo,
					   timer::TimeStamp ts = timer::getCurNanoTime()) noexcept -> void
	{
		getFeatureInfo(exchange, symbol).updateFromBBO(bbo, ts);
	}

	auto updateFromTrade(const Trade& trade) noexcept -> void
	{
		getFeatureInfo(trade.exchange, trade.symbol).updateFromTrade(trade);
	}

	auto readSnapshot(ExchangeName exchange, SymbolName symbol) const noexcept -> FeatureSnapshot
	{
		return getFeatureInfo(exchange, symbol).readSnapshot();
	}

	auto getFeatures(ExchangeName exchange, SymbolName symbol) const noexcept -> FeatureSnapshot
	{
		return readSnapshot(exchange, symbol);
	}

	auto getMktPrice(ExchangeName exchange = ExchangeName::OKX,
					 SymbolName symbol = SymbolName::BTC_USDT) const noexcept -> double
	{
		return getFeatureInfo(exchange, symbol).getMidPrice();
	}

	auto getAggTradeQtyRatio(ExchangeName exchange = ExchangeName::OKX,
							 SymbolName symbol = SymbolName::BTC_USDT) const noexcept -> double
	{
		return getFeatureInfo(exchange, symbol).getAggTradeQtyRatio();
	}

	FeatureEngine(const FeatureEngine&) = delete;
	FeatureEngine(const FeatureEngine&&) = delete;
	FeatureEngine& operator=(const FeatureEngine&) = delete;
	FeatureEngine& operator=(const FeatureEngine&&) = delete;

private:
	static constexpr size_t EXCHANGE_COUNT = static_cast<size_t>(ExchangeName::DERIBIT) + 1;
	static constexpr size_t SYMBOL_COUNT = static_cast<size_t>(SymbolName::BTC_USDT_SWAP) + 1;

	using SymbolFeatures = std::array<FeatureInfo, SYMBOL_COUNT>;
	using ExchangeFeatures = std::array<SymbolFeatures, EXCHANGE_COUNT>;

	auto initFeatures() noexcept -> void
	{
		for (size_t exchange = 0; exchange < features_.size(); ++exchange) {
			for (size_t symbol = 0; symbol < features_[exchange].size(); ++symbol) {
				features_[exchange][symbol].configure(static_cast<ExchangeName>(exchange),
											  static_cast<SymbolName>(symbol));
			}
		}
	}

	auto getFeatureInfo(ExchangeName exchange, SymbolName symbol) noexcept -> FeatureInfo&
	{
		return features_[static_cast<size_t>(exchange)][static_cast<size_t>(symbol)];
	}

	auto getFeatureInfo(ExchangeName exchange, SymbolName symbol) const noexcept -> const FeatureInfo&
	{
		return features_[static_cast<size_t>(exchange)][static_cast<size_t>(symbol)];
	}

	ExchangeFeatures features_{};
};
}
