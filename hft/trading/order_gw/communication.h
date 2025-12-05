#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/bind/bind.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <json/json.hpp>
#include <regex>
#include "common/AsnLog.h"
#include "common/singleton.h"

using namespace std;
using json = nlohmann::json;
using tcp = boost::asio::ip::tcp;
namespace http = boost::beast::http;
using namespace boost::placeholders;

static AsnLoggerPtr loggerH_communication = ASN_GETLOGGER("planning.communication_h");

#define IO_SERVICE_THREAD_NUM 2

enum HttpRequestType {GET, POST};


/*
    sync http client for restful API
*/
class SyncHttpClient : public Singleton<SyncHttpClient>
{
private:
    string host;
    string port;

    SyncHttpClient() = default;
    SyncHttpClient(string host_addr, string port_num): 
        host(host_addr), 
        port(port_num) {}; 
    
public:    
    static shared_ptr<SyncHttpClient> getInstance();   
    string make_request(string requestHeader, string requestBody);
    string request(string request);

    friend class Singleton<SyncHttpClient>;
};



/*
    async http client for restful API
*/

// 定义回调类型，接收 HTTP 响应
using callback_t = std::function<void(http::response<http::string_body>&)>;

class AsyncHttpRequest : public std::enable_shared_from_this<AsyncHttpRequest> 
{
public:
    AsyncHttpRequest(boost::asio::io_service& io_service) : 
        socket_(io_service), 
        resolver_(io_service), 
        retry_timer_(io_service),
        seq_index(seq_num++)
    {
        ASN_TRACE(loggerH_communication, "A new HttpRequest" );
    }

    ~AsyncHttpRequest() = default;

    // suport GET and POST
    void request(const string& host, const string& port, const string& url, HttpRequestType method, 
            string postBody = "", callback_t&& cb = callback_t(), int version = 11)
    {
        if (host.empty()) 
        {
            ASN_ERROR(loggerH_communication, "Host cannot be empty" );
            return;
        }
        if (port.empty()) 
        {
            ASN_ERROR(loggerH_communication, "Port cannot be empty" );
            return;
        }
        if (url.empty() || url[0] != '/')
        {
            ASN_ERROR(loggerH_communication, "url cannot be empty, or must start with '/'" );
            return;
        }

        //cb_ = move(cb);
        cb_ = cb ? std::move(cb) : [](auto&){}; 
        req_.version(version);
        req_.set(http::field::host, host);
        req_.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

        switch (method) 
        {
        case HttpRequestType::GET:
        {
            req_.method(http::verb::get);
            req_.target(url);
            break;
        }           
        case HttpRequestType::POST:
        {
            req_.method(http::verb::post);
            req_.target(url);
            req_.set(http::field::content_type, "application/json");
            req_.body() = postBody; // 设置 POST 请求的消息体
            req_.prepare_payload(); // 确保 Content-Length 被正确设置
            break;
        }            
        default:
        {
            ASN_ERROR(loggerH_communication, "Unsupported HTTP method");
        }
        }

        // 异步解析域名
        // resolver_.async_resolve(
        //     host, port,
        //     std::bind(&AsyncHttpRequest::on_resolve, shared_from_this(),
        //               std::placeholders::_1, std::placeholders::_2));
        resolver_.async_resolve(
            host, port,
            [this, self = shared_from_this(), host, port](
                boost::system::error_code ec,
                tcp::resolver::results_type results
            ) 
            {
                on_resolve(ec, results, host, port);  // 传递 host 和 port
            }
        );
    }

private:
    void on_resolve(boost::system::error_code ec, tcp::resolver::results_type results, const std::string& host, const std::string& port) 
    {
        if (ec) 
        {
            ASN_ERROR(loggerH_communication, "Resolve error: "<< ec.message() << ", index = " << seq_index);
            start_retry("resolve", host, port);
            return;
        }

        // 保存解析结果
        saved_resolve_results_ = results;

        // 异步建立连接
        boost::asio::async_connect(
            socket_, results.begin(), results.end(),
            std::bind(&AsyncHttpRequest::on_connect, shared_from_this(),
                      std::placeholders::_1));
    }

    void on_connect(boost::system::error_code ec) {
        if (ec) 
        {
            ASN_ERROR(loggerH_communication, "Connect error: "<< ec.message() << ", index = " << seq_index);
            start_retry("connect");
            return;
        }

        // 异步发送 HTTP 请求
        http::async_write(
            socket_, req_,
            std::bind(&AsyncHttpRequest::on_write, shared_from_this(),
                      std::placeholders::_1, std::placeholders::_2));
    }

