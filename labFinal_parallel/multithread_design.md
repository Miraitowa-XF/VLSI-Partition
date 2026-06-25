# 多线程并行化改造设计文档

## 1. 概述

本文档描述了 VLSI K-Way Partitioning 算法的多线程并行化改造思路和实现方案。通过使用 OpenMP，将原本顺序执行的算法关键阶段进行并行化加速。

## 2. 原始算法分析

### 2.1 算法主要阶段

1. **候选 FPGA 传播** (`propagate_candidates_with_graph`)
   - BFS 遍历计算每个节点的可行 FPGA 集合
   - 根据拓扑距离约束剪枝候选集

2. **初始化划分** (`init_partition`)
   - 贪心分配节点到初始 FPGA

3. **FM 迭代优化**
   - `compute_initial_gains`: 计算初始增益
   - `fm_pass`: 单轮 FM 移动
   - `update_gains_after_move`: 更新邻居增益

4. **违规修复** (在 `my_partition_algorithm` 中)
   - 检测并修复拓扑违规的网表连接

### 2.2 可并行化的部分

| 阶段 | 可并行化程度 | 原因 |
|------|-------------|------|
| Cddt_mask 初始化 | 完全并行 | 每个节点的初始化相互独立 |
| radius_has_预处理 | 完全并行 | 每个 FPGA 的计算独立 |
| BFS 邻居处理 | 部分并行 | BFS 队列有依赖，但邻居遍历可并行 |
| compute_initial_gains | 完全并行 | 每个节点的增益计算独立 |
| 违规修复扫描 | 部分并行 | 需保证每次只应用一个最佳修复 |

## 3. 并行化实现策略

### 3.1 线程数控制

```cpp
// solution.h
void set_num_threads(int n) { num_threads_ = n; }
int get_num_threads() const { return num_threads_; }
```

在 `Solution` 类中添加线程数接口，用户可通过命令行参数指定：
```bash
./main <netlist> [topo] [K] [num_threads]
```

### 3.2 条件编译

使用 `#ifdef _OPENMP` 进行条件编译，确保代码在不支持 OpenMP 的环境中也能正常工作：

```cpp
#ifdef _OPENMP
#pragma omp parallel for if(num_threads_ > 0 && total_nodes_ > 1000)
#endif
```

### 3.3 细粒度并行优化

#### 3.3.1 候选传播并行化

**Cddt_mask 初始化：**
```cpp
#pragma omp parallel for if(num_threads_ > 0 && total_nodes_ > 1000)
for (int v = 0; v <= total_nodes_; v++) {
    if (is_fixed[v]) {
        Cddt_mask[v][fixed_fpga[v]] = true;
    } else {
        for (int f = 0; f < K_; f++) Cddt_mask[v][f] = true;
    }
}
```

**radius_has_预处理（collapse(2)）：**
```cpp
#pragma omp parallel for collapse(2) if(num_threads_ > 0 && K_ > 10)
for (int f = 0; f < K_; f++) {
    for (int d = 0; d <= max_dist_[f]; d++) {
        for (int j = 0; j < K_; j++) {
            if (topo_dist[f][j] <= d) {
                radius_has_[f][j][d] = true;
            }
        }
    }
}
```

**BFS 邻居处理（带 critical）：**
```cpp
#pragma omp parallel for schedule(dynamic) if(num_threads_ > 1 && nbrs.size() > 64)
for (size_t idx = 0; idx < nbrs.size(); idx++) {
    int nb = nbrs[idx];
    // ... 计算逻辑
    
    #pragma omp critical
    {
        bfs_q.push({nb, fpga_u});
    }
    
    #pragma omp critical(cddt_update)
    {
        // 更新 Cddt_mask
    }
}
```

#### 3.3.2 增益计算并行化

```cpp
void Solution::compute_initial_gains(Graph &graph) {
    #ifdef _OPENMP
    if (num_threads_ > 0) omp_set_num_threads(num_threads_);
    #endif
    
    // net_count 统计使用原子操作
    vector<int> temp_net_count((int)net_count_.size() * K_, 0);
    #pragma omp parallel for if(num_threads_ > 1 && graph.get_node_num() > 1000)
    for (int idx = 0; idx < (int)graph.get_nodes().size(); idx++) {
        Node *node = graph.get_nodes()[idx];
        int nid = node->get_index(); 
        int P = part_[nid];
        for (Net *net : node->get_nets()) {
            #pragma omp atomic capture
            net_count_[net->get_index()][P]++;
        }
    }
    
    // 增益计算完全并行
    #pragma omp parallel for schedule(dynamic, 64) if(num_threads_ > 1 && graph.get_node_num() > 1000)
    for (int idx = 0; idx < (int)graph.get_nodes().size(); idx++) {
        // ... 计算 gain_ 和 topo_delta_
    }
}
```

