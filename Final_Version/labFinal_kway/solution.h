#ifndef SOLUTION_H
#define SOLUTION_H

#include <string>
#include "Graph.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <random>
#include <algorithm>
#include <climits>
#include <cmath>

using namespace std;

class Solution {
public:
    void read_benchmark(Graph &graph, string benchmark_name);
    
    // K: 划分路数, r: 平衡比例
    void my_partition_algorithm(Graph &graph, int K, double r, vector<int> &part_result);

private:
    int K_;
    double r_;
    int total_nodes_;
    int max_degree_;
    int gain_offset_;

    // FM 核心数据
    vector<int> part_;                     // 节点当前的块 ID [0, K-1]
    vector<vector<int>> gain_;             // gain_[u][X]: 节点 u 移动到块 X 的增益
    vector<int> max_gain_;                 // max_gain_[u]: 节点 u 到所有合法目标块的最大增益
    vector<int> best_dest_;                // best_dest_[u]: 取得最大增益的目标块
    vector<bool> locked_;                  // 记录节点是否已锁定

    vector<vector<int>> net_count_;        // net_count_[net_id][block_id]: 超网在各个块中的节点数
    vector<int> current_size_;             // current_size_[block_id]: 各个块当前的节点总数

    // K 路桶结构：bucket_heads_[block][gain]
    vector<vector<int>> bucket_heads_;
    vector<int> bucket_next_;
    vector<int> bucket_prev_;
    vector<int> max_gain_in_bucket_;       // max_gain_in_bucket_[block]: 该块内最大的 gain 索引

    int min_part_size_;
    int max_part_size_;

    void init_partition(Graph &graph, mt19937 &rng);
    void compute_initial_gains(Graph &graph);
    void build_buckets();
    void bucket_insert(int node_id);
    void bucket_remove(int node_id);
    
    // K-way 核心：在所有块中寻找全局最大的合法移动
    int bucket_get_best_node(int &from_side, int &to_side);
    
    // K-way 核心：更新受影响邻居的增益 (K-1 Metric)
    void update_gains_after_move(Graph &graph, int moved_node, int from_side, int to_side);
    int fm_pass(Graph &graph, int trial_id, int pass_id, ofstream &csv_file);
    int compute_cut_size(Graph &graph);
};

#endif