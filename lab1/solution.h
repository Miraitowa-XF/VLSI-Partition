#ifndef SOLUTION_H
#define SOLUTION_H

#include <string>
#include "Graph.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <set>
#include <vector>
#include <random>
#include <algorithm>
#include <climits>

using namespace std;

class Solution {
public:
    void read_benchmark(Graph &graph, string benchmark_name);
    void my_partition_algorithm(Graph &graph, set<int> &X, set<int> &Y);

private:
    // ========== FM 核心数据 ==========
    // part_[i] = 0 表示节点 i 在 X 集合, 1 表示在 Y 集合 (1-indexed)
    vector<int> part_;
    // gain_[i] = 节点 i 移动到另一侧时的 cut size 减少量
    vector<int> gain_;
    // locked_[i] = true 表示节点 i 在当前 pass 中已被移动过
    vector<bool> locked_;

    // ========== 超网分布计数 ==========
    // net_count_X_[nid] = 超网 nid 在 X 中的节点数
    vector<int> net_count_X_;
    // net_count_Y_[nid] = 超网 nid 在 Y 中的节点数
    vector<int> net_count_Y_;

    // ========== 桶结构 (Bucket Array) ==========
    // 两套桶: [0] 存放 X 侧节点(按移动到 Y 的增益), [1] 存放 Y 侧节点(按移动到 X 的增益)
    // bucket_heads_[side][gain + offset] = 链表头节点 id, -1 表示空
    vector<int> bucket_heads_[2];
    // bucket_next_[node_id] / bucket_prev_[node_id] = 双向链表指针
    vector<int> bucket_next_;
    vector<int> bucket_prev_;
    // max_gain_[side] = 当前该侧非空桶的最大增益值
    int max_gain_[2];
    // gain_offset_ = max_degree, 用于将增益值映射到数组下标
    int gain_offset_;
    // max_degree_ = 任意节点的最大超网度数
    int max_degree_;

    // ========== 平衡约束 ==========
    int min_part_size_;   // ceil(0.48 * N)
    int max_part_size_;   // floor(0.52 * N)
    int total_nodes_;
    int current_X_size_;

    // ========== 核心方法 ==========
    // 初始化划分 (随机将节点分为 X 和 Y, 满足平衡约束)
    void init_partition(Graph &graph, mt19937 &rng);
    // 计算所有节点的初始增益
    void compute_initial_gains(Graph &graph);
    // 构建桶结构 (将所有未锁定节点插入对应桶)
    void build_buckets();
    // 桶操作: 插入节点
    void bucket_insert(int node_id, int side);
    // 桶操作: 移除节点
    void bucket_remove(int node_id, int side);
    // 桶操作: 获取该侧满足平衡约束的最大增益节点
    int bucket_get_best_node(int side);
    // 更新节点增益并维护桶
    void update_node_gain(int node_id, int new_gain, int side);
    // 执行一次 FM pass
    void fm_pass(Graph &graph);
    // 局部增益更新: 移动节点后更新受影响邻居的增益
    void update_gains_after_move(Graph &graph, int moved_node, int from_side);
    // 计算当前 cut size (基于 net_count_X_/Y_)
    int compute_cut_size(int num_nets);
};

#endif
