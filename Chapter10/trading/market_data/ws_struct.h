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

	/// Risk configuration containing limits on risk parameters for the RiskManager.
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

	enum WsConnectState {CONNECTED = 0, CONNECTING = 1, DISCONNECTED = 2};
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

