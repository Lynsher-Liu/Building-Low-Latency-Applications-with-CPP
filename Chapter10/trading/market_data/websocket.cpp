#include "websocket.h"
#include <regex>


using json = nlohmann::json;
using namespace std;
namespace Trading 
{

void WsRouter::register_route(string topic_regex, 
		callback_t&& cb) 
{
    WsRoute route;
    route.topic_regex = topic_regex;
    route.m_cb = std::move(cb);
    routes.push_back(route);
}


void WsRouter::route_request(json msg)
{
    for (auto& r : routes) 
	{
        // match mqtt msg topic with route regex
        regex reg {r.topic_regex};
        smatch match;

        //{"arg":{"channel":"trades","instId":"BTC-USDT-SWAP"},"connId":"f5bee808","event":"subscribe"}
        //{"arg":{"channel":"books5","instId":"BTC-USDT-SWAP"},"data":[{"asks":[["103259.9","885.31","0","2"],["103261.1","0.83","0","1"],["103261.3","0.02","0","2"],["103261.4","0.01","0","1"],["103262","19.28","0","2"]],"bids":[["103250","21871.09","0","2"],["103218.9","885.4","0","1"],["103217.1","1.95","0","1"],["103216.5","1.96","0","1"],["103215","0.05","0","1"]],"instId":"BTC-USDT-SWAP","ts":"1762700094902","seqId":178414521}]}
        std::string channel = msg["arg"]["channel"];
        std::string instId = msg["arg"]["instId"];

        // handle events individually
        if (msg.contains("event"))
        {
            string event = msg["event"];
            if (std::regex_search(event, match, reg)) 
            {
                r.m_cb(msg);
                break;
            }
        }
		else if (msg.contains("data"))
        {
            std::string uid = channel + "|" + instId;
            if (std::regex_search(uid, match, reg)) 
            {
                r.m_cb(msg);
                break;
            }
        }
    }
}

/**
 * @brief handle depth data of top 5 bid/ask
 * 数量: 合并了该价位所有挂单的总量
 * 档位: 该档位的等待成交的挂单数量, 有时代表“由流动性提供商提供的数量”
 * order数量: 这个价格档位是由n个独立的order组成的
 */
void AsyncWebsocketClient::handle_books5_BTC_USDT_SPOT(const json& msg)
{
	std::cout << "enter handle_books5_BTC_USDT_SPOT, msg: " << msg << "\n";
    //TODO: simply process then put into queue

    /**
     * {
        "arg": {
            "channel": "books5",
            "instId": "BTC-USDT-SWAP"
        },
        "data": [
            {
                "asks": [ //["价格", "总量", "档位", "order数量"]
                    [
                        "90608.4",
                        "64.88",
                        "0",
                        "1"
                    ],
                    [
                        "90608.6",
                        "65.23",
                        "0",
                        "1"
                    ]
                ],
                "bids": [
                    [
                        "90608.3",
                        "0.66",
                        "0",
                        "1"
                    ]
                ],
                "instId": "BTC-USDT-SWAP",
                "seqId": 785659573,
                "ts": "1764505178009"
            }
        ]
    }
     */
}

void AsyncWebsocketClient::handle_trades_BTC_USDT_SPOT(const json& msg)
{
	std::cout << "enter handle_trades_BTC_USDT_SPOT, msg: " << msg << "\n";
    //TODO: simply process then put into queue
    /**
     * {
        "arg": {
            "channel": "trades",
            "instId": "BTC-USDT-SWAP"
        },
        "data": [
            {
                "count": "1",
                "instId": "BTC-USDT-SWAP",
                "px": "90608.4",
                "seqId": 785659571,
                "side": "sell",
                "source": "0",
                "sz": "0.35",
                "tradeId": "2491342311",
                "ts": "1764505177974"
            }
        ]
    }
     */
}

void AsyncWebsocketClient::handle_bbo_tbt_BTC_USDT_SPOT(const json& msg)
{
    std::cout << "enter handle_bbo_tbt_BTC_USDT_SPOT, msg: " << msg << "\n";
}

void AsyncWebsocketClient::handle_subscribe_success(const json& msg)
{
    std::cout << "subscribe success, channel = " << msg["arg"]["channel"] << ", instId = "<< 
        msg["arg"]["instId"] << std::endl;
}

void AsyncWebsocketClient::handle_subscribe_error(const json& msg)
{
    std::cout << "subscribe error, channel = " << msg["arg"]["channel"] << ", instId = "<< 
        msg["arg"]["instId"] << std::endl;
    handleStateTransition(WsConnectEvent::CONNECT_LOST); 
}


void WebSocketSession::handleStateTransition(WsConnectEvent event) 
{
    switch (m_state.load()) 
    {
        case WsConnectState::CONNECTED:
            handleConnected(event);
            break;
        case WsConnectState::CONNECTING:
            handleConnecting(event);
            break;
        case WsConnectState::DISCONNECTED:
            handleDisconnected(event);
            break;
        default:
            throw std::logic_error("Unknown state");
    }
}

void WebSocketSession::handleConnected(WsConnectEvent event)
{
    WsConnectState expected = WsConnectState::CONNECTED;
    switch (event)
    {
    case WsConnectEvent::CONNECT_LOST:
    {
        if (m_state.compare_exchange_strong(expected, WsConnectState::DISCONNECTED))
        {
            //ASN_INFO(loggerH_mqtt, "mqttStatus turns from CONNECT to DISCONNECT, reconnect...");
            //reconnect();
        } 
        break;
    }    
    default:
        break;
    }
}

void WebSocketSession::handleConnecting(WsConnectEvent event)
{
    WsConnectState expected = WsConnectState::CONNECTING;
    switch (event)
    {
    case WsConnectEvent::CONNECT_SUCCESS:
    {
        if (m_state.compare_exchange_strong(expected, WsConnectState::CONNECTED))
        {
            //ASN_INFO(loggerH_mqtt, "mqttStatus turns from CONNECTING to CONNECTED, success");
            subscribeAllTopics();
        } 
        break;
    }
    
    case WsConnectEvent::CONNECT_FAILED:
    {
        if (m_state.compare_exchange_strong(expected, WsConnectState::DISCONNECTED))
        {
            //ASN_INFO(loggerH_mqtt, "mqttStatus turns from CONNECTING to DISCONNECTED, failed, try again...");
            //reconnect();
        } 
        break;
    }
    
    default:
        break;
    }
}

void WebSocketSession::handleDisconnected(WsConnectEvent event)
{
    WsConnectState expected = WsConnectState::DISCONNECTED;
    switch (event)
    {
    case WsConnectEvent::START_CONNECT:
    {
        if (m_state.compare_exchange_strong(expected, WsConnectState::CONNECTING))
        {
			std::cout << "wsStatus turns from DISCONNECTED to CONNECTING" << "\n";
            //ASN_INFO(loggerH_mqtt, "mqttStatus turns from DISCONNECTED to CONNECTING, try to connect...");
            //connect();
        } 
        break;
    }   
    default:
        break;
    }
}


}