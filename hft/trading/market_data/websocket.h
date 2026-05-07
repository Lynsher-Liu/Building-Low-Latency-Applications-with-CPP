#pragma once

#include <functional>
#include <tuple>
#include <map>
#include <deque>

#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <atomic>
#include <chrono>
#include <optional>
#include <json/json.hpp>
#include <iostream> 
#include <fstream>

#include "concurrentqueue/concurrentqueue.h"
#include <folly/concurrency/ConcurrentHashMap.h>

#include "common/thread_utils.h"
#include "common/lf_queue.h"
#include "common/macros.h"
#include "common/AsnLog.h"
#include "common/timer.h"

#include "ws_struct.h"
#include "../strategy/exchange_processor.h"

#include "exchange/market_data/market_update.h"

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/algorithm/string.hpp>
#include <cstdlib>

static AsnLoggerPtr loggerWebsocketH = ASN_GETLOGGER("websocket_h");

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace ws = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl;       // from <boost/asio/ssl.hpp>
using namespace std::chrono_literals;

using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
using json = nlohmann::json;

using callback_t = std::function<void(const json&)>;

// Report a failure
inline void fail(beast::error_code ec, char const* what)
{
    std::cerr << what << ": " << ec.message() << "\n";
}


namespace Market
{

/**
 * Definition for async MQTT message router
*/

struct WsRoute 
{
    std::string topic_regex;
    callback_t m_cb;
};

class WsRouter 
{	
public:
	WsRouter() = default;
	WsRouter(const WsRouter& other)
	{
		routes = other.routes;
	}

	WsRouter(WsRouter&& other)
	{
		routes = std::move(other.routes);
	}

    /*! Adds a route to the router
        *
        * \param topic_regex String regex of the url path.
        */
    void register_route(std::string topic_regex, 
            callback_t&& cb);

    /*! Routes based on the path.
        *
        * It will match the path with the registered routes and call the callback
        * to handle the specific request.
        *
        */
    void route_request(json msg);

private:
    std::vector<WsRoute> routes;
};

// Sends a WebSocket message and prints the response
template <typename... Processors>
class WebSocketSession : public std::enable_shared_from_this<WebSocketSession<Processors...>>
{
private:
    tcp::resolver resolver_;
    net::io_context& ioc_;
	ssl::context ssl_ctx_{ssl::context::tls_client};
 	std::optional<ws::stream<ssl::stream<beast::tcp_stream> > > stream_;

    beast::flat_buffer inbuf_;
	std::deque<std::string> outbox_;  
    std::string host_;
	std::string port_;
	std::string target_;

	bool b_private_session_;

	// topics to subscribe
	std::vector<WsTopic> m_topics;
	// router
	WsRouter m_router;

	//status mechine
	WsConnectState m_state{WsConnectState::DISCONNECTED};	
    
	net::steady_timer ping_timer_;
    net::steady_timer retry_timer_;
    std::chrono::steady_clock::time_point last_pong_{};
    std::chrono::seconds backoff_{1};

    std::tuple<std::reference_wrapper<Processors>...> processors_; // tuple of processor references

    using ProcessorVariant = std::variant<Trading::ExchangeProcessor<ExchangeName::OKX>*,
                                      Trading::ExchangeProcessor<ExchangeName::BINANCE>*,
                                      Trading::ExchangeProcessor<ExchangeName::BYBIT>*,
                                      Trading::ExchangeProcessor<ExchangeName::DERIBIT>*>;

