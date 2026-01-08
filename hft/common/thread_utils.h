#pragma once

#include <iostream>
#include <atomic>
#include <thread>
#include <unistd.h>
#include <memory>
#include <mutex>
#include <numa.h>
#include <numaif.h>

//#include "AsnLog.h"

#include <sys/syscall.h>

//static AsnLoggerPtr logger_thread = ASN_GETLOGGER("thread_utils");

namespace Common 
{

#if 0
static std::atomic<int> coreIndex = 0;  
static std::mutex coreMutex; 

inline void bindThreadToNumaCPU(int numa_node) 
{
    static struct bitmask* cpu_mask = numa_allocate_cpumask();
    numa_node_to_cpus(numa_node, cpu_mask);

    int totalCpus = numa_num_configured_cpus();
    int selected_cpu = -1;

    {
        std::lock_guard<std::mutex> lock(coreMutex); 
        int count = 0;

        // 从 NUMA 节点的 CPU 中选择一个
        for (int i = 0; i < totalCpus; i++) {
            if (numa_bitmask_isbitset(cpu_mask, i)) {
                if (count == coreIndex) {
                    selected_cpu = i;
                    coreIndex++;  // 递增 CPU 选择索引
                    break;
                }
                count++;
            }
        }
    }

    if (selected_cpu == -1) {
        ASN_WARN(logger_thread, "No available CPU found for NUMA node " << numa_node);
        return;
    }

    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(selected_cpu, &mask);

    pthread_t thread = pthread_self();
    if (pthread_setaffinity_np(thread, sizeof(mask), &mask) != 0) {
        ASN_INFO(logger_thread, "Failed to bind thread " << std::this_thread::get_id() << " to CPU " << selected_cpu);
    } else {
        ASN_INFO(logger_thread, "Thread " << std::this_thread::get_id() << " bound to CPU " << selected_cpu << " on NUMA node " << numa_node);
    }
}
#endif

/// Set affinity for current thread to be pinned to the provided core_id.
inline auto setThreadCore(int core_id) noexcept {
	cpu_set_t cpuset;

	CPU_ZERO(&cpuset);
	CPU_SET(core_id, &cpuset);

	return (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) == 0);
}

/// Creates a thread instance, sets affinity on it, assigns it a name and
/// passes the function to be run on that thread as well as the arguments to the function.
template<typename T, typename... A>
inline auto createAndStartThread(int numa_node, const std::string &name, T &&func, A &&... args) noexcept 
{
	int core_id = -1;
	auto t = new std::thread([&]() 
	{
	if (core_id >= 0 && !setThreadCore(core_id)) {
		std::cerr << "Failed to set core affinity for " << name << " " << pthread_self() << " to " << core_id << std::endl;
		exit(EXIT_FAILURE);
	}
	std::cerr << "Set core affinity for " << name << " " << pthread_self() << " to " << core_id << std::endl;

	// pthread_setname_np(pthread_self(), name.c_str());
	// bindThreadToNumaCPU(numa_node);

	std::forward<T>(func)((std::forward<A>(args))...);
	});

	using namespace std::literals::chrono_literals;
	std::this_thread::sleep_for(1s);

	return t;
}

}
