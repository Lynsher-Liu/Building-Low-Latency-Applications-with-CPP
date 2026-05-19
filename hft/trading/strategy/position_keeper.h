#pragma once

#include "common/macros.h"
#include "common/types.h"

#include "exchange/order_server/client_response.h"

#include "market_order_book.h"

using namespace Common;

namespace Trading 
{
struct PositionSnapshot 
{
	ExchangeName exchange;
	SymbolName symbol;
	BBOSnapshot bbo;
	int32_t position = 0;
	double realized_pnl = 0.0;
	double unrealized_pnl = 0.0;
	double total_pnl = 0.0;
};

// PositionInfo tracks the position, pnl (realized and unrealized) and volume for a single trading instrument.
/**
 *  PositionInfo 是否需要做成模板类？
	不需要。 原因：
		1.  所有成员类型都与 exchange 无关 — position_(int32_t)、real_pnl_/unreal_pnl_/total_pnl_(double)、
			open_vwap_(array<double,3>)、volume_(Qty)、bbo_(const BBO*) 都是通用类型
		2.  addFill 的参数 Exchange::MEClientResponse* 也是固定类型，不随 exchange 变化。
		3.  做成模板只会增加编译时间和代码膨胀，不会带来任何性能收益 — 
				PositionInfo 的热路径（addFill、updatePnlByBBO、writeSnapshot/readSnapshot seqlock）
				不包含任何可以通过模板消除的运行时分支
 */
class PositionInfo 
{
private:
	ExchangeName exchange;
	SymbolName symbol;

	int32_t position_ = 0;
	double real_pnl_ = 0, unreal_pnl_ = 0, total_pnl_ = 0;
	std::array<double, sideToIndex(Side::MAX) + 1> open_vwap_{};
	Qty volume_ = 0; //the total quantity that has been executed
	const BBO *bbo_ = nullptr;

	// variables for generating position snapshot and read
	mutable std::atomic<uint64_t> seq_{0};
	PositionSnapshot snapshot_{};

public:
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
	 *  Process a change from BBO
	 *  calculate mid_price and unrealized pnl
	 * */ 
	auto updateFromBBO(const BBO* bbo) noexcept 
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
			{
				std::cout << timer::getCurNanoTime() << " " << toString() << " " << bbo_->toString() << "\n";
			}
		}
	}

	PositionSnapshot readSnapshot() const noexcept 
	{
		PositionSnapshot out;

		for (;;) {
			const auto before = seq_.load(std::memory_order_acquire);
			if (before & 1) continue;

			out = snapshot_;

			const auto after = seq_.load(std::memory_order_acquire);
			if (before == after && !(after & 1)) {
				return out;
			}
		}
	}

private:
	/**
	 * Use a seqlock-style published snapshot for readers, this version assumes single writer.
	 * 
	 * That fits the recommended design: the ExchangeProcessor / hot market-data thread updates PositionKeeper, 
	 * and other threads only read snapshots.
	 */
	void writeSnapshot() noexcept 
	{
		seq_.fetch_add(1, std::memory_order_acq_rel); // odd = writer active

		// only single writer thread writes this
		snapshot_.exchange = exchange;
		snapshot_.symbol = symbol;
		snapshot_.position = position_;
		snapshot_.realized_pnl = real_pnl_;
		snapshot_.unrealized_pnl = unreal_pnl_;
		snapshot_.total_pnl = total_pnl_;
		const auto ts = timer::getCurNanoTime();
		if (bbo_) {
			snapshot_.bbo = bbo_->writeSnapshot(ts);
		} else {
			snapshot_.bbo = BBOSnapshot{};
			snapshot_.bbo.ts = ts;
		}

		seq_.fetch_add(1, std::memory_order_release); // even = stable
	}
};



/// Symbol-pack PositionKeeper: intended for ExchangeProcessor hot path.
/// - Single writer (exchange processor thread), multi-reader via PositionInfo seqlock snapshots.
/// - Routes by SymbolName using a small compile-time symbol pack.
template<ExchangeName E, SymbolName... Symbols>
class PositionKeeper 
{
public:
	static constexpr size_t kNumSymbols = sizeof...(Symbols);
	static constexpr std::array<SymbolName, kNumSymbols> kSymbols{Symbols...};
	static constexpr size_t npos = static_cast<size_t>(-1);

private:
	std::array<PositionInfo, kNumSymbols> positions_{};

public:
	PositionKeeper() noexcept
	{
		for (size_t i = 0; i < positions_.size(); ++i) {
			positions_[i].exchange = E;
			positions_[i].symbol = kSymbols[i];
		}
	}

	PositionKeeper(const PositionKeeper &) = delete;
	PositionKeeper(const PositionKeeper &&) = delete;
	PositionKeeper &operator=(const PositionKeeper &) = delete;
	PositionKeeper &operator=(const PositionKeeper &&) = delete;

