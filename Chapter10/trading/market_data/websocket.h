#pragma once

#include <functional>
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

#include "concurrentqueue/concurrentqueue.h"
#include <folly/concurrency/ConcurrentHashMap.h>

#include "common/thread_utils.h"
#include "common/lf_queue.h"
#include "common/macros.h"
#include "common/mcast_socket.h"

#include "ws_struct.h"

#include "exchange/market_data/market_update.h"

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/algorithm/string.hpp>
#include <cstdlib>


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


namespace Trading 
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
class WebSocketSession : public std::enable_shared_from_this<WebSocketSession>
{
private:
    tcp::resolver resolver_;
    //ws::stream<beast::tcp_stream> stream_;
	ssl::context ssl_ctx_{ssl::context::tls_client};
 	ws::stream<ssl::stream<beast::tcp_stream>> stream_;

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

public:
    // Resolver and socket require an io_context
    explicit WebSocketSession(net::io_context& ioc, bool b_private_session, WsRouter& router, char const* host, string target) :
        resolver_(net::make_strand(ioc)),
        stream_(net::make_strand(ioc), ssl_ctx_),
		ping_timer_(ioc),
        retry_timer_(ioc),
		b_private_session_(b_private_session),
		target_(target),
		m_router(router)
    {
		std::cout << "Start a new WebSocketSession\n";
		m_topics.reserve(MAX_TOPIC_SIZE);

		if(!SSL_set_tlsext_host_name(stream_.next_layer().native_handle(), host))
      		throw beast::system_error(beast::error_code(static_cast<int>(::ERR_get_error()),
                net::error::get_ssl_category()));
        
        // Set the expected hostname in the peer certificate for verification
        stream_.next_layer().set_verify_callback(ssl::host_name_verification(host));
		
		// 控制帧：捕获 pong
        stream_.control_callback(
		[this](beast::websocket::frame_type kind, beast::string_view){
			if(kind == ws::frame_type::pong){
				last_pong_ = std::chrono::steady_clock::now();
			}
		});
    }

	void addTopic(std::string channel, std::string instId)
	{
		m_topics.emplace_back(WsTopic{channel, instId, WsTopicStatus::PENDING});
	}

    // Start the asynchronous operation
    void run(char const* host, char const* port)
    {
// put it in constructor
#if 0 
		// Set SNI Hostname (many hosts need this to handshake successfully)
        if(! SSL_set_tlsext_host_name(stream_.next_layer().native_handle(), host))
        {
            beast::error_code ec{
                static_cast<int>(::ERR_get_error()),
                net::error::get_ssl_category()};
            std::cerr << ec.message() << "\n";
            return;
        }

        // Set the expected hostname in the peer certificate for verification
        stream_.next_layer().set_verify_callback(ssl::host_name_verification(host));
#endif

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
                shared_from_this()));
    }

	void close() 
	{
		beast::error_code ec;
        ping_timer_.cancel(ec);
        retry_timer_.cancel(ec);
        stream_.next_layer().next_layer().cancel();

    	net::post(stream_.get_executor(), [self = shared_from_this()]
		{
			beast::error_code ec;
			self->stream_.close(ws::close_code::normal, ec);

			if(ec)
            	return fail(ec, "close");
		});

        set_state(WsConnectState::DISCONNECTED);
  	}

	void subscribeAllTopics()
	{		
        // OKX 要求单报长度 ≤ 4096 bytes，简化：一条条发送（可自行做聚合/分批）
		for (const auto& topic : m_topics)
		{
			send(topic.toSubscribeStr());
		}
	}

