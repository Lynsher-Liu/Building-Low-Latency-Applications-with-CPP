#pragma once

#include <cstdint>
#include <limits>
#include <sstream>
#include <array>

#include "macros.h"
#include "timer.h"

namespace Common 
{
	// 订单方向
	enum class Side : int8_t {
		INVALID = 0,
		BUY = 1,
		SELL = -1,
		MAX = 2
	};

	inline std::string sideToString(Side side) 
	{
		switch (side) {
			case Side::BUY:
				return "BUY";
			case Side::SELL:
				return "SELL";
			case Side::INVALID:
				return "INVALID";
			case Side::MAX:
				return "MAX";
			}

		return "UNKNOWN";
	}

	/// Convert Side to an index which can be used to index into a std::array.
	inline constexpr auto sideToIndex(Side side) noexcept {
		return static_cast<size_t>(side) + 1;
	}

	/// Convert Side::BUY=1 and Side::SELL=-1.
	inline constexpr auto sideToValue(Side side) noexcept {
		return static_cast<int>(side);
	}
	/// Constants used across the ecosystem to represent upper bounds on various containers.

	/// Define max lengths
	constexpr size_t MAX_TRADE_ID_LEN = 16;	

	/// Trading instruments / TickerIds from [0, ME_MAX_TICKERS].
	constexpr size_t ME_MAX_TICKERS = 8;

	/// Maximum size of lock free queues used to transfer client requests, client responses and market updates between components.
	constexpr size_t ME_MAX_CLIENT_UPDATES = 256 * 1024;
	constexpr size_t ME_MAX_MARKET_UPDATES = 256 * 1024;

	/// Maximum trading clients.
	constexpr size_t ME_MAX_NUM_CLIENTS = 256;

	/// Maximum number of orders per trading client.
	constexpr size_t ME_MAX_ORDER_IDS = 1024 * 1024;

	/// Maximum price level depth in the order books.
	constexpr size_t ME_MAX_PRICE_LEVELS = 256;

	enum class ExchangeName : uint8_t {
		OKX = 0,
		BINANCE = 1,
		BYBIT = 2,
		DERIBIT = 3
	};

inline std::string exchangeToString(ExchangeName exchange) 
{
    switch (exchange) 
    {
        case ExchangeName::OKX:
            return "OKX";
        case ExchangeName::BINANCE:
            return "BINANCE";
        case ExchangeName::BYBIT:
            return "BYBIT";
        case ExchangeName::DERIBIT:
            return "DERIBIT";
    }

    return "UNKNOWN";
}

enum SymbolName : uint8_t {
    BTC_USDT = 0,
    BTC_USDT_SWAP = 1
};

inline std::string symbolToString(SymbolName symbol) 
{
    switch (symbol) 
    {
        case SymbolName::BTC_USDT:
            return "BTC_USDT";
        case SymbolName::BTC_USDT_SWAP:
            return "BTC_USDT_SWAP";
    }
    return "UNKNOWN";
}


	typedef uint64_t OrderId;
	constexpr auto OrderId_INVALID = std::numeric_limits<OrderId>::max();

	inline auto orderIdToString(OrderId order_id) -> std::string {
		if (UNLIKELY(order_id == OrderId_INVALID)) {
		return "INVALID";
		}

		return std::to_string(order_id);
	}

	typedef uint32_t TickerId;
	constexpr auto TickerId_INVALID = std::numeric_limits<TickerId>::max();

	inline auto tickerIdToString(TickerId ticker_id) -> std::string {
		if (UNLIKELY(ticker_id == TickerId_INVALID)) {
		return "INVALID";
		}

		return std::to_string(ticker_id);
	}

	typedef uint32_t ClientId;
	constexpr auto ClientId_INVALID = std::numeric_limits<ClientId>::max();

	inline auto clientIdToString(ClientId client_id) -> std::string {
		if (UNLIKELY(client_id == ClientId_INVALID)) {
		return "INVALID";
		}

		return std::to_string(client_id);
	}

	typedef int64_t Price;
	constexpr auto Price_INVALID = std::numeric_limits<Price>::max();

	inline auto priceToString(Price price) -> std::string {
		if (UNLIKELY(price == Price_INVALID)) {
		return "INVALID";
		}

		return std::to_string(price);
	}

	typedef uint32_t Qty;
	constexpr auto Qty_INVALID = std::numeric_limits<Qty>::max();

	inline auto qtyToString(Qty qty) -> std::string {
		if (UNLIKELY(qty == Qty_INVALID)) {
		return "INVALID";
		}

		return std::to_string(qty);
	}