	static inline auto symbolIndex(SymbolName symbol) noexcept -> size_t
	{
		for (size_t i = 0; i < kSymbols.size(); ++i) {
			if (kSymbols[i] == symbol) {
				return i;
			}
		}
		return npos;
	}

	/**
	 * run-time visit
	 */
	inline auto find(SymbolName symbol) noexcept -> PositionInfo*
	{
		const auto idx = symbolIndex(symbol);
		return (idx == npos) ? nullptr : &(positions_[idx]);
	}

	inline auto find(SymbolName symbol) const noexcept -> const PositionInfo*
	{
		const auto idx = symbolIndex(symbol);
		return (idx == npos) ? nullptr : &(positions_[idx]);
	}

	/**
	 * compile-time visit
	 */
	template<SymbolName S>
	inline auto info() noexcept -> PositionInfo&
	{
		constexpr auto idx = []() constexpr {
			constexpr std::array<SymbolName, kNumSymbols> syms{Symbols...};
			for (size_t i = 0; i < syms.size(); ++i) {
				if (syms[i] == S) return i;
			}
			return npos;
		}();
		static_assert(idx != npos, "Symbol not present in PositionKeeper symbol pack.");
		return positions_[idx];
	}

	inline auto addFill(const Exchange::MEClientResponse *client_response, SymbolName symbol) noexcept -> void
	{
		auto *pos = find(symbol);
		if (UNLIKELY(!pos)) {
			return;
		}
		pos->addFill(client_response);
		pos->writeSnapshot();
	}

	inline auto updateFromBBO(SymbolName symbol, const BBO* bbo) noexcept -> void
	{
		auto *pos = find(symbol);
		if (UNLIKELY(!pos)) {
			return;
		}
		pos->updateFromBBO(bbo);
		pos->writeSnapshot();
	}

	inline auto getPositionInfoSnapshot(SymbolName symbol) const noexcept -> const PositionSnapshot
	{
		return find(symbol)->readSnapshot();
	}

	inline auto toString() const -> std::string
	{
		double total_pnl = 0;
		Qty total_vol = 0;

		std::stringstream ss;
		for (size_t i = 0; i < positions_.size(); ++i) {
			ss << "Exchange:" << exchangeToString(E) << ", Symbol:" << symbolToString(positions_[i].symbol)
				<< " " << positions_[i].toString() << "\n";

			total_pnl += positions_[i].total_pnl_;
			total_vol += positions_[i].volume_;
		}
		ss << "Total PnL:" << total_pnl << " Vol:" << total_vol << "\n";
		return ss.str();
	}
};

#if 0
/// Top level position keeper class to compute position, pnl and volume for all trading instruments.
/// Legacy ticker-id keyed keeper used by the original TradeEngine / RiskManager path.
class PositionKeeperTickerId 
{
public:
	PositionKeeperTickerId() = default;

	PositionKeeperTickerId(const PositionKeeperTickerId &) = delete;
	PositionKeeperTickerId(const PositionKeeperTickerId &&) = delete;
	PositionKeeperTickerId &operator=(const PositionKeeperTickerId &) = delete;
	PositionKeeperTickerId &operator=(const PositionKeeperTickerId &&) = delete;

private:
	ExchangeName exchange{ExchangeName::OKX};
	std::string time_str_;

	/// Hash map container from SymbolName -> PositionInfo.
	std::array<PositionInfo, ME_MAX_TICKERS> ticker_position_;

	public:
	auto addFill(const Exchange::MEClientResponse *client_response) noexcept 
	{
		ticker_position_.at(client_response->ticker_id_).addFill(client_response);
		ticker_position_.at(client_response->ticker_id_).writeSnapshot();
	}

	void updateFromBBO(SymbolName symbol, const BBO *bbo) noexcept 
	{
		ticker_position_.at(symbol).updateFromBBO(bbo);     // mark position using that same snapshot
		ticker_position_.at(symbol).writeSnapshot();    // optional seqlock publication
	}

	auto getPositionInfo(TickerId ticker_id) const noexcept {
		return &(ticker_position_.at(ticker_id));
	}

	auto toString() const 
	{
		double total_pnl = 0;
		Qty total_vol = 0;

		std::stringstream ss;
		for (size_t i = 0; i < ticker_position_.size(); ++i) 
		{
			ss << "Exchange:" << exchangeToString(exchange) << ", Symbol:" << symbolToString(ticker_position_.at(i).symbol) 
				<< " " << ticker_position_.at(i).toString() << "\n";

			total_pnl += ticker_position_.at(i).total_pnl_;
			total_vol += ticker_position_.at(i).volume_;
		}
		ss << "Total PnL:" << total_pnl << " Vol:" << total_vol << "\n";

		return ss.str();
	}
};
#endif
}
