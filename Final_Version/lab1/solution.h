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
    // FM 核心数据
    vector<int> part_; // 节点划分状态，0 表示节点 i 在 X 集合, 1 表示在 Y 集合
    vector<int> gain_; // 节点增益
    vector<bool> locked_; // 记录节点是否已锁定

    // 超网分布计数，用于快速计算增益和 cut size
    vector<int> net_count_X_; // 超网 nid 在 X 中的节点数
    vector<int> net_count_Y_; // 超网 nid 在 Y 中的节点数

    // 桶结构：[0] 存放 X 侧节点(按移动到 Y 的增益), [1] 存放 Y 侧节点
    vector<int> bucket_heads_[2];
    // 双向链表指针
    vector<int> bucket_next_;
    vector<int> bucket_prev_;

    int max_gain_[2]; // 当前该侧非空桶的最大增益值
    int gain_offset_; // 最大增益绝对值 (即 max_degree)，用于桶数组索引偏移
    int max_degree_; // 任意节点的最大超网度数 (用于确定桶大小)

    // 平衡约束
    int min_part_size_;   // ceil(0.48 * N)
    int max_part_size_;   // floor(0.52 * N)
    int total_nodes_;
    int current_X_size_;

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
    // 计算当前 cut size
    int compute_cut_size(int num_nets);
};

#endif
