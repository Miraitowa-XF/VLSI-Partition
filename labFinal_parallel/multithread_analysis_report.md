# 多线程性能问题分析报告

## 问题概述

当前项目虽然实现了并行化代码，但**并没有获得加速效果**。经过代码分析，主要原因如下：

---

## 核心问题：FM 算法的本质是串行依赖

### 1. FM 主循环无法并行化

查看 [`fm_pass()`](../labFinal/solution.cpp:894) 函数，这是整个算法的时间瓶颈所在：

```cpp
while (true) {
    int chosen = bucket_get_best_node(from_side, to_side, graph);  // ← 步骤 A
    if (chosen == -1) break;
    bucket_remove(chosen); part_[chosen] = to_side; locked_[chosen] = true;  // ← 步骤 B
    current_size_[from_side]--; current_size_[to_side]++;
    cumulative_gain += gain_[chosen][to_side];
    cumulative_topo += topo_delta_[chosen][to_side];
    update_gains_after_move(graph, chosen, from_side, to_side);  // ← 步骤 C（关键！）
    
    // 下一步必须等待上一步完成！
}
```

**问题在于 `update_gains_after_move()`**：每次移动节点后，需要更新其邻居节点的增益值。而 `bucket_get_best_node()` 又依赖于这些更新后的增益值来选择下一个要移动的节点。这形成了**严格的数据依赖链**，无法并行执行。

### 2. 已并行化的部分占时太少

当前只有以下三个地方实现了并行化：

| 函数 | 功能 | 占比估计 |
|------|------|----------|
| [`compute_initial_gains_parallel()`](../labFinal/solution.cpp:19) | 初始增益计算 | < 5% |
| [`compute_cut_size_parallel()`](../labFinal/solution.cpp:94) | Cut size 计算 | < 1% |
| [`compute_violations_parallel()`](../labFinal/solution.cpp:142) | 违规数计算 | < 1% |

根据代码结构，主要时间消耗在 FM 主循环上（`bucket_get_best_node` + `update_gains_after_move`），这部分完全串行。

---

## 并行开销对比

即使数据量足够大，当前的并行实现也面临以下开销问题：

### 线程创建/销毁开销
```cpp
vector<future<void>> futures;
for (int t = 0; t < num_threads; t++) {
    futures.emplace_back(async(std::launch::async, [...](){ ... }));
}
// 每个 future 都要创建线程，有显著开销
```

### 同步等待开销
```cpp
for (auto& f : futures) {
    f.get();  // 强制等待所有线程完成
}
```

当单个任务的工作量不足以抵消上述开销时，多线程反而会比串行慢。

---

## 证据：代码中的条件判断

在多个并行函数的开头都有这样的检查：

```cpp
// compute_initial_gains_parallel()
if (num_threads <= 1 || num_nodes < PARALLEL_MIN_TASK_SIZE) {
    compute_initial_gains_serial(graph);
    return;
}

// compute_cut_size_parallel()  
if (num_threads <= 1 || num_nets < PARALLEL_MIN_TASK_SIZE) {
    return compute_cut_size_serial(graph);
}
```

这说明实现者已经意识到：**当数据量太小时，并行化反而有害**。

---

## 为什么时间都差不多？

综合以上因素：

1. **主要计算部分是串行的** → 多核无法加速
2. **并行化的部分占比很小** → 对总时间影响微乎其微
3. **小数据集下并行开销 > 收益** → 甚至可能更慢
4. **FM 算法迭代性质决定难以并行** → 每一步依赖前一步结果

因此，无论设置多少线程，总时间主要由串行部分组成，导致**加速比接近 1:1**。

---

## 解决方案建议

### 方案 1：使用并行化更强的划分算法

考虑使用与 FM 不同的算法范式：

| 算法 | 并行友好度 | 说明 |
|------|-----------|------|
| Multilevel Kernighan-Lin | ★★★★★ | 分层 + 并行粗化/精细化的方案 |
| Spectral Partitioning | ★★★★★ | 基于特征向量计算，天然可并行 |
| Genetic Algorithm | ★★★★★ | 种群进化，个体独立评估 |

### 方案 2：批处理优化（Limited Parallel FM）

不追求每步最优，而是批量选择多个候选节点同时移动：

```cpp
// 伪代码：并行批次 FM
for (int batch = 0; batch < num_batches; batch++) {
    // 并行：每个线程选择一个候选节点
    auto results = parallel_for_each_thread([&]() {
        return find_local_best_node(thread_id);
    });
    
    // 串行：合并冲突，应用移动
    merge_and_apply(results);
}
```

这种方式可以牺牲一定解的质量换取加速。

### 方案 3：针对大数据集优化现有代码

如果数据集确实很大（如数万节点以上），可以考虑：

1. **使用 OpenMP 替代 std::future**（更低的开销）
2. **使用 TBB 等并发库**（更好的负载均衡）
3. **NUMA 感知的内存布局优化**

---

## 验证方法

若要验证分析正确性，可以在 [`fm_pass()`](../labFinal/solution.cpp:894) 中添加详细计时：

```cpp
#include <chrono>

void Solution::fm_pass(...) {
    auto t0 = chrono::high_resolution_clock::now();
    
    compute_initial_gains(graph);  // 这里用并行版本
    
    auto t1 = chrono::high_resolution_clock::now();
    cout << "init_gains: " << chrono::duration<double>(t1-t0).count() << "s" << endl;
    
    // ... main loop ...
    
    auto t2 = chrono::high_resolution_clock::now();
    cout << "main_loop: " << chrono::duration<double>(t2-t1).count() << "s" << endl;
}
```

预期输出会显示 `main_loop` 占总时间的 90% 以上。

---

## 结论

**当前多线程实现无效的根本原因**：FM 划分的核心算法是强顺序依赖的，而并行化的部分只是辅助计算，占比太小。要达到有效加速，需要从根本上改变算法设计思路。

---

## 参考资料

- [Kernighan-Lin Algorithm](https://en.wikipedia.org/wiki/Kernighan%E2%80%93Lin_algorithm)
- [Multilevel K-way Graph Partitioning](https://epubs.siam.org/doi/10.1137/S0036144598336764)
- [OpenMP Guidelines for Performance](https://www.openmp.org/resources/openmp-guidelines-for-performance/)
