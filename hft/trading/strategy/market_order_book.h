#pragma once

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>

#include "common/types.h"
#include "common/mem_pool.h"
#include "common/timer.h"
#include "market_order.h"
#include "exchange/market_data/market_update.h"

namespace Trading 
{
// -------------------- 交易所 + 币对 特性萃取 --------------------
// 根据交易所和币对，提供编译期常量（如深度、最小变动价位）
template<ExchangeName E, SymbolName S>
struct OrderBookTraits;

// OKX BTCUSDT 特性
template<>
struct OrderBookTraits<ExchangeName::OKX, SymbolName::BTC_USDT> {
	static constexpr size_t depth = 5;
	static constexpr double tick_size = 0.01;
	static constexpr const char* name = "OKX BTCUSDT";
};

// OKX BTCUSDT_SWAP 特性
template<>
struct OrderBookTraits<ExchangeName::OKX, SymbolName::BTC_USDT_SWAP> {
	static constexpr size_t depth = 5;
	static constexpr double tick_size = 0.01;
	static constexpr const char* name = "OKX BTC_USDT_SWAP";
};


// -------------------- OrderBook 模板类 --------------------
template<ExchangeName E, SymbolName S>
class OrderBook {
	using Traits = OrderBookTraits<E, S>;
	// 静态数组存储深度（编译期确定大小）
	std::array<Common::PriceLevel, Traits::depth> bids_{};
	std::array<Common::PriceLevel, Traits::depth> asks_{};

	BBO bbo_{};

	uint64_t last_trade_seq_id_{0};
	std::array<std::array<char, Common::MAX_TRADE_ID_LEN>, 16> trade_ids_in_seq_{};
	size_t trade_ids_in_seq_size_{0};

	double last_trade_price_{0.0};
	double last_trade_qty_{0.0};
	Side last_trade_side_{Side::INVALID};
	timer::TimeStamp last_trade_ts_{0};
	double aggressive_buy_qty_{0.0};
	double aggressive_sell_qty_{0.0};

	static auto toBookPrice(double px) noexcept -> Price {
		if (UNLIKELY(px <= 0.0)) {
			return Price_INVALID;
		}
		if (UNLIKELY(Traits::tick_size <= 0.0)) {
			return static_cast<Price>(std::llround(px));
		}
		return static_cast<Price>(std::llround(px / Traits::tick_size));
	}

	static auto toBookQty(double qty) noexcept -> Qty {
		if (UNLIKELY(qty <= 0.0)) {
			return 0;
		}
		return static_cast<Qty>(qty);
	}

	auto hasSeenTradeInCurrentSeq(const char* trade_id) const noexcept -> bool {
		for (size_t i = 0; i < trade_ids_in_seq_size_; ++i) {
			if (std::strncmp(trade_ids_in_seq_[i].data(), trade_id, Common::MAX_TRADE_ID_LEN) == 0) {
				return true;
			}
		}
		return false;
	}

	auto rememberTradeInCurrentSeq(const char* trade_id) noexcept -> void {
		if (trade_ids_in_seq_size_ < trade_ids_in_seq_.size()) {
			auto& slot = trade_ids_in_seq_[trade_ids_in_seq_size_++];
			std::memset(slot.data(), 0, slot.size());
			std::strncpy(slot.data(), trade_id, Common::MAX_TRADE_ID_LEN - 1);
		}
	}

	auto initLevels() noexcept -> void {
		for (size_t i = 0; i < Traits::depth; ++i) {
			bids_[i].exchange = E;
			bids_[i].symbol = S;
			bids_[i].side = Side::BUY;
			bids_[i].order_count = 0;
			bids_[i].price = 0.0;
			bids_[i].quantity = 0.0;
			bids_[i].last_update_time = 0;
			bids_[i].level = static_cast<uint32_t>(i);

			asks_[i].exchange = E;
			asks_[i].symbol = S;
			asks_[i].side = Side::SELL;
			asks_[i].order_count = 0;
			asks_[i].price = 0.0;
			asks_[i].quantity = 0.0;
			asks_[i].last_update_time = 0;
			asks_[i].level = static_cast<uint32_t>(i);
		}
	}

public:
	OrderBook() {
		initLevels();
		updateBBO();
	}

	// 更新价格档位（简化，实际需维护完整订单簿）
	void onPricelevelUpdate(std::shared_ptr<const Common::PriceLevel> pl) 
	{
		if (!pl || pl->level >= Traits::depth) {
			return;
		}

		auto& levels = (pl->side == Side::BUY) ? bids_ : asks_;
		levels[pl->level] = *pl;

		updateBBO();
	}

void onMarketUpdate(std::shared_ptr<const Common::Trade> market_update) 
{
		if (!market_update) {
			return;
		}

		if (market_update->seqId < last_trade_seq_id_) {
			return;
		}

		if (market_update->seqId > last_trade_seq_id_) {
			last_trade_seq_id_ = market_update->seqId;
			trade_ids_in_seq_size_ = 0;
		}

		if (hasSeenTradeInCurrentSeq(market_update->trade_id)) {
			return;
		}

		rememberTradeInCurrentSeq(market_update->trade_id);

		last_trade_price_ = market_update->price;
		last_trade_qty_ = market_update->quantity;
		last_trade_side_ = market_update->side;
		last_trade_ts_ = market_update->timestamp;

		if (market_update->side == Side::BUY) {
			aggressive_buy_qty_ += market_update->quantity;
			if (!asks_.empty()) {
				asks_[0].quantity = std::max(0.0, asks_[0].quantity - market_update->quantity);
				asks_[0].last_update_time = market_update->timestamp;
			}
		} else if (market_update->side == Side::SELL) {
			aggressive_sell_qty_ += market_update->quantity;
			if (!bids_.empty()) {
				bids_[0].quantity = std::max(0.0, bids_[0].quantity - market_update->quantity);
				bids_[0].last_update_time = market_update->timestamp;
			}
		}

		updateBBO();
}

	auto updateBBO() noexcept -> void {
		if (!bids_.empty() && bids_[0].quantity > 0.0 && bids_[0].price > 0.0) {
			bbo_.bid_price_ = toBookPrice(bids_[0].price);
			bbo_.bid_qty_ = toBookQty(bids_[0].quantity);
		} else {
			bbo_.bid_price_ = Price_INVALID;
			bbo_.bid_qty_ = Qty_INVALID;
		}

		if (!asks_.empty() && asks_[0].quantity > 0.0 && asks_[0].price > 0.0) {
			bbo_.ask_price_ = toBookPrice(asks_[0].price);
			bbo_.ask_qty_ = toBookQty(asks_[0].quantity);
		} else {
			bbo_.ask_price_ = Price_INVALID;
			bbo_.ask_qty_ = Qty_INVALID;
		}
}

    auto getBBO() const noexcept -> const BBO* {
      return &bbo_;
    }

    auto toString(bool detailed, bool validity_check) const -> std::string;

    /// Deleted default, copy & move constructors and assignment-operators.
	OrderBook(const OrderBook &) = default;

	OrderBook(OrderBook &&) = default;

	OrderBook &operator=(const OrderBook &) = default;
};

class TradeEngine;

}