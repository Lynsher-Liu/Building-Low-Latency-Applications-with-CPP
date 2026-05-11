/*
 * @Author: Lynsher xinyiliu@astri.org
 * @Date: 2025-12-01 13:52:34
 * @LastEditors: Lynsher xinyiliu@astri.org
 * @LastEditTime: 2026-03-27 16:33:59
 * @FilePath: /my_HFT/hft/common/time_utils.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#pragma once

#include <string>
#include <chrono>
#include <ctime>
#include <variant>
#include <functional>
#include <memory>
#include <atomic>
#include <cassert>
#include <type_traits>

#include "concurrentqueue/concurrentqueue.h"
#include "types.h"
#include "mem_pool.h"
#include "thread_utils.h"
#include "AsnLog.h"
#include "affinity.h"


static AsnLoggerPtr loggerEventBus_h = ASN_GETLOGGER("eventBus_h");

namespace Common
{

using Event = std::variant<shared_ptr<const PriceLevel>, shared_ptr<const Trade>>;

class EventSubscriber;

class EventBus
{
private:
    std::vector<EventSubscriber*> subscribers_;

public:
    EventBus() = default;
    ~EventBus() = default;

    // 注册订阅者
    void subscribe(EventSubscriber* subscriber) 
    {
        subscribers_.push_back(subscriber);
    }

    // 发布事件：将事件拷贝到每个订阅者的队列
    void publish(const Event& event);
};


/**
 * Modules who want to receive events should inherit from this Subscriber class and implement the handleEvent method. 
 * They can then subscribe to the EventBus to receive events.
 * e.g, strategy engine, feature engine, risk manager, position keeper, etc.
 */
class EventSubscriber
{
public:
    static int id_counter;

    EventSubscriber(EventBus& bus, std::string name, const int& numaNode)
        : bus_(bus), name_(std::move(name)), bindToNumaNode(numaNode), m_stop(false) 
    {
        id_counter++;
        bus_.subscribe(this);
        //start();
    }

    virtual ~EventSubscriber() 
    {
        stop();
    }

    void start()
    {
		m_stop.store(false);
		m_worker_thread = Common::createAndStartThread(bindToNumaNode, "Subscriber-"+std::to_string(id_counter), [this]() { run(); });
		if (!m_worker_thread)
			ASN_ERROR(loggerEventBus_h, "Failed to start Subscriber-"+std::to_string(id_counter));
    }

    void stop()
	{
		m_stop.store(true);
		
		// 给一点时间让会话优雅关闭（可选）
        std::this_thread::sleep_for(100ms);

		if (m_worker_thread && m_worker_thread->joinable())
			m_worker_thread->join();
    }

    // 实现Subscriber接口
    // moodycamel::ConcurrentQueue<Event>& getQueue() 
    // {
    //     return m_queue;
    // }

    void enqueueEvent(const Event& event) 
    {
        m_queue.enqueue(event);
    }

protected:
    virtual void handleEvent(const Event& event) = 0;

private:
    void run() 
    {
        while (!m_stop.load()) 
        {
            Event event;
            if (m_queue.try_dequeue(event)) {
                handleEvent(event);
            } else {
                std::this_thread::yield();
            }
        }
    }

    const int& bindToNumaNode;
    EventBus& bus_;
    std::string name_;
    std::atomic<bool> m_stop{false};
    std::thread* m_worker_thread;
    moodycamel::ConcurrentQueue<Event> m_queue;
};

int EventSubscriber::id_counter = 0;




}
