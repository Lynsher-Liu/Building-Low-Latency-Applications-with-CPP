/*
 * @Author: Lynsher xinyiliu@astri.org
 * @Date: 2025-03-19 16:08:12
 * @LastEditors: Lynsher xinyiliu@astri.org
 * @LastEditTime: 2025-03-19 18:01:28
 * @FilePath: /path_planning_service/src/mapf/affinity.cpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
/*
 * @Author: LynsherLiu xinyiliu@astri.org
 * @Date: 2024-03-15 12:07:49
 * @LastEditors: Lynsher xinyiliu@astri.org
 * @LastEditTime: 2025-03-19 15:49:34
 * @FilePath: /path_planning_service/src/mapf/timer.cpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */


#include "AsnLog.h"
#include "utils.h"
#include "affinity.h"
#include <sched.h>
#include <numa.h>


using namespace std;

static AsnLoggerPtr logger = ASN_GETLOGGER("affinity.cpp");

namespace affinity 
{

static std::atomic<int> coreIndex = 0;  
static std::mutex coreMutex; 

void bindThreadToNumaCPU(int numa_node) {
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
        ASN_WARN(logger, "No available CPU found for NUMA node " << numa_node);
        return;
    }

    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(selected_cpu, &mask);

    pthread_t thread = pthread_self();
    if (pthread_setaffinity_np(thread, sizeof(mask), &mask) != 0) {
        ASN_INFO(logger, "Failed to bind thread " << std::this_thread::get_id() << " to CPU " << selected_cpu);
    } else {
        ASN_INFO(logger, "Thread " << std::this_thread::get_id() << " bound to CPU " << selected_cpu << " on NUMA node " << numa_node);
    }
}

// only intialize cached_node one time
int get_least_loaded_numa_node() 
{
    static int cached_node = [](){
        int num_nodes = numa_max_node() + 1;
        std::vector<int> node_load(num_nodes, 0);

        cpu_set_t cpu_set;
        sched_getaffinity(0, sizeof(cpu_set_t), &cpu_set);

        for (int cpu = 0; cpu < CPU_SETSIZE; ++cpu) {
            if (CPU_ISSET(cpu, &cpu_set)) {
                int node = numa_node_of_cpu(cpu);
                node_load[node]++;  // 统计每个 NUMA node 的 CPU 负载
            }
        }

        // 选择负载最轻的 NUMA node
        int min_load = node_load[0];
        int best_node = 0;
        for (int i = 1; i < num_nodes; ++i) {
            if (node_load[i] < min_load) {
                min_load = node_load[i];
                best_node = i;
            }
        }
        return best_node;
    }();
    return cached_node;
}

} //namespace affinity