    // Runtime dispatch based on ExchangeName (cannot use if constexpr with runtime value)
    // Returns variant holding pointer to appropriate processor type
    ProcessorVariant getProcessor(ExchangeName exchange) noexcept {
        switch (exchange) {
            case ExchangeName::OKX:
                return &std::get<0>(processors_).get();
            case ExchangeName::BINANCE:
                return &std::get<1>(processors_).get();
            case ExchangeName::BYBIT:
                return &std::get<2>(processors_).get();
            case ExchangeName::DERIBIT:
                return &std::get<3>(processors_).get();
            default:
                throw std::runtime_error("Invalid exchange type for processor dispatch");
        }
    }


public:
    // Resolver and socket require an io_context
    explicit WebSocketSession(net::io_context& ioc, bool b_private_session, WsRouter& router, char const* host, 
        std::string target, std::tuple<std::reference_wrapper<Processors>...> processors) :
            resolver_(net::make_strand(ioc)),
            ioc_(ioc),
            //stream_(net::make_strand(ioc), ssl_ctx_),
            ping_timer_(ioc),
            retry_timer_(ioc),
            target_(target),
            host_(host),
            b_private_session_(b_private_session),
            processors_(processors),
            m_router(router)           
    {
		ASN_DEBUG(loggerWebsocketH, "Start a new WebSocketSession");
		m_topics.reserve(MAX_TOPIC_SIZE);

        // 延迟构造
        stream_.emplace(net::make_strand(ioc), ssl_ctx_);

		if(!SSL_set_tlsext_host_name(stream_->next_layer().native_handle(), host))
      		throw beast::system_error(beast::error_code(static_cast<int>(::ERR_get_error()),
                net::error::get_ssl_category()));
        
        // Set the expected hostname in the peer certificate for verification
        //NextLayer表示WebSocket连接使用的下一层流类型，例如TCP套接字或TLS握手后的数据流
        stream_->next_layer().set_verify_callback(ssl::host_name_verification(host));
		
		// 控制帧：捕获 pong
        stream_->control_callback(
		[this](beast::websocket::frame_type kind, beast::string_view){
			if(kind == ws::frame_type::pong){
				last_pong_ = std::chrono::steady_clock::now();
			}
		});
    }

	void addTopic(json&& args) //std::string channel, std::string instId
	{
		std::unordered_map<std::string, std::string> map;
		for (auto& [key, value] : args.items()) 
		{
			//ASN_TRACE(loggerH, "add topic key = " << key << ", value = " << value);
			map[key] = value;
		}	
		m_topics.emplace_back(WsTopic{map, WsTopicStatus::PENDING});
	}

    // Start the asynchronous operation
    void run(char const* host, char const* port)
    {
        // Save these for later
        host_ = host;
		port_ = port;

		set_state(WsConnectState::DNS_RESOLVE);

        // Look up the domain name
        resolver_.async_resolve(
            host,
            port,
            beast::bind_front_handler(
                &WebSocketSession::on_resolve,
                this->shared_from_this()));
    }

	void close() 
	{
		beast::error_code ec;
        ping_timer_.cancel(ec);
        retry_timer_.cancel(ec);
        stream_->next_layer().next_layer().cancel();
    	
        net::post(stream_->get_executor(), [self = this->shared_from_this()]
		{
			beast::error_code ec;
			self->stream_->close(ws::close_code::normal, ec);

			if(ec)
            	return fail(ec, "close");
		});

        set_state(WsConnectState::DISCONNECTED);
  	}

	void subscribePublicTopics()
	{		
        // OKX 要求单报长度 ≤ 4096 bytes，简化：一条条发送（可自行做聚合/分批）
		for (auto& topic : m_topics)
		{
            topic.m_status = WsTopicStatus::PENDING;
            // TODO: combine all the topic who needs to be subscribed together and send one
            /**
             * {
                "op": "subscribe",
                "args": [
                    {"channel": "books5", "instId": "BTC-USDT"},
                    {"channel": "trades", "instId": "BTC-USDT"}
                ]
                }
             */
			send(topic.toSubscribeStr());
		}
	}

private:
    void set_state(WsConnectState s)
    {
        m_state = s;
		ASN_DEBUG(loggerWebsocketH, "[STATE] -> " << state_name(s));
    }

    void on_resolve(beast::error_code ec, tcp::resolver::results_type results)
    {
        if(ec)
            return reconnect("resolve", ec);

        set_state(WsConnectState::TCP_CONNECT);

        // Set a timeout on the operation
        beast::get_lowest_layer(*stream_).expires_after(std::chrono::seconds(30));

        // Make the connection on the IP address we get from a lookup
        beast::get_lowest_layer(*stream_).async_connect(
            results,
            beast::bind_front_handler(
                &WebSocketSession::on_connect,
                this->shared_from_this()));
    }

