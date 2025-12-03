/*
 * @Author: Lynsher xinyiliu@astri.org
 * @Date: 2025-12-01 13:52:35
 * @LastEditors: Lynsher xinyiliu@astri.org
 * @LastEditTime: 2025-12-03 17:20:10
 * @FilePath: /my_HFT/Chapter10/trading/market_data/ws_struct.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#pragma once

#include <cstdint>
#include <limits>
#include <sstream>
#include <array>

#include "common/macros.h"
#include <json/json.hpp>

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
		std::string m_channel;
		std::string m_instId;
		WsTopicStatus m_status{WsTopicStatus::PENDING};

		auto toSubscribeStr() const 
		{
			json topicJson;
			//std::stringstream ss;

			// demo
			//send(R"({"op":"subscribe","args":[{"channel":"books5","instId":"BTC-USDT-SWAP"}]})");

			topicJson["op"] = "subscribe";
			json arg;
			arg["channel"] = m_channel;
			arg["instId"] = m_instId;
			topicJson["args"].push_back(arg);

			return topicJson.dump();
		}
	};

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

	const char* state_name(WsConnectState s)
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

	enum WsConnectEvent {START_CONNECT = 0, CONNECT_SUCCESS = 1, CONNECT_FAILED = 2, CONNECT_LOST = 3};
	/**
	 * state machine diagram:
	 * 
	 * DISCONNECTED -> CONNECTING: START_CONNECT
	 * CONNECTING   -> CONNECTED:  CONNECT_SUCCESS
	 * CONNECTING   -> DISCONNECTED: CONNECT_FAILED
	 * CONNECTED    -> DISCONNECTED: CONNECT_LOST
	 */

}