    void on_write(boost::system::error_code ec, std::size_t bytes_transferred) {
        boost::ignore_unused(bytes_transferred);

        if (ec) {
            ASN_ERROR(loggerH_communication, "Write error: "<< ec.message() << ", index = " << seq_index);
            start_retry("write");
            return;
        }

        // 异步读取 HTTP 响应
        http::async_read(
            socket_, buffer_, res_,
            std::bind(&AsyncHttpRequest::on_read, shared_from_this(),
                      std::placeholders::_1, std::placeholders::_2));
    }

    void on_read(boost::system::error_code ec, std::size_t bytes_transferred) {
        boost::ignore_unused(bytes_transferred);

        if (ec) 
        {
            if (ec != boost::asio::error::eof) {
                ASN_ERROR(loggerH_communication, "Read error: "<< ec.message());
            }
            return;
        }

        if (res_.result_int() >= 400) {
            ASN_ERROR(loggerH_communication, "Server returned error HTTP status: " << res_.result_int());
            return;
        }
        
        // 调用回调函数，传递响应数据
        if (cb_) {
            cb_(res_); // execute callback
        }

        // 关闭连接
        socket_.shutdown(tcp::socket::shutdown_both, ec);
        if (ec && ec != boost::asio::error::not_connected) 
        {
            ASN_ERROR(loggerH_communication, "Shutdown error: "<< ec.message());
        }
    }

    void start_retry(const std::string& step, const std::string& host = "", const std::string& port = "") 
    {
        if (current_retry_ >= max_retries_) {
            ASN_ERROR(loggerH_communication, "Max retries exceeded, stop retry");
            return; // 彻底失败
        }
    
        // 指数退避：1s, 2s, 4s...
        auto delay = std::chrono::seconds(1 << current_retry_);
        retry_timer_.expires_after(delay);        

        retry_timer_.async_wait(
            [this, self = shared_from_this(), step, host, port](boost::system::error_code ec) 
            {
                if (!ec) 
                {
                    current_retry_++;  
                    ASN_DEBUG(loggerH_communication, "start retry for " << step << ", current_retry_ = " << current_retry_ << ", index = " << seq_index);                  
                    if (step == "resolve") 
                    {                        
                        resolver_.async_resolve(
                            host, port,
                            [this, self = shared_from_this(), host, port](auto ec, auto results) {
                                on_resolve(ec, results, host, port);
                            });
                    } 
                    else if (step == "connect") 
                    {
                        boost::asio::async_connect(
                            socket_, saved_resolve_results_.begin(), saved_resolve_results_.end(),
                            [this, self = shared_from_this()](auto ec, const auto& /*endpoint*/) {
                                on_connect(ec);
                            });
                    } 
                    else if (step == "write") {
                        http::async_write(
                            socket_, req_,
                            [this, self = shared_from_this()](auto ec, auto bytes) {
                                on_write(ec, bytes);
                            });
                    }
                }
            });
    }

    inline static size_t seq_num;
    size_t seq_index;

    tcp::socket socket_;
    tcp::resolver resolver_;
    boost::beast::flat_buffer buffer_; // (Must persist between reads)
    http::request<http::string_body> req_;
    http::response<http::string_body> res_;
    callback_t cb_;

    const int max_retries_ = 10;      // 最大重试次数
    int current_retry_ = 0;    // 当前重试次数
    boost::asio::steady_timer retry_timer_;  // 重试定时器
    tcp::resolver::results_type saved_resolve_results_; // 保存解析结果
};

class AsyncHttpClient : public Singleton<AsyncHttpClient>
{
public: 
    static shared_ptr<AsyncHttpClient> getInstance();  // overwrite

    void request(const string& host, const string& port, const string& url, HttpRequestType method, 
        string postBody = "", callback_t cb = callback_t(), int version = 11)
    {
        std::make_shared<AsyncHttpRequest>(io_service_)->request(host, port, url, method, postBody, move(cb), version);
    }  

private:
    AsyncHttpClient(boost::asio::io_service& io_service): io_service_(io_service) {}; 
    friend class Singleton<AsyncHttpClient>;
    boost::asio::io_service& io_service_;  
};


 