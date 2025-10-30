#include "websocket.h"
#include <regex>


using json = nlohmann::json;
using namespace std;
namespace Trading 
{

void WsRouter::register_route(string topic_regex, 
		void (*callback)(json msg) ) 
{
    WsRoute route;
    route.topic_regex = topic_regex;
    route.callback = callback;
    routes.push_back(route);
}


void WsRouter::route_request(json msg)
{
    for (auto& r : routes) 
	{
        // match mqtt msg topic with route regex
        regex reg {r.topic_regex};
        smatch match;

		string topic = ""; //from msg

        if (std::regex_search(topic, match, reg)) {
            r.callback(msg);
            break;
        }
    }
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
            reconnect();
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
            reconnect();
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
            //ASN_INFO(loggerH_mqtt, "mqttStatus turns from DISCONNECTED to CONNECTING, try to connect...");
            connect();
        } 
        break;
    }   
    default:
        break;
    }
}


}