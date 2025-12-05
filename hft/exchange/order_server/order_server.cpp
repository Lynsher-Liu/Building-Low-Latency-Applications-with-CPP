/*
 * @Author: Lynsher xinyiliu@astri.org
 * @Date: 2025-12-01 13:52:34
 * @LastEditors: Lynsher xinyiliu@astri.org
 * @LastEditTime: 2025-12-05 16:59:43
 * @FilePath: /my_HFT/hft/exchange/order_server/order_server.cpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include "order_server.h"

namespace Exchange {
  OrderServer::OrderServer(ClientRequestLFQueue *client_requests, ClientResponseLFQueue *client_responses, const std::string &iface, int port)
      : iface_(iface), port_(port), outgoing_responses_(client_responses), logger_("exchange_order_server.log"),
        tcp_server_(logger_), fifo_sequencer_(client_requests, &logger_) {
    cid_next_outgoing_seq_num_.fill(1);
    cid_next_exp_seq_num_.fill(1);
    cid_tcp_socket_.fill(nullptr);

    tcp_server_.recv_callback_ = [this](auto socket, auto rx_time) { recvCallback(socket, rx_time); };
    tcp_server_.recv_finished_callback_ = [this]() { recvFinishedCallback(); };
  }

  OrderServer::~OrderServer() {
    stop();

    using namespace std::literals::chrono_literals;
    std::this_thread::sleep_for(1s);
  }

  /// Start and stop the order server main thread.
  auto OrderServer::start() -> void {
    run_ = true;
    tcp_server_.listen(iface_, port_);

    ASSERT_MSG(Common::createAndStartThread(-1, "Exchange/OrderServer", [this]() { run(); }) != nullptr, "Failed to start OrderServer thread.");
  }

  auto OrderServer::stop() -> void {
    run_ = false;
  }
}