    void on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type ep)
    {
        if(ec)
            return reconnect("connect", ec);
        
        set_state(WsConnectState::TLS_HANDSHAKE);
 
        // Update the host_ string. This will provide the value of the
        // Host HTTP header during the WebSocket handshake.
        // See https://tools.ietf.org/html/rfc7230#section-5.4
        //host_ += ':' + std::to_string(ep.port());
        
        //beast::get_lowest_layer(stream_).expires_never();

        // Perform the ssl handshake
        stream_->next_layer().async_handshake(ssl::stream_base::client,
            beast::bind_front_handler(
                &WebSocketSession::on_ssl_handshake,
                this->shared_from_this()));
    }

	void on_ssl_handshake(beast::error_code ec)
    {
        if(ec)
            return reconnect("ssl_handshake", ec);

        set_state(WsConnectState::WS_HANDSHAKE);

        // Turn off the timeout on the tcp_stream, because the websocket stream has its own timeout system.
        beast::get_lowest_layer(*stream_).expires_never();

        // Set suggested timeout settings for the websocket
        stream_->set_option(
            ws::stream_base::timeout::suggested(
                beast::role_type::client));

        // Set a decorator to change the User-Agent of the handshake
        stream_->set_option(ws::stream_base::decorator(
            [](ws::request_type& req)
            {
                req.set(http::field::user_agent,
                    std::string(BOOST_BEAST_VERSION_STRING) +
                        " websocket-client-async");
            }));

        // Perform the websocket handshake
        stream_->async_handshake(host_, target_,
            beast::bind_front_handler(
                &WebSocketSession::on_handshake,
                this->shared_from_this()));
    }

    void on_handshake(beast::error_code ec)
    {
        if(ec)
		    return reconnect("ws_handshake", ec);

        if(b_private_session_)
		{
			do_login();  // 登录后再订阅
		}           
		else
		{
			set_state(WsConnectState::SUBSCRIBING);
			subscribePublicTopics();
		}       

		// 2) 启动永久读循环
		do_read();

		// 3) heartbeat
		start_ping_loop();
    }

	void do_read() {
		stream_->async_read(inbuf_,
		    beast::bind_front_handler(&WebSocketSession::on_read, this->shared_from_this()));
	}

