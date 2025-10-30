#pragma once

#include <functional>
#include <map>
#include <deque>

#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <atomic>
#include <json/json.hpp>

#include "concurrentqueue/concurrentqueue.h"
#include <folly/concurrency/ConcurrentHashMap.h>

#include "common/thread_utils.h"
#include "common/lf_queue.h"
#include "common/macros.h"
#include "common/mcast_socket.h"

#include "types.h"

#include "exchange/market_data/market_update.h"

#include "example/common/root_certificates.hpp"

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/strand.hpp>
#include <cstdlib>


namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace ws = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl;       // from <boost/asio/ssl.hpp>

using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
using json = nlohmann::json;

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
    void (*callback)(json msg);
};

class WsRouter 
{	
public:
	WsRouter(WsRouter&& other)
	{
		routes = std::move(other.routes);
	}
	
    /*! Adds a route to the router
        *
        * \param topic_regex String regex of the url path.
        */
    void register_route(std::string topic_regex, 
            void (*callback)(json msg) );

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


void handle_ws_for_books5(json msg);
//void handle_mqtt_for_task_assign(json msg);
//void handle_mqtt_for_log_saving(json msg);


// Sends a WebSocket message and prints the response
class WebSocketSession : public std::enable_shared_from_this<WebSocketSession>
{
private:
    tcp::resolver resolver_;
    ws::stream<ssl::stream<beast::tcp_stream>> stream_;
    beast::flat_buffer inbuf_;
	std::deque<std::string> outbox_;  
    std::string host_;

	// topics to subscribe
	std::vector<WsTopic> m_topics;
	// router
	WsRouter m_router;

	//status mechine
	std::atomic<WsConnectState> m_state{WsConnectState::DISCONNECTED};	
    

public:
    // Resolver and socket require an io_context
    explicit WebSocketSession(net::io_context& ioc, ssl::context& ctx, WsRouter&& router) :
        resolver_(net::make_strand(ioc)),
        stream_(net::make_strand(ioc), ctx)
		m_router(router)
    {
		m_topics.reserve(MAX_TOPIC_SIZE);
    }

	void addTopic(std::string channel, std::string instId)
	{
		//TODO: determine WsOpType
		WsTopic topic{WsOpType::SUBSCRIBE, channel, instId};
		m_topics.emplace_back(topic);
	}

    // Start the asynchronous operation
    void run(char const* host, char const* port) //char const* text
    {
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

        // Save these for later
        host_ = host;
        //text_ = text;

        // Look up the domain name
        resolver_.async_resolve(
            host,
            port,
            beast::bind_front_handler(
                &WebSocketSession::on_resolve,
                shared_from_this()));
    }

	void subscribeAllTopics()
	{		
		for (const auto& topic : m_topics)
		{
			send(topic.toString());
		}
	}

private:
	void handleStateTransition(WsConnectEvent event);

    void handleConnected(WsConnectEvent event);
    void handleConnecting(WsConnectEvent event);
    void handleDisconnected(WsConnectEvent event);

    void on_resolve(
        beast::error_code ec,
        tcp::resolver::results_type results)
    {
        if(ec)
            return fail(ec, "resolve");

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
            return fail(ec, "connect");

        // Set a timeout on the operation
        beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));
 
        // Update the host_ string. This will provide the value of the
        // Host HTTP header during the WebSocket handshake.
        // See https://tools.ietf.org/html/rfc7230#section-5.4
        host_ += ':' + std::to_string(ep.port());
        
        // Perform the SSL handshake
        stream_.next_layer().async_handshake(
            ssl::stream_base::client,
            beast::bind_front_handler(
                &WebSocketSession::on_ssl_handshake,
                shared_from_this()));
    }

    void on_ssl_handshake(beast::error_code ec)
    {
        if(ec)
            return fail(ec, "ssl_handshake");

        // Turn off the timeout on the tcp_stream, because
        // the websocket stream has its own timeout system.
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
                        " websocket-client-async-ssl");
            }));

        // Perform the websocket handshake
        stream_.async_handshake(host_, "/",
            beast::bind_front_handler(
                &WebSocketSession::on_handshake,
                shared_from_this()));
    }

    void on_handshake(beast::error_code ec)
    {
        if(ec)
            return fail(ec, "handshake");
		
		// 1) 握手成功后先发订阅（例：OKX books5）
		subscribeAllTopics();
		//send(R"({"op":"subscribe","args":[{"channel":"books5","instId":"BTC-USDT-SWAP"}]})");
		//send(R"({"op":"subscribe","args":[{"channel":"trades","instId":"BTC-USDT-SWAP"}]})");

		// 2) 启动永久读循环
		do_read();
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
		// 处理消息（文本/二进制判断）
		auto s = beast::buffers_to_string(inbuf_.cdata());
		// TODO: 解析 JSON，分发到回调/队列
		// std::cout << "recv: " << s << "\n";
		inbuf_.consume(bytes);

		// 继续读
		do_read();
	}

	void do_write() {
		// 只要队列不空，就把队首拿出来写
		stream_.async_write(net::buffer(outbox_.front()),
			beast::bind_front_handler(&WebSocketSession::on_write, shared_from_this()));
	}

	void on_write(beast::error_code ec, std::size_t) {
		if (ec) 
			return fail(ec, "write");
		outbox_.pop_front();
		
		if (!outbox_.empty()) 
		{
			do_write(); // 写下一条
		}
	}

	void close() 
	{
    	net::post(stream_.get_executor(), [self = shared_from_this()]
		{
			beast::error_code ec;
			self->stream_.close(ws::close_code::normal, ec);

			if(ec)
            	return fail(ec, "close");
		});
  	}

	// 线程安全：可在任何线程调用
	void send(std::string msg) 
	{
		net::post(stream_.get_executor(),
		[self = shared_from_this(), m = std::move(msg)]() mutable 
		{
			self->outbox_.emplace_back(std::move(m));
			if (self->outbox_.size() == 1) 
			{
				self->do_write();
			}
		});
	}
};


