/*
 * @Author: Lynsher xinyiliu@astri.org
 * @Date: 2025-03-20 09:58:50
 * @LastEditors: Lynsher xinyiliu@astri.org
 * @LastEditTime: 2025-03-26 16:15:29
 * @FilePath: /path_planning_service/src/mapf/affinity.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#pragma once

#include <iostream>
#include <cmath>
#include <unistd.h>
#include <memory>
#include <ctime>
#include <mutex>
#include <atomic>
#include <sys/sysinfo.h>
#include <memory_resource>
#include <numa.h>
#include <numaif.h>

#define MAX_BOUND_CORE 4

namespace affinity 
{
// NUMA 绑定的 PMR 资源
class NUMA_MemoryResource : public std::pmr::memory_resource {
    int node;  // 绑定的 NUMA 节点

public:
    explicit NUMA_MemoryResource(int numa_node) : node(numa_node) {
        if (numa_available() == -1) {
            throw std::runtime_error("NUMA not supported on this system!");
        }
    }

protected:
    void* do_allocate(size_t bytes, size_t alignment) override {
        return numa_alloc_onnode(bytes, node);  // 在指定 NUMA 节点分配内存
    }

    void do_deallocate(void* p, size_t bytes, size_t alignment) override {
        numa_free(p, bytes);
    }

    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        return this == &other;
    }
};


class PmrMemoryNumaAllocator
{
public:
    ~PmrMemoryNumaAllocator() {};
	PmrMemoryNumaAllocator(int numa_node) noexcept: //int thread_id, 
        //t_id(thread_id), 
        numa_node(numa_node), 
        numa_resource(numa_node),
        pool(&numa_resource), 
        allocator(&pool) {}

    template <typename T, typename ... Args>
    shared_ptr<T> produceSharedPtr(Args&& ... args) noexcept
    {
        return std::allocate_shared<T>(allocator, std::forward<Args>(args)...); 
    }

private:
    // NUMA-related resources
    //int t_id{0};

    int numa_node;
    affinity::NUMA_MemoryResource numa_resource;
    std::pmr::synchronized_pool_resource pool;
    std::pmr::polymorphic_allocator<std::byte> allocator;
};

void bindThreadToNumaCPU(int numa_node);
int get_least_loaded_numa_node();

} //namespace affinity



