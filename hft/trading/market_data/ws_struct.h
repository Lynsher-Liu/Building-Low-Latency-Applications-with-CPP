/*
 * @Author: Lynsher xinyiliu@astri.org
 * @Date: 2025-12-01 13:52:35
 * @LastEditors: Lynsher xinyiliu@astri.org
 * @LastEditTime: 2025-12-15 11:34:47
 * @FilePath: /my_HFT/Chapter10/trading/market_data/ws_struct.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#pragma once

#include <cstdint>
#include <limits>
#include <sstream>
#include <array>
#include <unordered_map>

#include "common/macros.h"
#include "common/timer.h"
#include <json/json.hpp>

#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include <iomanip>
#include <sstream>

#define MAX_TOPIC_SIZE 8

using json = nlohmann::json;

namespace Common 
{
	/// Type of trading algorithm.
	enum class WsTopicStatus : int8_t {
		PENDING = 0,
		OK = 1,		
		ERROR = 2
	};

	//NOTICE = 2, //TODO: 用户会在如下场景收到此类信息：Websocket服务升级断线
				//在推送服务升级前60秒会推送信息，告知用户WebSocket服务即将升级。用户可以重新建立新的连接避免由于断线造成的影响。

	inline auto WsTopicStatusToString(WsTopicStatus type) -> std::string 
	{
		switch (type) 
		{
			case WsTopicStatus::PENDING:
				return "PENDING";
			case WsTopicStatus::OK:
				return "OK";
			case WsTopicStatus::ERROR:
				return "ERROR";
		}
		return "UNKNOWN";
	}

	struct WsTopic 
	{		
		//std::string m_channel;
		//std::string m_instId;
		std::unordered_map<std::string, std::string> args;
		WsTopicStatus m_status{WsTopicStatus::PENDING};

		auto toSubscribeStr() const 
		{
			json topicJson;
			//std::stringstream ss;

			// demo
			//send(R"({"op":"subscribe","args":[{"channel":"books5","instId":"BTC-USDT-SWAP"}]})");

			topicJson["op"] = "subscribe";
			json arg;
			for (const auto& [key, value] : args)
			{
				arg[key] = value;
			}
			//arg["channel"] = m_channel;
			//arg["instId"] = m_instId;
			topicJson["args"].push_back(arg);

			return topicJson.dump();
		}
	};

	inline std::ostream& operator<<(std::ostream& os, const WsTopic& topic)
	{
		os << "topic:\n";
		for (const auto& [key, value] : topic.args)
		{
			os << key << " = " << value << ",\n";
		}
		return os;
	}

	enum WsConnectState {
		DISCONNECTED,
		DNS_RESOLVE,
		TCP_CONNECT,
		TLS_HANDSHAKE,
		WS_HANDSHAKE,
		LOGGED_IN,     // 私有连接用
		SUBSCRIBING,
		RUNNING
	}; //CONNECTED = 0, CONNECTING = 1, DISCONNECTED = 2

	inline const char* state_name(WsConnectState s)
	{
        switch(s)
		{
            case WsConnectState::DISCONNECTED:  return "DISCONNECTED";
            case WsConnectState::DNS_RESOLVE:   return "DNS_RESOLVE";
            case WsConnectState::TCP_CONNECT:   return "TCP_CONNECT";
            case WsConnectState::TLS_HANDSHAKE: return "TLS_HANDSHAKE";
            case WsConnectState::WS_HANDSHAKE:  return "WS_HANDSHAKE";
            case WsConnectState::LOGGED_IN:     return "LOGGED_IN";
            case WsConnectState::SUBSCRIBING:   return "SUBSCRIBING";
            case WsConnectState::RUNNING:       return "RUNNING";
        }
        return "?";
    }

	/**
     * 1. **apiKey**: 调用API的唯一标识。需要用户手动设置一个 
        2. **passphrase**: APIKey的密码 
        3. **timestamp**:Unix Epoch 时间戳，单位为秒，如 1704876947 
        4. **sign**:签名字符串，签名算法如下：
            1. 先将`timestamp` 、 `method` 、`requestPath` 进行字符串拼接，再使用HMAC SHA256方法将拼接后的字符串和SecretKey加密，然后进行Base64编码
                1. **SecretKey:**用户申请APIKey时所生成的安全密钥，如：22582BD0CFF14C41EDBF1AB98506286D
                2. **其中 timestamp 示例**:const timestamp = '' + Date.now() / 1,000
                3. **其中 sign 示例**: sign=CryptoJS.enc.Base64.stringify(CryptoJS.HmacSHA256(timestamp +'GET'+ '/users/self/verify', secret))
                4. **method** 总是 'GET'
                5. **requestPath** 总是 '/users/self/verify'
        5. 请求在时间戳之后30秒会失效，如果您的服务器时间和API服务器时间有偏差，推荐使用 REST API查询API服务器的时间，然后设置时间戳
     */
	class WsAuthenticator 
	{
	private:
		std::string api_key_;
		std::string passphrase_;
		std::string secretkey_;
		
	public:
		WsAuthenticator(const std::string& api_key,
						const std::string& passphrase,
						const std::string& secretkey)
			: api_key_(api_key), 
				passphrase_(passphrase),
				secretkey_(secretkey) {}		
		
		// 生成HMAC SHA256签名并Base64编码
		std::string generate_signature(const uint64_t& timestamp) 
		{
			// 1. 拼接字符串: timestamp + "GET" + "/users/self/verify"
			std::string message = std::to_string(timestamp) + "GET" + "/users/self/verify";
			
			// 2. 计算HMAC SHA256
			unsigned char hash[EVP_MAX_MD_SIZE];
			unsigned int hash_len;
			
			HMAC(EVP_sha256(),
				secretkey_.c_str(), static_cast<int>(secretkey_.length()),
				reinterpret_cast<const unsigned char*>(message.c_str()),
				static_cast<int>(message.length()),
				hash, &hash_len);
			
			// 3. Base64编码
			BIO *bmem, *b64;
			BUF_MEM *bptr;
			
			b64 = BIO_new(BIO_f_base64());
			bmem = BIO_new(BIO_s_mem());
			b64 = BIO_push(b64, bmem);
			
			BIO_write(b64, hash, hash_len);
			BIO_flush(b64);
			BIO_get_mem_ptr(b64, &bptr);
			
			std::string signature(bptr->data, bptr->length - 1); // 去掉换行符
			
			BIO_free_all(b64);
			
			return signature;
		}
		
		// 构建登录JSON消息
		std::string build_login_message() 
		{
			auto clock = timer::TradingClock::getInstance();
			auto timestamp = clock->getUnixEpochTime();
			std::string signature = generate_signature(timestamp);
			
			std::ostringstream oss;
			oss << "{"
				<< "\"op\":\"login\","
				<< "\"args\":[{"
				<< "\"apiKey\":\"" << api_key_ << "\","
				<< "\"passphrase\":\"" << passphrase_ << "\","
				<< "\"timestamp\":\"" << timestamp << "\","
				<< "\"sign\":\"" << signature << "\""
				<< "}]"
				<< "}";
			
			return oss.str();
		}
	};
}