class AsyncWebsocketClient
{
public:
	explicit AsyncWebsocketClient(const std::string host, const std::string port) :
		m_host(host),
		m_port(port) 
		{
			// Verify the remote server's certificate
			m_ctx.set_verify_mode(ssl::verify_peer);

			// This holds the root certificate used for verification
			load_root_certificates(m_ctx);
		}
	
	void start()
    {
        m_publicSession = std::make_shared<WebSocketSession>(m_ioc, m_ctx);
		m_publicSession->addTopic("books5", "BTC-USDT-SWAP");
		m_publicSession->addTopic("trades", "BTC-USDT-SWAP");
		m_publicSession->run(m_host.c_str(), m_port.c_str());
    }

private:
	const std::string m_host{""};
	const std::string m_port{""};

	// The io_context is required for all I/O
    net::io_context m_ioc;

    // The SSL context is required, and holds certificates
    ssl::context m_ctx{ssl::context::tlsv12_client};

	std::shared_ptr<WebSocketSession> m_publicSession;
	std::shared_ptr<WebSocketSession> m_privateSession;
};


#if 0
class WsClient : public std::enable_shared_from_this<WsClient> {
public:
  WsClient(net::io_context& ioc, std::string host, std::string port, std::string target)
    : host_(std::move(host)), port_(std::move(port)), target_(std::move(target)),
      resolver_(ioc), stream_(net::make_strand(ioc)) {}

  void run() {
    resolver_.async_resolve(host_, port_,
      beast::bind_front_handler(&WsClient::on_resolve, shared_from_this()));
  }

  // 线程安全：可在任何线程调用
  void send(std::string msg) {
    net::post(stream_.get_executor(),
      [self = shared_from_this(), m = std::move(msg)]() mutable {
        self->outbox_.emplace_back(std::move(m));
        if (self->outbox_.size() == 1) {
          self->do_write();
        }
      });
  }

  void close() {
    net::post(stream_.get_executor(),
      [self = shared_from_this()]{
        beast::error_code ec;
        self->stream_.close(ws::close_code::normal, ec);
      });
  }

private:
  void on_resolve(beast::error_code ec, tcp::resolver::results_type results) {
    if (ec) return fail("resolve", ec);
    beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));
    beast::get_lowest_layer(stream_).async_connect(results,
      beast::bind_front_handler(&WsClient::on_connect, shared_from_this()));
  }

  void on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type ep) {
    if (ec) return fail("connect", ec);
    host_ += ":" + std::to_string(ep.port());
    // 关闭 Nagle 降低延迟
    beast::get_lowest_layer(stream_).set_option(tcp::no_delay(true));
    // 合理的超时设置
    stream_.set_option(ws::stream_base::timeout::suggested(ws::role_type::client));
    // 如需压缩：stream_.set_option(ws::permessage_deflate(true));
    stream_.async_handshake(host_, target_,
      beast::bind_front_handler(&WsClient::on_handshake, shared_from_this()));
  }

  void on_handshake(beast::error_code ec) {
    if (ec) return fail("handshake", ec);

    // 1) 握手成功后先发订阅（例：OKX books5）
    send(R"({"op":"subscribe","args":[{"channel":"books5","instId":"BTC-USDT-SWAP"}]})");
    send(R"({"op":"subscribe","args":[{"channel":"trades","instId":"BTC-USDT-SWAP"}]})");

    // 2) 启动永久读循环
    do_read();

    // 3) 你可以在任何时候再次 send(...)，例如定时 ping / 主动下单指令等
  }

  void do_read() {
    stream_.async_read(inbuf_,
      beast::bind_front_handler(&WsClient::on_read, shared_from_this()));
  }

  void on_read(beast::error_code ec, std::size_t bytes) {
    if (ec) {
      if (ec == ws::error::closed) return;
      return fail("read", ec);
    }
    // 处理消息（文本/二进制判断）
    auto s = beast::buffers_to_string(inbuf_.cdata());
    // TODO: 解析 JSON，分发到回调/队列
    // std::cout << "recv: " << s << "\n";
    inbuf_.consume(bytes);

    // 继续读
    do_read();
  }

  void do_write() {
    // 只要队列不空，就把队首拿出来写
    stream_.async_write(net::buffer(outbox_.front()),
      beast::bind_front_handler(&WsClient::on_write, shared_from_this()));
  }

  void on_write(beast::error_code ec, std::size_t) {
    if (ec) return fail("write", ec);
    outbox_.pop_front();
    if (!outbox_.empty()) {
      do_write(); // 写下一条
    }
  }

  static void fail(const char* what, beast::error_code ec) {
    std::cerr << what << ": " << ec.message() << "\n";
  }

  std::string host_, port_, target_;
  tcp::resolver resolver_;
  ws::stream<tcp::socket> stream_;    // 若 wss: 改为 websocket::stream<ssl::stream<tcp::socket>>
  beast::flat_buffer inbuf_;
  std::deque<std::string> outbox_;    // 发送队列（串行写）
};
#endif


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




}
