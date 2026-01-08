#include "websocket.h"
#include <regex>

using json = nlohmann::json;
using namespace std;

static AsnLoggerPtr logger = ASN_GETLOGGER("websocket_cpp");
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
    try
    {
        for (auto& r : routes) 
        {
            // match mqtt msg topic with route regex
            regex reg {r.topic_regex};
            smatch match;

            //{"arg":{"channel":"trades","instId":"BTC-USDT-SWAP"},"connId":"f5bee808","event":"subscribe"}
            //{"arg":{"channel":"books5","instId":"BTC-USDT-SWAP"},"data":[{"asks":[["103259.9","885.31","0","2"],["103261.1","0.83","0","1"],["103261.3","0.02","0","2"],["103261.4","0.01","0","1"],["103262","19.28","0","2"]],"bids":[["103250","21871.09","0","2"],["103218.9","885.4","0","1"],["103217.1","1.95","0","1"],["103216.5","1.96","0","1"],["103215","0.05","0","1"]],"instId":"BTC-USDT-SWAP","ts":"1762700094902","seqId":178414521}]}
            std::string channel = msg["arg"]["channel"];
            std::string uid = channel;

            // handle events individually
            if (msg["arg"].contains("instId"))
            {
                std::string instId = msg["arg"]["instId"];
                uid += ("|" + instId);
            }
            if (std::regex_search(uid, match, reg)) 
            {
                r.m_cb(msg);
                break;
            }
        }
    }
    catch(const std::exception& e)
    {
        ASN_ERROR(logger, "Exception in handling json: " << e.what());
    }
}

/**
 * @brief handle depth data of top 5 bid/ask
 * 数量: 合并了该价位所有挂单的总量
 * 档位: 0该字段已弃用(始终为0)
 * order数量: 这个价格档位是由n个独立的order组成的
 * 
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
void AsyncWebsocketClient::handle_books5_BTC_USDT(const json& msg)
{
    ASN_TRACE(logger, "enter handle_books5_BTC_USDT, msg: " << msg << "\n");
    //TODO: simply process then put into queue

}

/**
 * 1. **极速前沿更新**。延迟最低，专注于盘口最敏感部分
 * 2. 提供买一、卖一数据
 * 3. 首次推1档快照数据，以后定量推送，每10毫秒当1档快照数据有变化推送一次1档数据
 * {
    "arg": {
        "channel": "bbo-tbt",
        "instId": "BTC-USDT"
    },
    "data": [
        {
            "asks": [ //["价格", "总量", "档位", "order数量"]
                [
                    "91083.2",
                    "0.01024577",
                    "0",
                    "1"
                ]
            ],
            "bids": [
                [
                    "91082.4",
                    "0.01010275",
                    "0",
                    "1"
                ]
            ],
            "seqId": 173055617,
            "ts": "1764947891649"
        }
    ]
}
 */
void AsyncWebsocketClient::handle_bbo_tbt_BTC_USDT(const json& msg)
{
    ASN_TRACE(logger, "enter handle_bbo_tbt_BTC_USDT, msg: " << msg << "\n");
}

/**
 * @brief 获取最近的成交数据，有成交数据就推送，每次推送可能聚合多条成交数据
 * sz成交数量: 对于币币交易，成交数量的单位为交易货币;对于交割、永续以及期权，单位为张
 * 
 * 聚合订单:当count = 1时，表示taker订单部分或完全成交时仅匹配了一个maker订单。
 *          当count > 1时，表示taker订单以相同价格匹配了多个maker订单。
 *              例如，如果tradeId = 123，且count = 3，表示该消息聚合了tradeId = 123, 122, 121的成交。maker侧有多笔价格相同的订单被成交。
 * 
 * seqId: 同时发生的不同交易推送数据的`seqId`可能相同
 * 
 * {
        "arg": {
            "channel": "trades",
            "instId": "BTC-USDT-SWAP"
        },
        "data": [
            {
                "count": "1",           // 聚合的订单匹配数量
                "instId": "BTC-USDT-SWAP",
                "px": "90608.4",        // 成交价格
                "seqId": 785659571,     // 推送的序列号
                "side": "sell",         // 吃单方向
                "source": "0",          // 订单来源, 0：普通订单, 1：流动性增强计划订单
                "sz": "0.35",           // 成交数量
                "tradeId": "2491342311", // 聚合的多笔交易中最新一笔交易的成交ID
                "ts": "1764505177974"
            }
        ]
    }
 */
void AsyncWebsocketClient::handle_trades_BTC_USDT(const json& msg)
{
    ASN_TRACE(logger, "enter handle_trades_BTC_USDT, msg: " << msg << "\n");
    //TODO: simply process then put into queue

}

/**
 * 账户余额与可用保证金的更新
 * 当账户余额、可用保证金、冻结金额等发生变化时推送
 * 风控模块的核心，用于计算可用资金、保证金率、强平价
 * 
 * {
    "arg": {
        "channel": "account",
        "uid": "447074731796078878"
    },
    "curPage": 1,
    "data": [
        Object{...}
    ],
    "eventType": "snapshot",
    "lastPage": true
}
 */
void AsyncWebsocketClient::handle_account_update(const json& msg)
{
    ASN_TRACE(logger, "enter handle_account_update, msg: " << msg << "\n");
}

/**
 * 持仓详情 的更新（适用于币币、杠杆、合约等所有产品类型）
 * 当持仓数量、持仓均价、未实现盈亏、强平价格等发生变化时推送
 * 本地 position info 模块的唯一真相源
 * 
 * {
    "arg": {
        "channel": "positions",
        "instType": "ANY",
        "uid": "447074731796078878"
    },
    "curPage": 1,
    "data": [

    ],
    "eventType": "snapshot",
    "lastPage": true
}
 */
void AsyncWebsocketClient::handle_positions_update(const json& msg)
{
    ASN_TRACE(logger, "enter handle_positions_update, msg: " << msg << "\n");
}


/**
 * balance_and_position用于初始同步
 * 账户余额和持仓的完整快照（在登录后或断线重连时使用）。
 * 订阅后立即推送一次完整状态，之后仅在余额或持仓有重大变化时推送。
 *  初始化与校准的关键。避免本地与交易所状态不一致。
 * 
 * {
    "arg": {
        "channel": "balance_and_position",
        "uid": "447074731796078878"
    },
    "data": [
        {
            "balData": [
                {
                    "cashBal": "3",
                    "ccy": "BTC",
                    "uTime": "1684852904715"
                },
                {
                    "cashBal": "30",
                    "ccy": "LTC",
                    "uTime": "1684852904759"
                },...
            ]
            "eventType": "snapshot",
            "pTime": "1765472158607",
            "posData": [
            ],
            "trades": [

            ]
        }
    ]
}
 */
void AsyncWebsocketClient::handle_balance_and_position_update(const json& msg)
{
    ASN_TRACE(logger, "enter handle_balance_and_position_update, msg: " << msg << "\n");
}



}