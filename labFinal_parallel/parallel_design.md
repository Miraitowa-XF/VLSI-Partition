# 多路 FM 划分算法并行化设计文档

## 1. 概述

本文档描述了多路（K-way）Fiduccia-Mattheyses (FM) 划分算法的 OpenMP 多线程并行化实现方案。该算法支持拓扑约束，用于将超图划分为 K 个部分，同时最小化切割大小和拓扑违规数。

## 2. 并行化策略总览

### 2.1 并行模型选择：OpenMP

- **理由**：共享内存架构、细粒度并行控制、易于与现有 C++ 代码集成
- **编译选项**：`-fopenmp -O2 -std=c++17`
- **线程管理**：通过 `num_threads_` 成员变量控制线程数，默认使用系统推荐值

### 2.2 并行化原则

1. **数据独立性优先**：优先并行化无数据依赖的计算密集型操作
2. **避免数据竞争**：对于共享数据结构（如桶结构），使用串行或临界区保护
3. **动态调度**：对不规则工作负载使用 `schedule(dynamic, chunk_size)`
4. **条件并行**：使用 `if(num_threads_ > 1 && data_size > threshold)` 避免小数据集的并行开销

## 3. 已并行化的函数详解

### 3.1 compute_cut_size() - 切割大小计算

```cpp
// 位置：solution.cpp:624+
int Solution::compute_cut_size(Graph &graph) {
    #pragma omp parallel for reduction(+:cut) 
    for (size_t i = 0; i < nets.size(); i++) {
        // 每个网的计算相互独立
        // 使用 reduction 累积 cut 值
    }
}
```

**并行策略**：
- **并行类型**：SIMD-style 并行循环
- **同步机制**：`reduction(+:cut)` 原子累加
- **适用场景**：大网表（>1000 条边）
- **加速比预期**：接近线性（CPU 核心数限制）

### 3.2 compute_violations() - 拓扑违规计算

```cpp
// 位置：solution.cpp:648+
int Solution::compute_violations(Graph &graph, const vector<int> &part) {
    #pragma omp parallel for reduction(+:violations)
    for (size_t i = 0; i < nets.size(); i++) {
        // 每条网的违规检查相互独立
    }
}
```

**并行策略**：与 `compute_cut_size()` 相同
- **特点**：纯读操作，无数据竞争风险

### 3.3 build_buckets() - 桶结构构建

```cpp
// 位置：solution.cpp:541+
void Solution::build_buckets(Graph &graph) {
    // 1. 初始化桶头数组（可并行）
    #pragma omp parallel for if(K_ > 2)
    for (int s = 0; s < K_; s++) { ... }
    
    // 2. 收集待插入节点（并行筛选 + 临界区收集）
    #pragma omp parallel for schedule(dynamic, 64)
    #pragma omp critical
    to_insert.push_back(i);
    
    // 3. 串行插入桶（避免并发修改链表结构）
    for (int node_id : to_insert) bucket_insert(node_id);
}
```

**并行策略**：
- **阶段 1**：桶头初始化完全并行
- **阶段 2**：并行筛选候选节点
- **阶段 3**：串行插入（桶是链表结构，难以安全并行化）
- **权衡**：减少同步开销，保持正确性

### 3.4 compute_initial_gains() - 初始增益计算

```cpp
// 位置：solution.cpp:471+
void Solution::compute_initial_gains(Graph &graph) {
    // 1. 并行统计 net_count（使用原子操作）
    #pragma omp parallel for
    for (int idx = 0; idx < nodes.size(); idx++) {
        #pragma omp atomic
        temp_net_count[net_idx * K_ + P]++;
    }
    
    // 2. 完全并行计算每个节点的增益
    #pragma omp parallel for schedule(dynamic, 64)
    for (int idx = 0; idx < nodes.size(); idx++) {
        // 每个节点的增益计算独立
        // 计算 gain_[nid][X] 和 topo_delta_[nid][X]
    }
}
```