	/// Priority represents position in the FIFO queue for all orders with the same side and price attributes.
	typedef uint64_t Priority;
	constexpr auto Priority_INVALID = std::numeric_limits<Priority>::max();

	inline auto priorityToString(Priority priority) -> std::string {
		if (UNLIKELY(priority == Priority_INVALID)) {
		return "INVALID";
		}

		return std::to_string(priority);
	}

	

	/// Type of trading algorithm.
	enum class AlgoType : int8_t {
		INVALID = 0,
		RANDOM = 1,
		MAKER = 2,
		TAKER = 3,
		MAX = 4
	};

	inline auto algoTypeToString(AlgoType type) -> std::string {
		switch (type) {
		case AlgoType::RANDOM:
			return "RANDOM";
		case AlgoType::MAKER:
			return "MAKER";
		case AlgoType::TAKER:
			return "TAKER";
		case AlgoType::INVALID:
			return "INVALID";
		case AlgoType::MAX:
			return "MAX";
		}

		return "UNKNOWN";
	}

	inline auto stringToAlgoType(const std::string &str) -> AlgoType {
		for (auto i = static_cast<int>(AlgoType::INVALID); i <= static_cast<int>(AlgoType::MAX); ++i) {
		const auto algo_type = static_cast<AlgoType>(i);
		if (algoTypeToString(algo_type) == str)
			return algo_type;
		}

		return AlgoType::INVALID;
	}

	/// Risk configuration containing limits on risk parameters for the RiskManager.
	struct RiskCfg {
		Qty max_order_size_ = 0;
		Qty max_position_ = 0;
		double max_loss_ = 0;

		auto toString() const {
		std::stringstream ss;

		ss << "RiskCfg{"
			<< "max-order-size:" << qtyToString(max_order_size_) << " "
			<< "max-position:" << qtyToString(max_position_) << " "
			<< "max-loss:" << max_loss_
			<< "}";

		return ss.str();
		}
	};

	/// Top level configuration to configure the TradeEngine, trading algorithm and RiskManager.
	struct TradeEngineCfg {
		Qty clip_ = 0;
		double threshold_ = 0;
		RiskCfg risk_cfg_;

		auto toString() const {
		std::stringstream ss;
		ss << "TradeEngineCfg{"
			<< "clip:" << qtyToString(clip_) << " "
			<< "thresh:" << threshold_ << " "
			<< "risk:" << risk_cfg_.toString()
			<< "}";

		return ss.str();
		}
	};

	/// Hash map from TickerId -> TradeEngineCfg.
	typedef std::array<TradeEngineCfg, ME_MAX_TICKERS> TradeEngineCfgHashMap;


// okx only provide these message info for each price level
struct PriceLevel 
{
	ExchangeName exchange{ExchangeName::OKX};
    SymbolName symbol{SymbolName::BTC_USDT};
	Side side{Side::INVALID};
	uint32_t order_count{1};
	
    double price{0.0};
    double quantity{0.0};	
    timer::TimeStamp last_update_time{0};

    /**
     * price level, for books5, level=0 means best bid/ask, level=1 means second best bid/ask, etc. 
     * For example, if we receive a books5 update with 5 price levels, we can assign level=1 to the best bid/ask, level=2 to the second best bid/ask, etc.
     */
    uint32_t level{0}; 
    static int id;
    
    PriceLevel() = default; // keep the default constructor for event
	// 	exchange(ExchangeName::OKX), 
	// 	symbol(SymbolName::BTC_USDT), 
	// 	side(Side::INVALID),
	// 	price(0.0), 
	// 	quantity(0.0), 
	// 	order_count(1), 
	// 	last_update_time(0), 
	// 	level(0)
    // {
    //     id++;
    // }

    PriceLevel(ExchangeName e, SymbolName s, Side si, double p, double q, uint32_t c, timer::TimeStamp ts, uint32_t l) : 
		exchange(e), 
		symbol(s), 
		side(si),
		price(p), 
		quantity(q), 
		order_count(c), 
		last_update_time(ts), 
		level(l) 
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
         << " level:" << level
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
	timer::TimeStamp timestamp{0};

    static int id;

	Trade() = default;

	Trade(ExchangeName e, SymbolName s, uint32_t c, Side si, double p, double q, uint64_t seq, int src, const char* tid, timer::TimeStamp ts) : 
		exchange(e), 
		symbol(s), 
		count(c), 
		side(si), 
		price(p), 
		quantity(q), 
		seqId(seq), 
		source(src), 
		timestamp(ts)
		{
			strncpy(trade_id, tid, MAX_TRADE_ID_LEN);
			id++;
		}

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

int Trade::id = 0;
}