	void on_read(beast::error_code ec, std::size_t bytes) 
    {
		if (ec) 
		{
			if(ec == ws::error::closed) return reconnect("closed", ec);
            return reconnect("read", ec);
		}

		boost::ignore_unused(bytes);

		// 处理消息（文本/二进制判断）
		auto s = beast::buffers_to_string(inbuf_.cdata());
        inbuf_.consume(bytes);
		json content = json::parse(s, nullptr, false);
        if(content.is_discarded())
        {
            // 非 JSON 帧（或 pong），忽略
            ASN_TRACE(loggerWebsocketH, "No json frame, maybe pong, ignore");
        }
        else
        {
			//ASN_TRACE(loggerWebsocketH, "recv msg = " << content);
            if (content.contains("event"))
            {
                try
                {
                    auto ev = content["event"];
                    if (ev == "login")
                    {
                        // {"event":"login","code":"0","msg":""}
                        if(content["code"] == "0") on_login_ack_ok();
                        else return reconnect("login-fail", beast::error_code{});
                    }
                    else if (ev=="subscribe")
                    {
                        // {"event":"subscribe","arg":{"channel":"books5","instId":"BTC-USDT-SWAP"}}
                        auto& arg = content["arg"];
                        on_subscribe_ack_ok(std::move(arg));
                    }
                    else if (ev == "error")
                    {
                        ASN_ERROR(loggerWebsocketH, "recv error msg = " << content.dump() << "\n");
                        // 订阅出错可选择只重发该订阅；此处简单化处理为重连
                        return reconnect("server-error-event", beast::error_code{});
                    }
                    else if (ev == "notice")
                    {
                        ASN_ERROR(loggerWebsocketH, "recv OKX notice = " << content.dump() << "\n");
                        // receive notice event from OKX, 用户会在如下场景收到此类信息：Websocket服务升级断线. 在推送服务升级前60秒会推送信息，用户可以重新建立新的连接避免由于断线造成的影响。
                        return reconnect("server-notice-event", beast::error_code{});
                    }
					else if (ev == "channel-conn-count")
					{
                        ASN_TRACE(loggerWebsocketH, "recv channel-conn-count msg = " << content.dump() << "\n");
						// do nothing
					}
                }
                catch(const std::exception& e)
                {
					ASN_ERROR(loggerWebsocketH, "Exception in handling json: " << e.what());
                }                          
            }
            // put data msg into PriceLevelHandler queue or TradeHandler, not directly call the callback
            else if(content.contains("arg") && content.contains("data"))
            {
                //m_router.route_request(content); TODO: move the router to exchange processor

                //auto clock = timer ::TradingClock::getInstance();
                auto curTime = timer::getCurMicroTime();
                auto exchange = ExchangeName::OKX; // TODO: obtain exchange name from msg

                try
                {
                    const auto& arg = content["arg"];
                    const auto channel = arg.value("channel", std::string{});
                    const auto inst_id = arg.value("instId", std::string{});

                    SymbolName symbol = SymbolName::BTC_USDT;
                    if (inst_id == "BTC-USDT")
                    {
                        symbol = SymbolName::BTC_USDT;
                    }
                    else if (inst_id == "BTC-USDT-SWAP")
                    {
                        symbol = SymbolName::BTC_USDT_SWAP;
                    }
                    else
                    {
                        ASN_ERROR(loggerWebsocketH, "Unsupported instId in market data frame: " << inst_id);
                        do_read();
                        return;
                    }

                    const auto& data_arr = content["data"];
                    if (!data_arr.is_array())
                    {
                        ASN_ERROR(loggerWebsocketH, "Invalid market data frame. data is not an array: " << content.dump());
                        do_read();
                        return;
                    }
                                            
                    if (channel == "bbo-tbt" || channel == "books5")
                    {
                        for (const auto& level_frame : data_arr)
                        {
                            if (!level_frame.contains("bids") || !level_frame.contains("asks"))
                            {
                                continue;
                            }

                            const auto& bids = level_frame["bids"];
                            const auto& asks = level_frame["asks"];

                            for (size_t i = 0; i < bids.size(); ++i)
                            {
                                if (!bids[i].is_array() || bids[i].size() < 4)
                                {
                                    continue;
                                }

                                const double price = std::stod(bids[i][0].get<std::string>());
                                const double quantity = std::stod(bids[i][1].get<std::string>());
                                const uint32_t order_count = static_cast<uint32_t>(std::stoul(bids[i][3].get<std::string>()));
                                const uint32_t level = static_cast<uint32_t>(i);

                                std::visit([&](auto* proc) {
                                    proc->writePriceLevelMsg2Queue(exchange, symbol, Side::BUY, price, quantity, order_count, curTime, level);
                                }, getProcessor(exchange));
                            }

                            for (size_t i = 0; i < asks.size(); ++i)
                            {
                                if (!asks[i].is_array() || asks[i].size() < 4)
                                {
                                    continue;
                                }

                                const double price = std::stod(asks[i][0].get<std::string>());
                                const double quantity = std::stod(asks[i][1].get<std::string>());
                                const uint32_t order_count = static_cast<uint32_t>(std::stoul(asks[i][3].get<std::string>()));
                                const uint32_t level = static_cast<uint32_t>(i);

                                std::visit([&](auto* proc) {
                                    proc->writePriceLevelMsg2Queue(exchange, symbol, Side::SELL, price, quantity, order_count, curTime, level);
                                }, getProcessor(exchange));
                            }
                        }

                        //getProcessor(exchange).writePriceLevelMsg2Queue(exchange, symbol, side, price, quantity, order_count, curTime, level);
                    }
                    else if (channel == "trades")
                    {
                        for (const auto& trade_msg : data_arr)
                        {
                            if (!trade_msg.is_object())
                            {
                                continue;
                            }

                            const Side trade_side = trade_msg.value("side", std::string{}) == "buy" ? Side::BUY : Side::SELL;
                            const double price = std::stod(trade_msg.value("px", std::string{"0"}));
                            const double quantity = std::stod(trade_msg.value("sz", std::string{"0"}));
                            const uint32_t count = static_cast<uint32_t>(std::stoul(trade_msg.value("count", std::string{"1"})));
                            const uint64_t seq_id = trade_msg["seqId"].is_number_unsigned() ?
                                trade_msg["seqId"].get<uint64_t>() :
                                std::stoull(trade_msg["seqId"].get<std::string>());
                            const int source = std::stoi(trade_msg.value("source", std::string{"0"}));
                            const auto trade_id = trade_msg.value("tradeId", std::string{});
                            const timer::TimeStamp timestamp = static_cast<timer::TimeStamp>(
                                std::stoull(trade_msg.value("ts", std::string{"0"})));

                            std::visit([&](auto* proc) {
                                proc->writeTradeMsg2Queue(exchange, symbol, count, trade_side, price, quantity, seq_id, source, trade_id.c_str(), timestamp);
                            }, getProcessor(exchange));
                        }
                    }
                }
                catch(const std::exception& e)
                {
                    ASN_ERROR(loggerWebsocketH, "Exception in handling json: " << e.what());
                }
            }       		    
        }
		
		// 继续读
		do_read();
	}