**并行策略**：
- **第一阶段**：使用临时数组 + 原子操作避免竞争
- **第二阶段**：完全并行，每个节点独立计算
- **调度**：`schedule(dynamic, 64)` 处理不均匀的工作负载

### 3.5 update_gains_after_move() - 移动后增益更新

```cpp
// 位置：solution.cpp:551+
void Solution::update_gains_after_move(Graph &graph, int moved_node, ...) {
    // 1. 收集受影响节点（串行 - 遍历移动节点的邻接网）
    
    // 2. 并行减去旧增益贡献
    #pragma omp parallel for schedule(dynamic, 8)
    for (size_t nidx = 0; nidx < nets_to_process.size(); nidx++) {
        Net *net = nets_to_process[nidx];
        // 每个节点只修改自己的增益值
        #pragma omp atomic
        gain_[cid][X] -= old_contrib;
    }
    
    // 3. 串行更新 net_count（后续计算依赖最新值）
    for (Net *net : nets_to_process) {
        net_count_[net_id][from_side]--; 
        net_count_[net_id][to_side]++;
    }
    
    // 4. 并行加上新增益贡献
    #pragma omp parallel for schedule(dynamic, 8)
    for (size_t nidx = 0; nidx < nets_to_process.size(); nidx++) {
        #pragma omp atomic
        gain_[cid][X] += new_contrib;
    }
    
    // 5. 并行计算 max_gain，串行插入桶
    #pragma omp parallel for
    for (size_t idx = 0; idx < affected.size(); idx++) {
        // 计算每个节点的最大增益
    }
    for (int cid : affected) {
        bucket_insert(cid);  // 串行插入
    }
}
```

**并行策略**：
- **关键洞察**：增益更新的本质是每个节点只修改自己对应的 `gain_[cid]` 数组，因此可以使用原子操作安全并行
- **顺序依赖**：`net_count` 更新必须在"减旧增益"之后、"加新增益"之前串行执行
- **桶操作**：串行插入以避免链表并发修改问题

### 3.6 bucket_get_best_node() - 寻找最佳移动节点

```cpp
// 位置：solution.cpp:355+
int Solution::bucket_get_best_node(int &from_side, int &to_side, Graph &graph) {
    for (int k = 0; k < K_; ++k) {
        // 收集当前块的候选节点
        vector<int> candidates;
        
        // 并行计算候选节点的 topo_delta
        #pragma omp parallel for schedule(dynamic, 8)
        for (size_t i = 0; i < candidates.size(); i++) {
            int curr = candidates[i];
            // 每个线程计算不同节点的 topo_delta，无竞争
            for (int X = 0; X < K_; ++X) {
                topo_delta_[curr][X] = compute_topo_delta(graph, curr, X);
            }
        }
        
        // 串行评估并更新最佳结果
        for (int curr : candidates) { ... }
    }
}
```

**并行策略**：
- **难点**：主搜索循环需要按增益降序遍历，且需要实时更新全局最佳
- **解决方案**：对计算密集型的 `topo_delta` 计算进行并行化，评估逻辑保持串行
- **tradeoff**：牺牲部分并行度换取正确性和简单性

### 3.7 propagate_candidates_with_graph() - 候选 FPGA 传播

```cpp
// 位置：solution.cpp:141+
void Solution::propagate_candidates_with_graph(Graph &graph) {
    // 1. 并行初始化 Cddt_mask
    #pragma omp parallel for
    for (int v = 0; v <= total_nodes_; v++) { ... }
    
    // 2. 并行预处理 radius_has_
    #pragma omp parallel for
    for (int f = 0; f < K_; f++) { ... }
    
    // 3. BFS 传播（保持串行 - 图遍历的本质特性）
    while (!bfs_q.empty()) { ... }
}
```

**并行策略**：
- **初始化阶段**：完全并行
- **BFS 阶段**：保持串行（BFS 本质是顺序依赖的算法）

## 4. 未并行化部分及原因

### 4.1 fm_pass() 主循环