private:
	void handleStateTransition(WsConnectEvent event);

    void handleConnected(WsConnectEvent event);
    void handleConnecting(WsConnectEvent event);
    void handleDisconnected(WsConnectEvent event);

    void set_state(WsConnectState s)
    {
        m_state = s;
        std::cout << "[STATE] -> " << state_name(s) << "\n";
    }

    void on_resolve(beast::error_code ec, tcp::resolver::results_type results)
    {
        if(ec)
            return reconnect("resolve", ec);

        set_state(WsConnectState::TCP_CONNECT);
		std::cout << "enter on_resolve\n";

        // Set a timeout on the operation
        beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));

        // Make the connection on the IP address we get from a lookup
        beast::get_lowest_layer(stream_).async_connect(
            results,
            beast::bind_front_handler(
                &WebSocketSession::on_connect,
                shared_from_this()));
    }

    void on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type ep)
    {
        if(ec)
            return reconnect("connect", ec);
        
        set_state(WsConnectState::TLS_HANDSHAKE);
		std::cout << "enter on_connect\n";
 
        // Update the host_ string. This will provide the value of the
        // Host HTTP header during the WebSocket handshake.
        // See https://tools.ietf.org/html/rfc7230#section-5.4
        //host_ += ':' + std::to_string(ep.port());
        
        //beast::get_lowest_layer(stream_).expires_never();

        // Perform the ssl handshake
        stream_.next_layer().async_handshake(ssl::stream_base::client,
            beast::bind_front_handler(
                &WebSocketSession::on_ssl_handshake,
                shared_from_this()));
    }

	void on_ssl_handshake(beast::error_code ec)
    {
        if(ec)
            return reconnect("ssl_handshake", ec);

        set_state(WsConnectState::WS_HANDSHAKE);

        // Turn off the timeout on the tcp_stream, because the websocket stream has its own timeout system.
        beast::get_lowest_layer(stream_).expires_never();

        // Set suggested timeout settings for the websocket
        stream_.set_option(
            ws::stream_base::timeout::suggested(
                beast::role_type::client));

        // Set a decorator to change the User-Agent of the handshake
        stream_.set_option(ws::stream_base::decorator(
            [](ws::request_type& req)
            {
                req.set(http::field::user_agent,
                    std::string(BOOST_BEAST_VERSION_STRING) +
                        " websocket-client-async");
            }));

        // Perform the websocket handshake
        stream_.async_handshake(host_, target_,
            beast::bind_front_handler(
                &WebSocketSession::on_handshake,
                shared_from_this()));
    }

    void on_handshake(beast::error_code ec)
    {
        if(ec)
		    return reconnect("ws_handshake", ec);

        if(b_private_session_)
            do_login();  // 登录后再订阅
		
		std::cout << "enter on_handshake\n";
        set_state(WsConnectState::SUBSCRIBING);

		// 1) 握手成功后先发订阅（例：OKX books5）
		subscribeAllTopics();
		//send(R"({"op":"subscribe","args":[{"channel":"books5","instId":"BTC-USDT-SWAP"}]})");
		//send(R"({"op":"subscribe","args":[{"channel":"trades","instId":"BTC-USDT-SWAP"}]})");

		// 2) 启动永久读循环
		do_read();

		// 3) heartbeat
		start_ping_loop();
    }

	void do_read() {
		stream_.async_read(inbuf_,
		beast::bind_front_handler(&WebSocketSession::on_read, shared_from_this()));
	}

	void on_read(beast::error_code ec, std::size_t bytes) {
		if (ec) 
		{
			if (ec == ws::error::closed) 
				return;
			return fail(ec, "read");
		}

		boost::ignore_unused(bytes);

		// 处理消息（文本/二进制判断）
		auto s = beast::buffers_to_string(inbuf_.cdata());
		json content = json::parse(s);

		m_router.route_request(content);
		inbuf_.consume(bytes);

		// 继续读
		do_read();
	}

	void do_write() {
		// 只要队列不空，就把队首拿出来写
		stream_.async_write(net::buffer(outbox_.front()),
			beast::bind_front_handler(&WebSocketSession::on_write, shared_from_this()));
	}

	void on_write(beast::error_code ec, std::size_t bytes_transferred) {
		if (ec) 
			return fail(ec, "write");
		
		std::cout << "enter on_write\n";
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
		net::post(stream_.get_executor(),
		[self = shared_from_this(), m = std::move(msg)]() mutable 
		{
			std::cout << "add msg to send = " << m << "\n";
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
        // 这里只给出骨架：OKX 需要 timestamp + sign (HMAC SHA256 base64)
        // 文档：https://www.okx.com/docs-v5/zh/#websocket-login
        // 你需要自己实现 sign = HMAC_SHA256( prehash, secret_key )→ base64

        json j;
        j["op"] = "login";
        j["args"] = json::array();
        json arg;
        arg["apiKey"]     = params_.api_key;
        arg["passphrase"] = params_.passphrase;

        // 示例：用秒级时间戳；实际按文档要求
        auto ts = std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
                 std::chrono::system_clock::now().time_since_epoch()).count());
        arg["timestamp"]  = ts;
        arg["sign"]       = generateSign(); // TODO: 生成签名

        j["args"].push_back(arg);

        write(j.dump());
    }

    std::string generateSign()
    {

    }

    // 登录/订阅/错误 回执统一在 on_read 里处理
    void on_login_ack_ok()
    {
        set_state(WsConnectState::SUBSCRIBING);
        send_all_subs();
    }

    void on_subscribe_ack_ok(const std::string& channel, const std::string& instId){
        for(auto& s : subs_){
            if(s.channel==channel && s.instId==instId){
                s.status = Subscription::Status::Ok;
                break;
            }
        }
        // 订阅都 OK 就进入 RUNNING
        bool all_ok = true;
        for(auto& s : subs_) if(s.status!=Subscription::Status::Ok) {all_ok=false;break;}
        if(all_ok){
            set_state(ConnState::RUNNING);
        }
    }

	// ---- 心跳：连接级 Ping/Pong ----
    void start_ping_loop()
	{
        last_pong_ = std::chrono::steady_clock::now();
        schedule_ping();
    }

    void schedule_ping(){
        ping_timer_.expires_after(20s);
        ping_timer_.async_wait([self=shared_from_this()](beast::error_code ec)
		{
            if(ec) return; // 可能是 cancel
            // 发 ping
            self->stream_.ping({});
            // 超时检测（10s 无 pong 认为断线）
            auto now = std::chrono::steady_clock::now();
            if(now - self->last_pong_ > 10s){
                self->reconnect("pong-timeout", beast::error_code{});
                return;
            }
            self->schedule_ping();
        });
    }

    // ---- 重连（指数退避 + 完整流程重走）----
    void reconnect(const char* where, beast::error_code ec){
        if(ec)
            std::cerr << "[RECONNECT] " << where << ": " << ec.message() << "\n";
        else
            std::cerr << "[RECONNECT] " << where << "\n";

        // 关闭与清理
        beast::error_code e2;
        ping_timer_.cancel(e2);
        beast::get_lowest_layer(stream_).cancel();
        stream_.close(ws::close_code::normal, e2); // 忽略错误

        set_state(ConnState::DISCONNECTED);

        // 退避
        backoff_ = std::min(backoff_ * 2, 30s);
        retry_timer_.expires_after(backoff_);
        retry_timer_.async_wait([self=shared_from_this()](beast::error_code ec2)
		{
            if(ec2) return; // 被取消
            // 重新构造底层流（重要！）
            self->rebuild_stream();
            // 重新发起连接
            self->set_state(ConnState::DNS_RESOLVE);
            self->resolver_.async_resolve(
                host_, port,
                beast::bind_front_handler(&WebSocketSession::on_resolve, self));
        });
    }

    void rebuild_stream()
	{
        ssl_ctx_ = ssl::context(ssl::context::tls_client);
        stream_ = ws::stream<beast::ssl_stream<beast::tcp_stream>>(net::make_strand(ioc_), ssl_ctx_);
        // SNI 重新设置
        if(!SSL_set_tlsext_host_name(ws_.next_layer().native_handle(), params_.host.c_str())){
            beast::error_code ec(static_cast<int>(::ERR_get_error()), net::error::get_ssl_category());
            throw beast::system_error{ec};
        }
        // 控制帧回调重新设置
        ws_.control_callback(
            [this](beast::websocket::frame_type kind, beast::string_view){
                if(kind == ws::frame_type::pong){
                    last_pong_ = std::chrono::steady_clock::now();
                }
            });
        ws_.text(true);
        outbox_.clear();
        inbuf_.consume(inbuf_.size());
        // 重连后需要重新订阅，send_all_subs() 会统一处理
    }

    // ---- 工具 ----
    static std::string json_to_str(const json& j){
        return j.dump();
    }
};