	void do_write() {
		// 只要队列不空，就把队首拿出来写
		stream_->async_write(net::buffer(outbox_.front()),
			beast::bind_front_handler(&WebSocketSession::on_write, this->shared_from_this()));
	}

	void on_write(beast::error_code ec, std::size_t bytes_transferred) {
		if (ec) 
			return reconnect("write", ec);
		
		boost::ignore_unused(bytes_transferred);
		outbox_.pop_front();
		
		if (!outbox_.empty()) 
		{
			do_write(); // 写下一条
		}
	}

	// 线程安全：可在任何线程调用
	void send(std::string msg) 
	{
		net::post(stream_->get_executor(),
		[self = this->shared_from_this(), m = std::move(msg)]() mutable 
		{
			//std::cout << "add msg to send = " << m << "\n";
			self->outbox_.emplace_back(std::move(m));
			if (self->outbox_.size() == 1) 
			{
				self->do_write();
			}
		});
	}

    // ---- 登录（私有连接才需要）----
    void do_login()
    {
		char cwd[1024];
		std::string full_path;
		if (getcwd(cwd, sizeof(cwd)) != nullptr) 
		{			
			// 检查文件是否存在
			struct stat buffer;
			full_path = std::string(cwd) + "/config/.env";
			if (stat(full_path.c_str(), &buffer) != 0) [[unlikely]]
			{
				ASN_ERROR(loggerWebsocketH, "Failed to find env path = " << full_path);
				return;
			}
		}

        // 1. 加载 .env 文件（在程序最开始调用）
        load_env_file(std::move(full_path)); // 默认就是找当前目录的 .env
        
        // 2. 从环境变量中读取值
        const char* api_key_ptr = std::getenv("OKX_API_KEY");
        const char* passphrase_ptr = std::getenv("OKX_PASSPHRASE");
		const char* api_secretkey_ptr = std::getenv("OKX_SECRETKEY");

        // 3. 安全检查（必须做！）
        if (!api_key_ptr || !passphrase_ptr || !api_secretkey_ptr) [[unlikely]]
		{
            ASN_ERROR(loggerWebsocketH,  "错误: 无法从环境变量中读取完整的API凭证, 请检查 .env 文件是否存在且格式正确\n");
            return; 
        }
        
        // 4. 转换成std::string（这样更安全方便）
        std::string api_key(api_key_ptr);
        std::string passphrase(passphrase_ptr);
		std::string api_secretkey(api_secretkey_ptr);
        
        auto authenticator = std::make_unique<WsAuthenticator>(api_key, passphrase, api_secretkey);
        auto msg = authenticator->build_login_message();

        send(msg);
    }

    // 登录/订阅/错误 回执统一在 on_read 里处理
    void on_login_ack_ok()
    {
        set_state(WsConnectState::SUBSCRIBING);
        subscribePublicTopics();
    }

	/**
	 * {"arg":{"channel":"account"},"connId":"27a856ec","event":"subscribe"}
	 */
    void on_subscribe_ack_ok(json&& args)
    {
        for (auto& topic : m_topics)
        {
			for (const auto& [key, value] : topic.args)
			{
				if(args.contains(key) && args[key] != value) // arg value not euqal
				{				
					break;
				}
			}
            //ASN_TRACE(loggerH, "set " << topic << ", status = OK");
			topic.m_status = WsTopicStatus::OK;            
        }

        // 订阅都 OK 就进入 RUNNING
        bool all_ok = true;
        for(const auto& topic : m_topics)
        {
            if (topic.m_status != WsTopicStatus::OK) 
            {
                all_ok=false;
                break;
            }
        }
         
        if(all_ok)
        {
            set_state(WsConnectState::RUNNING);
        }
    }

