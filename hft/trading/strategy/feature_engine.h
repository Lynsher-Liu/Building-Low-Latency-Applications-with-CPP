/*
 * @Author: Lynsher xinyiliu@astri.org
 * @Date: 2025-12-01 13:52:35
 * @LastEditors: Lynsher xinyiliu@astri.org
 * @LastEditTime: 2026-05-18 00:00:00
 * @FilePath: /my_HFT/hft/trading/strategy/feature_engine.h
 * @Description: Single-writer, multi-reader feature snapshots for the trading hot path.
 */
#pragma once

#include <array>
#include <atomic>
#include <cmath>
#include <deque>
#include <limits>
#include <utility>

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
	double micro_price = Feature_INVALID;
	double spread = Feature_INVALID;

	double vwap_1s = Feature_INVALID;
	double order_flow_imbalance_1s = 0.0;
	double agg_trade_qty_ratio_1s = 0.0;
	double large_trade_signal = 0.0;

	timer::TimeStamp ts = 0;
};

class FeatureInfo
{
public:
	FeatureInfo() = default;

	auto configure(ExchangeName exchange, SymbolName symbol) noexcept -> void
	{
		exchange_ = exchange;
		symbol_ = symbol;
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

		beginPublish();
		mid_price_.store((bid_price + ask_price) * 0.5, std::memory_order_relaxed);
		micro_price_.store((bid_price * ask_qty + ask_price * bid_qty) / (bid_qty + ask_qty),
						   std::memory_order_relaxed);
		spread_.store(ask_price - bid_price, std::memory_order_relaxed);
		ts_.store(ts, std::memory_order_relaxed);
		endPublish();
	}

	auto updateFromTrade(const Trade& trade) noexcept -> void
	{
		const auto now = trade.timestamp ? trade.timestamp : timer::getCurNanoTime();
		const auto signed_qty = (trade.side == Side::BUY) ? trade.quantity : -trade.quantity;

		trade_qty_window_.push_back({now, signed_qty});
		trade_pv_window_.push_back({now, trade.price * trade.quantity});
		trade_volume_window_.push_back({now, trade.quantity});

		while (!trade_qty_window_.empty() && now - trade_qty_window_.front().first > WINDOW_NS) {
			trade_qty_window_.pop_front();
		}
		while (!trade_pv_window_.empty() && now - trade_pv_window_.front().first > WINDOW_NS) {
			trade_pv_window_.pop_front();
		}
		while (!trade_volume_window_.empty() && now - trade_volume_window_.front().first > WINDOW_NS) {
			trade_volume_window_.pop_front();
		}

		double buy_qty = 0.0;
		double sell_qty = 0.0;
		double ofi = 0.0;
		for (const auto& [ts, qty] : trade_qty_window_) {
			(void)ts;
			ofi += qty;
			if (qty > 0.0) {
				buy_qty += qty;
			} else {
				sell_qty -= qty;
			}
		}

		double pv = 0.0;
		for (const auto& [ts, value] : trade_pv_window_) {
			(void)ts;
			pv += value;
		}

		double volume = 0.0;
		for (const auto& [ts, qty] : trade_volume_window_) {
			(void)ts;
			volume += qty;
		}

		const auto vwap = volume > 0.0 ? pv / volume : Feature_INVALID;
		const auto ratio = buy_qty / (sell_qty + 1e-9);
		const auto large_signal = std::abs(trade.quantity) >= LARGE_TRADE_THRESHOLD
									? (signed_qty > 0.0 ? 1.0 : -1.0)
									: 0.0;

		beginPublish();
		vwap_1s_.store(vwap, std::memory_order_relaxed);
		order_flow_imbalance_1s_.store(ofi, std::memory_order_relaxed);
		agg_trade_qty_ratio_1s_.store(ratio, std::memory_order_relaxed);
		large_trade_signal_.store(large_signal, std::memory_order_relaxed);
		ts_.store(now, std::memory_order_relaxed);
		endPublish();
	}

	auto readSnapshot() const noexcept -> FeatureSnapshot
	{
		FeatureSnapshot out;

		for (;;) {
			const auto before = seq_.load(std::memory_order_acquire);
			if (before & 1U) {
				continue;
			}

			out.exchange = exchange_;
			out.symbol = symbol_;
			out.mid_price = mid_price_.load(std::memory_order_relaxed);
			out.micro_price = micro_price_.load(std::memory_order_relaxed);
			out.spread = spread_.load(std::memory_order_relaxed);
			out.vwap_1s = vwap_1s_.load(std::memory_order_relaxed);
			out.order_flow_imbalance_1s = order_flow_imbalance_1s_.load(std::memory_order_relaxed);
			out.agg_trade_qty_ratio_1s = agg_trade_qty_ratio_1s_.load(std::memory_order_relaxed);
			out.large_trade_signal = large_trade_signal_.load(std::memory_order_relaxed);
			out.ts = ts_.load(std::memory_order_relaxed);

			const auto after = seq_.load(std::memory_order_acquire);
			if (LIKELY(before == after && !(after & 1U))) {
				return out;
			}
		}
	}

	auto getMidPrice() const noexcept -> double
	{
		return mid_price_.load(std::memory_order_acquire);
	}

	auto getAggTradeQtyRatio() const noexcept -> double
	{
		return agg_trade_qty_ratio_1s_.load(std::memory_order_acquire);
	}

private:
	static constexpr timer::TimeStamp WINDOW_NS = timer::NANOS_TO_SECS;
	static constexpr double LARGE_TRADE_THRESHOLD = 10.0;

	auto beginPublish() noexcept -> void
	{
		seq_.fetch_add(1, std::memory_order_acq_rel);
	}

	auto endPublish() noexcept -> void
	{
		seq_.fetch_add(1, std::memory_order_release);
	}

	ExchangeName exchange_ = ExchangeName::OKX;
	SymbolName symbol_ = SymbolName::BTC_USDT;

	std::atomic<uint64_t> seq_{0};
	std::atomic<double> mid_price_{Feature_INVALID};
	std::atomic<double> micro_price_{Feature_INVALID};
	std::atomic<double> spread_{Feature_INVALID};
	std::atomic<double> vwap_1s_{Feature_INVALID};
	std::atomic<double> order_flow_imbalance_1s_{0.0};
	std::atomic<double> agg_trade_qty_ratio_1s_{0.0};
	std::atomic<double> large_trade_signal_{0.0};
	std::atomic<timer::TimeStamp> ts_{0};

	std::deque<std::pair<timer::TimeStamp, double>> trade_qty_window_;
	std::deque<std::pair<timer::TimeStamp, double>> trade_pv_window_;
	std::deque<std::pair<timer::TimeStamp, double>> trade_volume_window_;
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
