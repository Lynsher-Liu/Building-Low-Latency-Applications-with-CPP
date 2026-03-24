/*
 * @Author: Lynsher xinyiliu@astri.org
 * @Date: 2025-12-01 13:52:34
 * @LastEditors: Lynsher xinyiliu@astri.org
 * @LastEditTime: 2026-03-24 18:05:33
 * @FilePath: /my_HFT/hft/common/time_utils.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#pragma once

#include "event_bus.h"


static AsnLoggerPtr logger = ASN_GETLOGGER("eventBus");

namespace Common
{


void EventBus::publish(const Event& event) 
{
    for (auto* sub : subscribers_) {
        //sub->getQueue().enqueue(event);
        sub->enqueueEvent(event);
    }
}



}
