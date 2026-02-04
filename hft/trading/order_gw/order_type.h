/*
 * @Author: Lynsher xinyiliu@astri.org
 * @Date: 2025-12-01 13:52:35
 * @LastEditors: Lynsher xinyiliu@astri.org
 * @LastEditTime: 2026-02-03 22:46:06
 * @FilePath: /my_HFT/hft/trading/order_gw/order_gateway.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#pragma once

#include <functional>
#include <string>
#include <cstdint>
#include <chrono>

#include "common/thread_utils.h"
#include "common/macros.h"

#include "exchange/order_server/client_request.h"
#include "exchange/order_server/client_response.h"

namespace Order 
{
	// 订单类型
	enum class OrderType {
		LIMIT,      // 限价单
		MARKET,     // 市价单
		POST_ONLY   // 只做Maker
	};

	// 订单状态
	enum class OrderStatus {
		PENDING,    // 待发送
		OPEN,       // 已接受
		PARTIAL,    // 部分成交
		FILLED,     // 完全成交
		CANCELLED,  // 已取消
		REJECTED    // 被拒绝
	};
}
