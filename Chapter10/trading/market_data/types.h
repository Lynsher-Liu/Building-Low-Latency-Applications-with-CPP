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
	constexpr size_t ME_MAX_TICKERS = 8;


	/// Type of trading algorithm.
	enum class WsOpType : int8_t {
		SUBSCRIBE = 0,
		UNSUBSCRIBE = 1
	};

	inline auto wsOpTypeToString(WsOpType type) -> std::string {
		switch (type) {
		case WsOpType::SUBSCRIBE:
			return "subscribe";
		case WsOpType::UNSUBSCRIBE:
			return "unsubscribe";
		}

		return "UNKNOWN";
	}

	/// Risk configuration containing limits on risk parameters for the RiskManager.
	struct WsTopic 
	{
		WsOpType m_opType;
		std::string m_channel;
		std::string m_instId;

		auto toString() const {
			json topicJson;
			std::stringstream ss;

			// demo
			//send(R"({"op":"subscribe","args":[{"channel":"books5","instId":"BTC-USDT-SWAP"}]})");

			topicJson["op"] = wsOpTypeToString(m_opType);
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
