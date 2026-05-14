/*
 * @Author: Lynsher xinyiliu@astri.org
 * @Date: 2025-12-01 13:52:35
 * @LastEditors: Lynsher xinyiliu@astri.org
 * @LastEditTime: 2026-05-14 17:24:07
 * @FilePath: /my_HFT/hft/trading/strategy/feature_engine.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#pragma once

#include <limits>

#include "common/macros.h"
#include "common/event_bus.h"
#include "common/AsnLog.h"
#include "market_order_book.h"

using namespace Common;

static AsnLoggerPtr loggerfeatureEngine_H = ASN_GETLOGGER("featureEngine_h");

namespace Trading 
{
constexpr auto Feature_INVALID = std::numeric_limits<double>::quiet_NaN();

struct FeatureSet {
	double mid_price = Price_INVALID;      // 中间价
	double fair_price = Price_INVALID;          // 公允价格（通常为 (bid+ask)/2 或深度加权）
	double spread = Price_INVALID;              // 买卖价差

	double agg_trade_qty_ratio = 0.0; // 主动买入量/主动卖出量
	double order_flow_imbalance = 0.0;// 过去N秒主动买入-卖出量
	double large_trade_signal = 0.0;  // 大单信号（0或1，方向可编码为+1/-1）
	double vwap = 0.0;                // 成交量加权平均价
	// 可根据需要扩展
};

class FeatureEngine : public EventSubscriber
{
public:
	struct PerSymbolData 
	{
		std::atomic<double> mid_price{Price_INVALID};
        std::atomic<double> fair_price{Price_INVALID};
        std::atomic<double> agg_trade_qty_ratio{0.0};
        std::atomic<double> order_flow_imbalance{0.0};
        std::atomic<double> large_trade_signal{0.0};
        std::atomic<double> vwap{0.0};
        std::atomic<double> spread{Price_INVALID};

        // 用于内部计算的滚动窗口（非原子，仅由本线程写）
        std::deque<std::pair<uint64_t, double>> trade_qty_window; // 时间戳, 主动买入量（正）或卖出量（负）
        double cumulative_buy_volume = 0.0;
        double cumulative_sell_volume = 0.0;
        double cumulative_price_volume = 0.0;   // 用于 VWAP
        double cumulative_volume = 0.0;
        uint64_t last_trade_ts = 0;

        static constexpr uint64_t WINDOW_MS = 1000; // 1秒窗口
        static constexpr double LARGE_TRADE_THRESHOLD = 10.0; // 假设10个币
    };

	FeatureEngine(EventBus& bus, const int& numaNode) : EventSubscriber(bus, "FeatureEngine", numaNode) {}	

	// 策略调用接口（线程安全）
	FeatureSet getFeatures(ExchangeName ex, SymbolName sym) const 
	{
		std::shared_lock lock(mutex_);
		auto it_ex = features_.find(ex);
		if (it_ex == features_.end()) return FeatureSet{};
		auto it_sym = it_ex->second.find(sym);
		if (it_sym == it_ex->second.end()) return FeatureSet{};
		const auto& data = it_sym->second;

		FeatureSet fs;
		fs.mid_price = data.mid_price.load(std::memory_order_acquire);
		fs.fair_price = data.fair_price.load(std::memory_order_acquire);
		fs.agg_trade_qty_ratio = data.agg_trade_qty_ratio.load(std::memory_order_acquire);
		fs.order_flow_imbalance = data.order_flow_imbalance.load(std::memory_order_acquire);
		fs.large_trade_signal = data.large_trade_signal.load(std::memory_order_acquire);
		fs.vwap = data.vwap.load(std::memory_order_acquire);
		fs.spread = data.spread.load(std::memory_order_acquire);
		
		return fs;
	}


	/// Process a trade event and in this case compute the feature to capture aggressive trade quantity ratio against the BBO quantity.
	auto onTradeUpdate(const Exchange::MEMarketUpdate *market_update, Trading::OrderBook* book) noexcept -> void {
		const auto bbo = book->getBBO();
		if(LIKELY(bbo->bid_price_ != Price_INVALID && bbo->ask_price_ != Price_INVALID)) {
		agg_trade_qty_ratio_ = static_cast<double>(market_update->qty_) / (market_update->side_ == Side::BUY ? bbo->ask_qty_ : bbo->bid_qty_);
		}

	}

	auto getMktPrice() const noexcept {
		return mkt_price_;
	}

	auto getAggTradeQtyRatio() const noexcept {
		return agg_trade_qty_ratio_;
	}

	FeatureEngine(const FeatureEngine &) = delete;
	FeatureEngine(const FeatureEngine &&) = delete;
	FeatureEngine &operator=(const FeatureEngine &) = delete;
	FeatureEngine &operator=(const FeatureEngine &&) = delete;

private:
	void handleEvent(const Event& event) override 
	{
        std::visit([this](const auto& e) {
            using T = std::decay_t<decltype(e)>;
            // if constexpr (std::is_same_v<T, PriceLevel>) {
            //     onPriceLevelUpdate(e);
            // } 
			if constexpr (std::is_same_v<T, Trade>) { 
                onTradeEvent(e);
            }
        }, event);
    }

	void onOrderBookUpdate(ExchangeName E, SymbolName S, const BBO* bbo) 
	{
        auto& data = getOrCreateData(E, S);
		if(bbo->bid_price_ != Price_INVALID && bbo->ask_price_ != Price_INVALID)
		{
			// calc mid price
			double mid = (bbo->bid_price_ + bbo->ask_price_) / 2.0;
			data.mid_price.store(mid, std::memory_order_release);

			// calc fair price
			double fair_price_ = (bbo->bid_price_ * bbo->ask_qty_ + bbo->ask_price_ * bbo->bid_qty_) / static_cast<double>(bbo->bid_qty_ + bbo->ask_qty_);
			data.fair_price.store(fair_price_, std::memory_order_release);

			// calc spread
			double spread = bbo->ask_price_ - bbo->bid_price_;
			data.spread.store(spread, std::memory_order_release);

			ASN_INFO(loggerfeatureEngine_H, "Updated features for " + std::to_string(static_cast<int>(E)) + " " + std::to_string(static_cast<int>(S)) + 
				": mid_price =" + std::to_string(mid) + 
				", fair_price =" + std::to_string(fair_price_) + 
				", spread =" + std::to_string(spread));
		}
        else
		{
			ASN_INFO(loggerfeatureEngine_H, "bbo->bid_price_: " + std::to_string(bbo->bid_price_) + ", Best Ask: " + std::to_string(bbo->ask_price_)
				<< " invalid! Not update mid_price, fair_price and spread for " << static_cast<int>(E) << " " << static_cast<int>(S));
		}
    }

    void onTradeEvent(const Trade& e) 
	{
        auto& data = getOrCreateData(e.exchange, e.symbol);
        uint64_t now = e.timestamp; // 假设事件中有时间戳

        // 1. 更新成交累计（用于 agg_trade_qty_ratio 和 OFI）
        double trade_value = e.quantity;
        // 假设 side: true 表示主动买入，false 表示主动卖出（需根据实际定义）
        double signed_qty = e.is_buyer_maker ? -e.quantity : e.quantity; // 买入为正，卖出为负
        // 更新累计买卖量
        if (signed_qty > 0) data.cumulative_buy_volume += signed_qty;
        else data.cumulative_sell_volume -= signed_qty;
        double ratio = data.cumulative_buy_volume / (data.cumulative_sell_volume + 1e-9);
        data.agg_trade_qty_ratio.store(ratio, std::memory_order_release);

        // 2. 滑动窗口计算 OFI（最近1秒净主动买入量）
        data.trade_qty_window.push_back({now, signed_qty});
        // 移除超过窗口的旧数据
        while (!data.trade_qty_window.empty() && now - data.trade_qty_window.front().first > PerSymbolData::WINDOW_MS) {
            data.trade_qty_window.pop_front();
        }
        double ofi = 0.0;
        for (const auto& [ts, qty] : data.trade_qty_window) {
            ofi += qty;
        }
        data.order_flow_imbalance.store(ofi, std::memory_order_release);

        // 3. 大单监控
        bool is_large = std::abs(e.quantity) > PerSymbolData::LARGE_TRADE_THRESHOLD;
        double large_signal = is_large ? (signed_qty > 0 ? 1.0 : -1.0) : 0.0;
        data.large_trade_signal.store(large_signal, std::memory_order_release);

        // 4. VWAP（累计加权平均）
        data.cumulative_price_volume += e.price * e.quantity;
        data.cumulative_volume += e.quantity;
        double vwap = data.cumulative_price_volume / (data.cumulative_volume + 1e-9);
        data.vwap.store(vwap, std::memory_order_release);
    }

    PerSymbolData& getOrCreateData(ExchangeName ex, SymbolName sym) {
        std::unique_lock lock(mutex_);
        auto& ex_map = features_[ex];
        auto it = ex_map.find(sym);
        if (it == ex_map.end()) {
            it = ex_map.emplace(sym, PerSymbolData{}).first;
        }
        return it->second;
    }

	//Trading::ExchangeManager& exchangeManager_;

	mutable std::shared_mutex mutex_;
    std::unordered_map<ExchangeName, std::unordered_map<SymbolName, PerSymbolData>> features_;

	/// The two features we compute in our feature engine.
	double mkt_price_ = Feature_INVALID, agg_trade_qty_ratio_ = Feature_INVALID;
	};
}
