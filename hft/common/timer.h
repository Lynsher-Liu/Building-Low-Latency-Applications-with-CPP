/*
 * @Author: LynsherLiu xinyiliu@astri.org
 * @Date: 2024-03-05 10:02:46
 * @LastEditors: Lynsher xinyiliu@astri.org
 * @LastEditTime: 2025-12-04 12:07:05
 * @FilePath: /path_planning_service/src/mapf/timer.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#pragma once

#include <iostream>
#include <memory>
#include <chrono>
#include <thread>
#include <functional>
#include <mutex>
#include <atomic>
#include <time.h>
#include <chrono>
#include <ctime>
#include <set>
#include <sys/time.h> 
#include <sys/epoll.h>
#include <sys/timerfd.h>

#include "singleton.h"


namespace timer 
{

class TradingClock : public Singleton<TradingClock>
{
public:
    friend class Singleton<TradingClock>;

    //void update(atomic_bool& stopFlag);
    uint64_t getUnixEpochTime() noexcept
    {
        return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }

    uint64_t getCurMicroTime() noexcept
    {
        return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }

    uint64_t getCurNanoTime() noexcept
    {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }

private:   
    TradingClock()
    {
        clockStartTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }

    TradingClock(const TradingClock&) = delete;
    TradingClock& operator=(const TradingClock&) = delete;
    TradingClock(TradingClock&&) = delete;
    TradingClock& operator=(TradingClock&&) = delete;

    uint64_t clockStartTime; //milli seconds
};

//typedef void callback(void);
using callback_t = std::function<void(void)>;

class TimerManager;
struct TimeNode
{
    //using callback_t = std::function<void(const timer::TimeNode& node)>;
    TimeNode(uint64_t expireTime, callback_t cb) :
        m_expireTime(expireTime),
        m_cb(cb) 
        {
            m_nodeId = nodeId++;
        }

    uint64_t m_expireTime;
    uint64_t m_nodeId;   
    callback_t m_cb;   
    
    static uint64_t nodeId;
};

struct compareTimeNode
{
    bool operator()(const TimeNode& node1, const TimeNode& node2) const
    {
        if (node1.m_expireTime < node2.m_expireTime)
        {
            return true;
        }
        else if (node1.m_expireTime > node2.m_expireTime)
        {
            return false;
        }
        else
        {
            return node1.m_nodeId < node2.m_nodeId;
        }
    }
};


class TimerManager : public Singleton<TimerManager>
{
public:
    friend class Singleton<TimerManager>;
    ~TimerManager();

    static uint64_t getCurTick()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();       
    }

    uint64_t getRecentTimeout();

    void handleTimer(uint64_t now);   

    /**
     * timeout: ms, how long to execute node after adding time
     */
    void addNode(uint64_t timeout, callback_t cb);
    void deleteNode(TimeNode* node);

    void updateTimerfd(const int fd);

    void start();

private:
    TimerManager() = default;
    TimerManager(const TimerManager&) = delete;
    TimerManager& operator=(const TimerManager&) = delete;
   
    std::set<TimeNode, compareTimeNode> timeNodeSet;
    int timerfd;

    std::atomic_bool m_stop{false};
    std::mutex m_mutex;
};

} //namespace timer