```cpp
while (true) {
    int chosen = bucket_get_best_node(...);  // 找最佳节点
    // 移动节点
    update_gains_after_move(...);  // 更新增益
}
```

**不并行化原因**：
1. **强顺序依赖**：每次移动后必须立即更新受影响的增益
2. **收敛性保证**：FM 算法的正确性依赖于顺序迭代
3. **收益有限**：即使尝试并行，也会因频繁同步而性能下降

### 4.2 违规修复阶段

```cpp
for (int repair_iter = 0; repair_iter < 20; repair_iter++) {
    for (Net *net : graph.get_nets()) {
        // 回溯式修复，涉及复杂的状态保存和恢复
    }
}
```

**不并行化原因**：
1. **状态竞争**：多个线程同时修改 `part_`, `current_size_`, `net_count_`
2. **回溯复杂性**：并行回溯需要复杂的版本控制机制
3. **实际影响小**：此阶段通常执行次数少，对总体性能影响不大

## 5. 并行化注意事项

### 5.1 线程数设置

```cpp
// 构造函数中初始化
Solution::Solution() : K_(4), num_threads_(1) {
#ifdef _OPENMP
    num_threads_ = omp_get_max_threads();  // 默认使用最大可用线程数
#endif
}

// 用户可通过 set_num_threads() 调整
void set_num_threads(int n);
```

### 5.2 条件并行阈值

| 函数 | 启用阈值 |
|------|----------|
| compute_cut_size | nets > 1000 |
| compute_violations | nets > 1000 |
| compute_initial_gains | nodes > 1000 |
| update_gains_after_move | affected_nets > 20 |
| build_buckets | nodes > 1000 |

### 5.3 原子操作使用

- 仅在对同一变量的竞争写入时使用
- 在 C++17 中，`#pragma omp atomic` 确保基本类型的原子操作
- 对于复杂结构，使用 `#pragma omp critical`

## 6. 性能分析

### 6.1 预期加速比

| 函数 | 单核时间占比 | 并行后预期时间占比 | 预期加速比 |
|------|-------------|-------------------|-----------|
| compute_initial_gains | 30% | 10% | ~3x |
| update_gains_after_move | 40% | 25% | ~1.6x |
| bucket_get_best_node | 15% | 12% | ~1.25x |
| compute_cut_size/violations | 5% | 2% | ~2.5x |
| **总体** | - | - | **~1.8x (4 核)** |

### 6.2 Amdahl 定律分析

假设串行部分占 35%（fm_pass 主循环 + 桶操作），则：
- 理论上界加速比 = 1 / 0.35 ≈ 2.86x
- 实际加速比受限于：同步开销、负载均衡、缓存争用

## 7. 编译和使用

### 7.1 编译命令

```bash
g++ -Wall -Wextra -std=c++17 -O2 -g -fopenmp \
    main.cpp Graph.cpp Net.cpp Node.cpp solution.cpp evaluate.cpp \
    -o main
```

### 7.2 运行时控制

```bash
# 设置线程数为 4
export OMP_NUM_THREADS=4
./main benchmark.hgr

# 或在代码中设置
solution.set_num_threads(4);
```

## 8. 未来改进方向

1. **SIMD 向量化**：对增益计算使用 AVX2/AVX-512 指令集
2. **GPU 加速**：将 `compute_initial_gains` 等 embarrassingly parallel 任务迁移到 GPU
3. **批量移动**：研究允许少量并行移动的 FM 变体算法
4. **锁_free 数据结构**：使用无锁队列替代桶结构

## 9. 总结

本次并行化遵循"能并行的地方大胆并行，有依赖的地方保守串行"的原则，在保证算法正确性的前提下实现了适度的性能提升。主要成果包括：

- ✅ 所有独立的遍历计算已并行化（`compute_*` 系列函数）
- ✅ 增益更新中的独立计算部分已并行化
- ✅ 使用了条件并行避免小数据集的性能退化
- ⚠️ FM 主迭代循环保持串行以保证收敛性
- ⚠️ 桶结构操作保持串行以确保数据一致性