	// ---- 心跳：连接级 Ping/Pong ----
    void start_ping_loop()
	{
        last_pong_ = std::chrono::steady_clock::now();
        schedule_ping();
    }

    void schedule_ping()
    {
        ping_timer_.expires_after(20s);
        ping_timer_.async_wait([self = this->shared_from_this()](beast::error_code ec)
		{
            if(ec) return; // 可能是 cancel
            // 发 ping
            self->stream_->ping({});
            // 超时检测（10s 无 pong 认为断线）
            auto now = std::chrono::steady_clock::now();
            if(now - self->last_pong_ > 10s)
            {
                self->reconnect("pong-timeout", beast::error_code{});
                return;
            }
            self->schedule_ping();
        });
    }

    // ---- 重连（指数退避 + 完整流程重走）----
    void reconnect(const char* where, beast::error_code ec)
    {
        if(ec)
            std::cerr << "[RECONNECT] " << where << ": " << ec.message() << "\n";
        else
            std::cerr << "[RECONNECT] " << where << "\n";

        // 关闭与清理
        beast::error_code e2;
        ping_timer_.cancel(e2);
        beast::get_lowest_layer(*stream_).cancel();
        stream_->close(ws::close_code::normal, e2); // 忽略错误

        set_state(WsConnectState::DISCONNECTED);

        // 退避
        backoff_ = std::min(backoff_ * 2, 30s);
        retry_timer_.expires_after(backoff_);
        retry_timer_.async_wait([self = this->shared_from_this()](beast::error_code ec2)
		{
            if(ec2) return; // 被取消
            // 重新构造底层流（重要！）
            self->rebuild_stream();
            // 重新发起连接
            self->set_state(WsConnectState::DNS_RESOLVE);
            self->resolver_.async_resolve(
                self->host_, self->port_,
                beast::bind_front_handler(&WebSocketSession::on_resolve, self));
        });
    }

    void rebuild_stream()
	{
        ssl_ctx_ = ssl::context(ssl::context::tls_client);
        stream_.reset();
        stream_.emplace(ws::stream<ssl::stream<beast::tcp_stream>>(net::make_strand(ioc_), ssl_ctx_));

        // SNI 重新设置
        if(!SSL_set_tlsext_host_name(stream_->next_layer().native_handle(), host_.c_str()))
        {
            beast::error_code ec(static_cast<int>(::ERR_get_error()), net::error::get_ssl_category());
            throw beast::system_error{ec};
        }
        // 控制帧回调重新设置
        stream_->control_callback(
            [this](beast::websocket::frame_type kind, beast::string_view)
            {
                if(kind == ws::frame_type::pong){
                    last_pong_ = std::chrono::steady_clock::now();
                }
            });
        stream_->text(true);
        outbox_.clear();
        inbuf_.consume(inbuf_.size());
    }

    bool load_env_file(std::string&& path) 
    {
        std::ifstream file(path);
        if (!file.is_open()) {
            std::cerr << "警告: 找不到 .env 文件，将使用系统环境变量。" << std::endl;
            return false;
        }
        
        std::string line;
        while (std::getline(file, line)) 
		{
            // 跳过空行和注释行
            if (line.empty() || line[0] == '#') continue;
            
            size_t pos = line.find('=');
            if (pos != std::string::npos) 
            {
                std::string key = line.substr(0, pos);
                std::string value = line.substr(pos + 1);

                // 去除可能的首尾空格（更健壮的写法）
                key.erase(0, key.find_first_not_of(" \t"));
                key.erase(key.find_last_not_of(" \t") + 1);
                value.erase(0, value.find_first_not_of(" \t"));
                value.erase(value.find_last_not_of(" \t") + 1);
                
                // 设置为进程的环境变量
    #ifdef _WIN32
                _putenv((key + "=" + value).c_str());
    #else
                setenv(key.c_str(), value.c_str(), 1);
    #endif
            }
        }
        file.close();
        return true;
    }
};

