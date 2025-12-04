/*
 * @Author: LynsherLiu xinyiliu@astri.org
 * @Date: 2024-03-15 12:07:49
 * @LastEditors: Lynsher xinyiliu@astri.org
 * @LastEditTime: 2025-12-04 12:00:01
 * @FilePath: /path_planning_service/src/mapf/timer.cpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */

#include <unistd.h>
#include "timer.h"
#include "AsnLog.h"


using namespace std;

static AsnLoggerPtr logger = ASN_GETLOGGER("timer.cpp");

namespace timer 
{

uint64_t TimeNode::nodeId = 0;

TimerManager::~TimerManager()
{
    //ASN_INFO(logger, "stop TimerManager ");
    m_stop = true;
    sleep(1); // wait timer thread stop, then deconstruct TimerManager
}

void TimerManager::addNode(uint64_t timeout, callback_t cb)
{
    if (timeout <= 0) return;

    uint64_t now = getCurTick();
    TimeNode node{now+timeout, cb};
    {
        unique_lock<std::mutex> locker(m_mutex);       
        timeNodeSet.emplace(node);
    }  
    //ASN_DEBUG(logger, "add timer nodeId = " << node.m_nodeId << " for timeout = " << timeout);

    updateTimerfd(timerfd);   
}

void TimerManager::deleteNode(TimeNode* node)
{
    auto it = timeNodeSet.find(*node);
    if (it != timeNodeSet.end())
    {
        {
            unique_lock<std::mutex> locker(m_mutex);
            timeNodeSet.erase(it);                     
        }        
        //ASN_DEBUG(logger, "delete timer nodeId = " << node->m_nodeId); 

        updateTimerfd(timerfd);        
    }
}

void TimerManager::handleTimer(uint64_t now)
{
    if (timeNodeSet.empty()) 
    {
        //ASN_DEBUG(logger, "timeNodeSet is empty, return");
        return;
    }

    std::vector<TimeNode> expiredNodes;  // 存储到期任务, devide timeNodeSet operation and callback exec
    
    {
        unique_lock<std::mutex> locker(m_mutex);
        for (auto iter = timeNodeSet.begin(); iter != timeNodeSet.end();)
        {
            int64_t timeout = iter->m_expireTime - now;
            if (timeout <= 0)
            {
                expiredNodes.push_back(*iter);
                iter = timeNodeSet.erase(iter);
            }
            else
            {
                break; // 因为是有序集合，后续任务肯定未到期
            }
        }
    }
    
    // 执行到期任务（解锁后执行回调，避免持锁时间过长）
    for (const auto& node : expiredNodes) {
        //ASN_DEBUG(logger, "execute timer nodeId = " << node.m_nodeId);
        node.m_cb();
    }
}

uint64_t TimerManager::getRecentTimeout()
{
    uint64_t timeout = -1;

    unique_lock<std::mutex> locker(m_mutex);
    if(timeNodeSet.empty())
        return timeout;

    uint64_t now = getCurTick();
    auto iter = timeNodeSet.begin();
    timeout = iter->m_expireTime - now;
    if(timeout < 0)
        timeout = 0;

    return timeout;
}

void TimerManager::updateTimerfd(const int fd)
{
    struct timespec abstime = {.tv_sec = 0, .tv_nsec = 0};

    {
        unique_lock<std::mutex> locker(m_mutex);
        auto iter = timeNodeSet.begin();
       
        // timeNodeSet is empty
        if (iter == timeNodeSet.end())
        {
            struct itimerspec new_value = 
            {
                .it_interval = {},
                .it_value = abstime,
            };
            timerfd_settime(fd, 0, &new_value, nullptr); //TFD_TIMER_ABSTIME
            //ASN_TRACE(logger, "timeNodeSet is empty(), set timerfd expire time: 0");
            return;
        }

        // timeNodeSet is not empty, set first node timeout to timerfd    
        uint64_t now = getCurTick();
        int64_t timeDiff = iter->m_expireTime - now;
        /**
         * !!! note !!! must check if the first node is expired
         * timeDiff must use int64_t with symbols, instead of uint64_t
         * otherwise it may overflow like abstime.tv_sec = 18446744073709538, abstime.tv_nsec = 178000000 
         */
        if (timeDiff <= 0) 
        {
            // 如果定时器已经过期，立即触发
            abstime.tv_sec = 0;
            abstime.tv_nsec = 1;  // 设置为 1 纳秒，尽快触发
        }
        else
        {
            abstime.tv_sec = timeDiff / 1000;
            abstime.tv_nsec = timeDiff % 1000 * 1000000;
        }        
    }

    struct itimerspec new_value = 
    {
        .it_interval = {},
        .it_value = abstime,
    };
    timerfd_settime(fd, 0, &new_value, nullptr);
    //ASN_TRACE(logger, "set timerfd expire time: abstime.tv_sec = " << abstime.tv_sec << ", abstime.tv_nsec = " << abstime.tv_nsec);
} 

void TimerManager::start()
{
    int epfd = epoll_create(1);
    timerfd = timerfd_create(CLOCK_MONOTONIC, 0);

    struct epoll_event ev = {.events = EPOLLIN | EPOLLET, .data = {.fd = timerfd}};
    epoll_ctl(epfd, EPOLL_CTL_ADD, timerfd, &ev);
    //ASN_DEBUG(logger, "create epoll and timerfd = " << timerfd);

    while (!m_stop.load())
    {
        struct epoll_event events[256];

        updateTimerfd(timerfd);
        int nfds = epoll_wait(epfd, events, 256, -1); 
        //int nfds = epoll_wait(epfd, events, 256, getRecentTimeout()); 
        uint64_t now = getCurTick();

        for (int i = 0; i < nfds; i++)
        {
            if (events[i].data.fd == timerfd)
            {
                //ASN_DEBUG(logger, "handleTimer ");               
                handleTimer(now);
                //ASN_DEBUG(logger, "finishTimer ");
            }
        }
    }

    epoll_ctl(epfd, EPOLL_CTL_DEL, timerfd, nullptr);
    close(epfd);
    close(timerfd);
    return;
}


} //namespace timer 