#### 3.3.3 违规修复并行化

采用"并行扫描 + 串行应用"的两阶段策略：

```cpp
struct RepairProposal {
    Net *net_ptr;
    int best_target;
    int best_viol_after;
    vector<int> best_state_part;
    vector<int> best_state_size;
    vector<vector<int>> best_state_netcount;
    bool valid;
};

RepairProposal global_best{nullptr, -1, INT_MAX, {}, {}, {}, false};

#pragma omp parallel
{
    RepairProposal local_best{nullptr, -1, INT_MAX, {}, {}, {}, false};
    
    #pragma omp for schedule(dynamic, 16)
    for (int net_idx = 0; net_idx < num_nets; net_idx++) {
        // 为当前网找到最佳修复方案
        // 更新 local_best
    }
    
    #pragma omp critical
    {
        if (local_best.valid && local_best.best_viol_after < global_best.best_viol_after) {
            global_best = local_best;
        }
    }
}

// 应用最佳修复
if (global_best.valid) {
    part_ = global_best.best_state_part;
    current_size_ = global_best.best_state_size;
    net_count_ = global_best.best_state_netcount;
}
```

## 4. 线程安全策略

### 4.1 只读数据共享

以下数据结构在并行期间是只读的，无需同步：
- `topo_adj`, `topo_dist`: 拓扑结构
- `max_dist_`: 最大距离
- `radius_has_`: 半径查询表（预处理后）

### 4.2 写冲突处理

| 数据结构 | 冲突类型 | 解决方案 |
|---------|---------|----------|
| `part_` | 频繁写 | 使用临界区保护修改 |
| `gain_`, `topo_delta_` | 每节点独立写 | 无锁并行（每个线程处理不同节点） |
| `net_count_` | 累加操作 | 使用原子操作或临时数组合并 |
| BFS 队列 | 并发入队 | 使用 critical 保护 |
| 桶结构 | 链表修改 | 保持串行（瓶颈较小） |

## 5. Makefile 配置

```makefile
CXXFLAGS = -Wall -Wextra -std=c++17 -O2 -g -fopenmp

all: main

main: $(OBJ)
	$(CXX) $(CFLAGS) -o main $(OBJ) -fopenmp

%.o: %.cpp
	$(CXX) $(CFLAGS) -c $< -o $@ -fopenmp
```

## 6. 使用方法

```bash
# 编译
cd labFinal
g++ -fopenmp -std=c++17 -O2 -o main main.cpp Graph.cpp Net.cpp Node.cpp solution.cpp evaluate.cpp

# 运行（单线程/默认）
./main ./dataset/case1

# 运行（指定线程数）
./main ./dataset/case1 ./dataset/MFS1 8 4

# 参数说明
# argv[1]: 网表文件
# argv[2]: 拓扑文件（可选）
# argv[3]: K 路划分数（可选，默认 4）
# argv[4]: 并行线程数（可选，默认系统 CPU 核心数）
```

## 7. 性能预期

| 规模 | 单线程时间 | 4 线程时间 | 加速比 |
|-----|-----------|----------|--------|
| 小 (<1000 节点) | ~1s | ~0.9s | 1.1x |
| 中 (1000-10000 节点) | ~10s | ~4s | 2.5x |
| 大 (>10000 节点) | ~60s | ~20s | 3x |

注意：由于 Amdahl 定律和 FM 迭代的串行性质，加速比不会线性增长。

## 8. 限制与改进方向

### 8.1 当前限制

1. FM pass 的选择过程仍需串行执行（寻找全局最优移动）
2. 桶结构的插入/删除操作未并行化
3. 小图上并行开销可能超过收益

### 8.2 潜在改进

1. **GPU 加速**: 对于超大规模图，可将增益计算迁移到 GPU
2. **异步并行 FM**: 探索多 pass 同时执行的策略
3. **自适应并行**: 根据问题规模动态调整并行粒度

## 9. 参考

- TopoPart Paper: arXiv:2308.15494
- OpenMP Specification 5.0
- "Introduction to Parallel Computing" by Grama et al.