class AsyncWebsocketClient
{
public:
	explicit AsyncWebsocketClient(std::string host, std::string port) :
		m_host(host),
		m_port(port) 
		{
			m_router.register_route("^books5\\|BTC-USDT-SPOT$", [this](const json& msg) {handle_books5_BTC_USDT_SPOT(msg);});
			m_router.register_route("^trades\\|BTC-USDT-SPOT$", [this](const json& msg) {handle_trades_BTC_USDT_SPOT(msg);});
			m_router.register_route("^bbo-tbt\\|BTC-USDT-SPOT$", [this](const json& msg) {handle_bbo_tbt_BTC_USDT_SPOT(msg);});
			m_router.register_route("subscribe", [this](const json& msg) {handle_subscribe_success(msg);});
			m_router.register_route("error", [this](const json& msg) {handle_subscribe_error(msg);});
		}
	
	void start()
    {
      m_publicSession = std::make_shared<WebSocketSession>(m_ioc, m_router, m_public_target);
      m_publicSession->addTopic("books5", "BTC-USDT-SPOT");
      m_publicSession->addTopic("trades", "BTC-USDT-SPOT");
	  m_publicSession->addTopic("bbo-tbt", "BTC-USDT-SPOT"); // only best bid/ask price size, 10ms, no depth structure
	  //m_publicSession->addTopic("books-l2-tbt", "BTC-USDT-SWAP"); // multiple level price to fully reconstruct L2 orderbook
      m_publicSession->run(m_host.c_str(), m_port.c_str());

		m_ioc.run();
    }

private:
    void handle_books5_BTC_USDT_SPOT(const json& msg);
	void handle_trades_BTC_USDT_SSPOT(const json& msg);
	void handle_bbo_tbt_BTC_USDT_SPOT(const json& msg);

	void handle_subscribe_success(const json& msg);
	void handle_subscribe_error(const json& msg);


private:
	std::string m_host{""};
	std::string m_port{""};

	// The io_context is required for all I/O
    net::io_context m_ioc;

	std::shared_ptr<WebSocketSession> m_publicSession;
	std::shared_ptr<WebSocketSession> m_privateSession;

	std::string m_public_target{"/ws/v5/public?brokerId=9999"};

	WsRouter m_router;
};


#if 0
int test() {
  net::io_context ioc;
  auto c = std::make_shared<WsClient>(ioc,
      "wspap.okx.com", "8443", "/ws/v5/public?brokerId=9999");
  c->run();

  // 示例：后台线程/定时器里随时 send（这里简单用 post）
  net::steady_timer t{ioc, std::chrono::seconds(5)};
  t.async_wait([c](auto){ c->send(R"({"op":"ping"})"); });

  ioc.run();
}
#endif


}
