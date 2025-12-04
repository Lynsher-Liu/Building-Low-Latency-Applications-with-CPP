/*
 * @Author: LynsherLiu xinyiliu@astri.org
 * @Date: 2024-03-05 10:02:46
 * @LastEditors: Lynsher xinyiliu@astri.org
 * @LastEditTime: 2025-12-04 11:01:52
 * @FilePath: /path_planning_service/src/mapf/timer.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#pragma once

#include <iostream>
#include <memory>
#include <mutex>
#include <atomic>


// singleton base class
template<typename T>
class Singleton{
public:
    static shared_ptr<T> getInstance()
    {
        if (m_instance == nullptr)
        {
            unique_lock<std::mutex> locker(m_mutex);
            if (m_instance == nullptr)
            {
                m_instance = shared_ptr<T>(new T());
            }
        }
        return m_instance;
    }

    Singleton(const Singleton&)=delete;
    Singleton& operator =(const Singleton&)=delete;

#if 0
    template <typename ... Args>
    static void createInstance(Args&& ... args)
    {
        if (m_instance == nullptr)
        {
            unique_lock<std::mutex> locker(m_mutex);
            if (m_instance == nullptr)
            {
                m_instance = shared_ptr<T>(new T(std::forward<Args>(args)...));
            }
        }
    }
#endif

protected:
    Singleton() noexcept = default;

//private:
    static std::mutex m_mutex;
    static shared_ptr<T> m_instance;
};

template<typename T> std::mutex Singleton<T>::m_mutex;
template<typename T> std::shared_ptr<T> Singleton<T>::m_instance = nullptr;