template <typename... Processors>
class AsyncWebsocketClient
{
public:
	explicit AsyncWebsocketClient(net::io_context& ioc, std::string host, std::string port, int numaNode, Processors&... processors) :
		m_ioc(ioc),
		m_host(host),
		m_port(port),
        processors_(std::ref(processors)...),
		bindToNumaNode(numaNode)		
		{
			// m_router.register_route("^books5\\|BTC-USDT$", [this](const json& msg) {handle_books5_BTC_USDT(msg);});
			// m_router.register_route("^trades\\|BTC-USDT$", [this](const json& msg) {handle_trades_BTC_USDT(msg);});
			// m_router.register_route("^bbo-tbt\\|BTC-USDT$", [this](const json& msg) {handle_bbo_tbt_BTC_USDT(msg);});
			
			// m_router.register_route("account", [this](const json& msg) {handle_account_update(msg);});
			// m_router.register_route("positions", [this](const json& msg) {handle_positions_update(msg);});
			// m_router.register_route("balance_and_position", [this](const json& msg) {handle_balance_and_position_update(msg);});
		}
    
    ~AsyncWebsocketClient() 
    {
		stop();

		using namespace std::literals::chrono_literals;
		std::this_thread::sleep_for(5s);
    }
	
    /// Start and stop the market data consumer main thread.
    auto start() 
    {
		m_stop.store(false);
		m_worker_thread = Common::createAndStartThread(bindToNumaNode, "Trading/websocket", [this]() { run(); });
		if (!m_worker_thread)
			ASN_ERROR(loggerWebsocketH, "Failed to start websocket thread");
    }

    auto stop() -> void 
	{
		m_stop.store(true);
		if (m_publicSession)
			m_publicSession->close();
		if (m_privateSession)
			m_privateSession->close();
		
		// 给一点时间让会话优雅关闭（可选）
        std::this_thread::sleep_for(100ms);

		//m_ioc.stop();

		if (m_worker_thread && m_worker_thread->joinable())
			m_worker_thread->join();
    }

	void run()
    {
		m_publicSession = std::make_shared<WebSocketSession<Processors...>>(m_ioc, false, m_router, m_host.c_str(), m_public_target, 
			//std::get<0>(processors_), std::get<1>(processors_), std::get<2>(processors_), std::get<3>(processors_));
            processors_);
		m_publicSession->addTopic({{"channel", "books5"}, {"instId", "BTC-USDT"}});
		m_publicSession->addTopic({{"channel", "trades"}, {"instId", "BTC-USDT"}});
		m_publicSession->addTopic({{"channel", "bbo-tbt"}, {"instId", "BTC-USDT"}}); // only best bid/ask price size, 10ms, no depth structure
		m_publicSession->run(m_host.c_str(), m_port.c_str());

		m_privateSession = std::make_shared<WebSocketSession<Processors...>>(m_ioc, true, m_router, m_host.c_str(), m_private_target,
			//std::get<0>(processors_), std::get<1>(processors_), std::get<2>(processors_), std::get<3>(processors_));
            processors_);

		m_privateSession->addTopic({{"channel", "account"},
					{"extraParams", "{\"updateInterval\":\"0\"}"}});
		m_privateSession->addTopic({{"channel", "positions"},
					{"instType", "ANY"},
					{"extraParams", "{\"updateInterval\":\"0\"}"}});
		m_privateSession->addTopic({{"channel", "balance_and_position"}});
		m_privateSession->run(m_host.c_str(), m_port.c_str());

	    // m_ioc.run(); // block here until m_ioc stops
    }

private:
    //public session callback
    // void handle_books5_BTC_USDT(const json& msg);
	// void handle_trades_BTC_USDT(const json& msg);
	// void handle_bbo_tbt_BTC_USDT(const json& msg);

    // //private session callback
	// void handle_account_update(const json& msg);
	// void handle_positions_update(const json& msg);
	// void handle_balance_and_position_update(const json& msg);

private:
	const std::string m_host{""};
	const std::string m_port{""};

	const std::string m_public_target{"/ws/v5/public?brokerId=9999"};
    const std::string m_private_target{"/ws/v5/private?brokerId=9999"};

	WsRouter m_router;

	std::tuple<std::reference_wrapper<Processors>...> processors_;

    std::shared_ptr<WebSocketSession<Processors...>> m_publicSession;
	std::shared_ptr<WebSocketSession<Processors...>> m_privateSession;

private:
    std::thread* m_worker_thread;
    std::atomic_bool m_stop{false};

	int bindToNumaNode{0};

	// The io_context is required for all I/O
    net::io_context& m_ioc;
};

}
