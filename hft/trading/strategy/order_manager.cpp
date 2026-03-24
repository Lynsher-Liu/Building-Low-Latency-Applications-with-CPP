/*
 * @Author: Lynsher xinyiliu@astri.org
 * @Date: 2025-12-01 13:52:35
 * @LastEditors: Lynsher xinyiliu@astri.org
 * @LastEditTime: 2026-03-24 16:28:28
 * @FilePath: /my_HFT/hft/trading/strategy/order_manager.cpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include "order_manager.h"
#include "trade_engine.h"

namespace Trading {
  /// Send a new order with specified attribute, and update the OMOrder object passed here.
  auto OrderManager::newOrder(OMOrder *order, TickerId ticker_id, Price price, Side side, Qty qty) noexcept -> void {
    const Exchange::MEClientRequest new_request{Exchange::ClientRequestType::NEW, trade_engine_->clientId(), ticker_id,
                                                next_order_id_, side, price, qty};
    trade_engine_->sendClientRequest(&new_request);

    *order = {ticker_id, next_order_id_, side, price, qty, OMOrderState::PENDING_NEW};
    ++next_order_id_;

    // logger_->log("%:% %() % Sent new order % for %\n", __FILE__, __LINE__, __FUNCTION__,
    //              Common::getCurrentTimeStr(&time_str_),
    //              new_request.toString().c_str(), order->toString().c_str());
  }

  /// Send a cancel for the specified order, and update the OMOrder object passed here.
  auto OrderManager::cancelOrder(OMOrder *order) noexcept -> void {
    const Exchange::MEClientRequest cancel_request{Exchange::ClientRequestType::CANCEL, trade_engine_->clientId(),
                                                   order->ticker_id_, order->order_id_, order->side_, order->price_,
                                                   order->qty_};
    trade_engine_->sendClientRequest(&cancel_request);

    order->order_state_ = OMOrderState::PENDING_CANCEL;

    // logger_->log("%:% %() % Sent cancel % for %\n", __FILE__, __LINE__, __FUNCTION__,
    //              Common::getCurrentTimeStr(&time_str_),
    //              cancel_request.toString().c_str(), order->toString().c_str());
  }
}
