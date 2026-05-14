#include <csignal>
#include <map>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <json/json.hpp>
#include <deque>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/bind/bind.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>

#include "strategy/trade_engine.h"
#include "order_gw/order_gateway.h"
#include "market_data/websocket.h"
#include "strategy/strategy_manager.h"
#include "strategy/exchange_processor.h"

#include "common/timer.h"
#include "common/affinity.h"

static AsnLoggerPtr logger_main = ASN_GETLOGGER("trading_main");

/// Main components.
//Common::Logger *logger = nullptr;
Trading::TradeEngine *trade_engine = nullptr;
//Trading::MarketDataConsumer *market_data_consumer = nullptr;
Trading::OrderGateway *order_gateway = nullptr;


int main(int argc, char **argv) 
{
	if(argc < 2) {
		//FATAL("USAGE trading_main CLIENT_ID ALGO_TYPE [CLIP_1 THRESH_1 MAX_ORDER_SIZE_1 MAX_POS_1 MAX_LOSS_1] [CLIP_2 THRESH_2 MAX_ORDER_SIZE_2 MAX_POS_2 MAX_LOSS_2] ...");
	}

	ASN_INITLOG("./config/trading.log.properties");

	// 1. Initiate common components like EventBus, IO context, etc.
	Common::EventBus bus;
	const int bindToNumaNode = affinity::get_least_loaded_numa_node();


	boost::asio::io_context ioc;

	// 2. Create ExchangeProcessors for each exchange and put them into the exchangeManager
	Trading::OKXProcessor okx_processor(bus, bindToNumaNode);
	Trading::BinanceProcessor binance_processor(bus, bindToNumaNode);
	Trading::BybitProcessor bybit_processor(bus, bindToNumaNode);
	Trading::DeribitProcessor deribit_processor(bus, bindToNumaNode);

	Trading::ExchangeManager exchangeManager = Trading::ExchangeManager(okx_processor, binance_processor, bybit_processor, deribit_processor);

	// 3. Create the websocket client to consume market data and feed into the corresponding ExchangeProcessor	
	Market::AsyncWebsocketClient* wsclient 
	    = new Market::AsyncWebsocketClient(ioc, "wspap.okx.com", "8443", bindToNumaNode, exchangeManager);

	// 4. Create strategies and strategy manager, who subscribes to the EventBus and automatically run
	Trading::StrategyManager<Trading::SimpleMM, Trading::CrossExArb> strategy_manager(
		bus, bindToNumaNode,
		Trading::SimpleMM(ExchangeName::OKX, SymbolName::BTC_USDT),
		Trading::CrossExArb(SymbolName::BTC_USDT));

	wsclient->start();

	boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
	signals.async_wait([&ioc, wsclient](const boost::system::error_code&, int) 
	{
		ASN_INFO(logger_main,  "---------------------------------------------------------");
		ASN_INFO(logger_main,  "-------------------------Stop main-----------------------");
		ASN_INFO(logger_main,  "---------------------------------------------------------\n\n\n\n");
		
		wsclient->stop();  // 优雅停止客户端
		ioc.stop();      // 停止事件循环

		using namespace std::literals::chrono_literals;
		std::this_thread::sleep_for(3s);
	});

  	ASN_INFO(logger_main,  "------------------------------------------------------------------");
	ASN_INFO(logger_main,  "------------------IO event loop runs, block here------------------");
	ASN_INFO(logger_main,  "------------------------------------------------------------------\n\n\n\n");
  
	ioc.run(); // block here until m_ioc stops
	ASN_INFO(logger_main, "IO event loop stopped successfully");

	return 0;

	const Common::ClientId client_id = atoi(argv[1]);
	srand(client_id);

	const auto algo_type = stringToAlgoType(argv[2]);

	const int sleep_time = 20 * 1000;

	// The lock free queues to facilitate communication between order gateway <-> trade engine and market data consumer -> trade engine.
	Exchange::ClientRequestLFQueue client_requests(ME_MAX_CLIENT_UPDATES);
	Exchange::ClientResponseLFQueue client_responses(ME_MAX_CLIENT_UPDATES);
	Exchange::MEMarketUpdateLFQueue market_updates(ME_MAX_MARKET_UPDATES);

	std::string time_str;

	TradeEngineCfgHashMap ticker_cfg;

	// Parse and initialize the TradeEngineCfgHashMap above from the command line arguments.
	// [CLIP_1 THRESH_1 MAX_ORDER_SIZE_1 MAX_POS_1 MAX_LOSS_1] [CLIP_2 THRESH_2 MAX_ORDER_SIZE_2 MAX_POS_2 MAX_LOSS_2] ...
	size_t next_ticker_id = 0;
	for (int i = 3; i < argc; i += 5, ++next_ticker_id) {
		ticker_cfg.at(next_ticker_id) = {static_cast<Qty>(std::atoi(argv[i])), std::atof(argv[i + 1]),
										{static_cast<Qty>(std::atoi(argv[i + 2])),
										static_cast<Qty>(std::atoi(argv[i + 3])),
										std::atof(argv[i + 4])}};
	}

	//logger->log("%:% %() % Starting Trade Engine...\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str));
	trade_engine = new Trading::TradeEngine(client_id, algo_type,
											ticker_cfg,
											&client_requests,
											&client_responses,
											&market_updates,
											bus,
											bindToNumaNode);
	trade_engine->start();

	const std::string order_gw_ip = "127.0.0.1";
	const std::string order_gw_iface = "lo";
	const int order_gw_port = 12345;
	order_gateway = new Trading::OrderGateway(client_id, &client_requests, &client_responses, order_gw_ip, order_gw_iface, order_gw_port);
	order_gateway->start();

	usleep(10 * 1000 * 1000);
	trade_engine->initLastEventTime();

	while (trade_engine->silentSeconds() < 60) {
		using namespace std::literals::chrono_literals;
		std::this_thread::sleep_for(30s);
	}

	trade_engine->stop();
	order_gateway->stop();

	using namespace std::literals::chrono_literals;
	std::this_thread::sleep_for(1s);

	delete trade_engine;
	trade_engine = nullptr;
	delete order_gateway;
	order_gateway = nullptr;
	return 0;

}
