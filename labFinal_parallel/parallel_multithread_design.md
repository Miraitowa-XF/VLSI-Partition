# FM算法多线程并行化设计思路

## 1. 并行化策略分析

### 1.1 FM算法的特点

FM (Fiduccia-Mattheyses) 算法是一种经典的划分算法，其核心特点包括：

1. **贪婪移动机制**: 每次选择增益最大的节点进行移动
2. **增量更新**: 每次移动后只更新受影响节点的增益
3. **多Pass迭代**: 一个Pass结束后回溯到最优状态，再进行下一个Pass
4. **随机重启**: 从不同的初始划分开始多次试验，选择最优结果

### 1.2 可并行化的部分分析

FM算法中有三个主要的可并行化层次：

| 并行化方式 | 描述 | 适用场景 | 线程同步复杂度 |
|-----------|------|---------|---------------|
| **试验级并行** | 多个线程同时运行独立的试验 | 探索多个初始划分 | 低 |
| **Pass级并行** | 同一试验内多个Pass并行 | 大规模划分问题 | 中 |
| **增益更新并行** | 单次移动内的增益更新并行 | 超大规模网络 | 高 |

我们选择了**试验级并行**作为主要策略，原因是：

1. **最低的同步开销**: 不同试验之间完全独立，不需要频繁同步
2. **最佳的并行效率**: 可以充分利用多核CPU，几乎没有线程等待
3. **实现简单**: 不需要复杂的锁机制和数据依赖处理

## 2. 并行化实现设计

### 2.1 整体架构

```
┌─────────────────────────────────────────────────────┐
│                   主线程                             │
│  - 初始化共享数据 (拓扑约束、候选FPGA传播)            │
│  - 创建工作线程                                     │
│  - 监控进度                                        │
│  - 收集最优结果                                    │
└─────────────────────────────────────────────────────┘
                        │
        ┌───────────────┼───────────────┐
        │               │               │
        ▼               ▼               ▼
┌───────────┐   ┌───────────┐   ┌───────────┐
│ Worker #0 │   │ Worker #1 │   │ Worker #2 │ ...
│           │   │           │   │           │
│ Trial 0-3 │   │ Trial 4-7 │   │ Trial 8-11│
│ 独立数据  │   │ 独立数据  │   │ 独立数据  │
└───────────┘   └───────────┘   └───────────┘
        │               │               │
        └───────────────┴───────────────┘
                        │
                        ▼
        ┌───────────────────────────────┐
        │    互斥锁保护的全局最优结果      │
        │    - best_cut                  │
        │    - best_violations           │
        │    - best_part                 │
        └───────────────────────────────┘
```

### 2.2 线程局部数据结构

每个工作线程拥有独立的局部数据结构，避免数据竞争：

```cpp
struct ThreadLocalData {
    vector<int> part;              // 局部划分结果
    vector<vector<int>> gain;      // 增益数组
    vector<bool> locked;           // 锁定标记
    vector<vector<int>> net_count; // 网计数
    vector<int> current_size;      // 各块大小
    
    // 桶结构（独立副本）
    vector<vector<int>> bucket_heads;
    vector<int> bucket_next;
    vector<int> bucket_prev;
    vector<bool> in_bucket;
    
    mt19937 rng;                   // 独立随机数生成器
};
```

### 2.3 共享数据结构

以下数据在所有线程间共享（只读，无需锁）：

```cpp
struct SharedReadOnlyData {
    int K_;                        // FPGA数量
    int total_nodes_;              // 总节点数
    int max_degree_;               // 最大度
    int gain_offset_;              // 桶偏移
    
    vector<vector<bool>> topo_adj; // 拓扑邻接矩阵
    vector<vector<int>> topo_dist; // 拓扑距离
    vector<int> min_size_;         // 最小块大小
    vector<int> max_size_;         // 最大块大小
    vector<bool> is_fixed;         // 固定节点标记
    vector<int> fixed_fpga;        // 固定节点FPGA
    vector<vector<bool>> Cddt_mask;// 候选FPGA掩码
};
```

### 2.4 同步机制

使用最小化的同步机制：

1. **互斥锁 (`std::mutex`)**: 保护全局最优结果更新
2. **原子变量 (`std::atomic<bool>`)**: 用于提前终止信号
3. **原子计数器 (`std::atomic<int>`)**: 用于进度报告

```cpp
mutex best_result_mutex_;           // 保护最优结果
atomic<bool> should_stop(false);    // 提前终止信号
atomic<int> completed_trials(0);    // 完成的试验计数
```

## 3. 关键实现细节

### 3.1 线程函数设计

每个线程执行多个独立的试验：

```cpp
void run_single_trial(Graph &graph, int trial_id, unsigned int seed,
                      vector<int> &local_part, int &local_cut, int &local_violations,
                      atomic<bool> &should_stop) {
    // 1. 初始化独立的数据结构
    // 2. 执行初始划分（贪心重平衡）
    // 3. 运行多次FM Pass直到收敛
    // 4. 执行违规修复
    // 5. 返回局部结果
}
```

### 3.2 全局最优结果更新

每次试验完成后，安全地更新全局最优：

```cpp
{
    lock_guard<mutex> lock(result_mutex);
    if (local_violations == 0 && local_cut < best_cut) {
        best_cut = local_cut;
        best_violations = local_violations;
        best_part = local_part;
    } else if (local_violations < best_violations) {
        // 即使cut不是最优，违规更少也优先
        best_cut = local_cut;
        best_violations = local_violations;
        best_part = local_part;
    }
}
```

### 3.3 提前终止机制

当找到完美解（零违规、零cut）时，可以提前终止：

```cpp
if (local_violations == 0 && local_cut == 0) {
    should_stop.store(true);
}
```

每个线程在关键循环中检查终止信号：

```cpp
while (!should_stop.load()) {
    // FM Pass循环
}
```

## 4. 拓扑约束的处理

### 4.1 候选FPGA传播（一次性）

候选FPGA传播 (`propagate_candidates_with_graph`) 是一个BFS过程，结果对所有试验相同，因此只需在主线程执行一次：

```cpp
propagate_candidates_with_graph(graph);  // 主线程执行
cout << "Propagation done." << endl;

// 然后创建工作线程
for (int t = 0; t < num_threads; t++) {
    workers.emplace_back(...);
}
```

### 4.2 拓扑违规增量计算

每个线程独立维护自己的 `topo_delta` 数组，计算节点移动对违规的影响：

```cpp
auto compute_topo_delta_local = [&](int node, int target_block) -> int {
    int old_block = part[node];
    int delta = 0;
    for (Net *net : graph.get_node(node)->get_nets()) {
        // 计算移动前后违规变化
    }
    return delta;
};
```

## 5. 理论分析

### 5.1 并行效率分析

假设：
- 单线程运行一次试验的时间为 T
- 线程数为 P
- 每个线程运行的试验数为 N/P

理论加速比：S ≈ P（接近理想加速比）

原因：
- 各试验完全独立，无数据依赖
- 共享数据只读，无需同步
- 只有结果更新需要锁，频率很低

### 5.2 工作负载分配

每个线程分配多个试验的原因：

1. **负载均衡**: 某些试验可能提前收敛，分配多个试验可以平衡负载
2. **减少同步开销**: 每个试验完成后才更新一次最优结果
3. **更好的搜索覆盖**: 每个线程探索多个初始状态

### 5.3 内存开销分析

每个线程需要独立的划分数据结构，总内存开销约为：

Memory = Shared_Data + P × Thread_Local_Data

对于大规模问题（10万节点，16 FPGA）：
- Shared_Data: ~10MB
- Thread_Local_Data: ~50MB per thread
- 8线程总开销: ~420MB

这是可接受的，因为划分问题通常在现代服务器上有足够内存。

## 6. 使用方法

### 6.1 编译

```bash
cd labFinal
make clean
make
```

Makefile已添加 `-pthread` 选项。

### 6.2 运行

单线程模式（默认）：
```bash
./main ./Titan23Benchmarks/segmentation ./FPGAGraph/MFS1 16
```

多线程模式（指定线程数）：
```bash
./main ./Titan23Benchmarks/segmentation ./FPGAGraph/MFS1 16 8
```

参数说明：
- 第1参数: 网表文件路径
- 第2参数: 拓扑文件路径
- 第3参数: FPGA数量 (K)
- 第4参数: 线程数 (可选，0表示单线程，>0表示多线程)

### 6.3 线程数建议

| 问题规模 | 建议线程数 | 理由 |
|---------|-----------|------|
| 小 (< 1万节点) | 1-2 | 试验时间短，多线程开销相对较高 |
| 中 (1-10万节点) | 4-8 | 平衡效率和资源使用 |
| 大 (> 10万节点) | 8-16 | 充分利用多核，试验时间长 |

## 7. 总结

本并行化设计采用**试验级并行**策略，具有以下优点：

1. **高并行效率**: 接近理想的线性加速比
2. **低同步开销**: 只需互斥锁保护最优结果
3. **实现简洁**: 线程间无复杂的数据依赖
4. **拓扑兼容**: 完整保留拓扑约束处理机制
5. **灵活控制**: 可通过命令行参数控制线程数

该设计特别适合需要多次随机重启的划分问题，能够显著缩短求解时间，尤其在大规模FPGA划分问题上效果显著。