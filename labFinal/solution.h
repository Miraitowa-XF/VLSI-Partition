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

#include <queue>
#include <unordered_set>
#include <unordered_map>
#include <set>

using namespace std;

class Solution {
public:
    // 读取benchmark文件并构建图结构（支持拓扑文件 + 网表文件 + 固定节点）
    void read_benchmark(Graph &graph, string benchmark_name);

    // K: 划分路数
    // 多路 FM 划分主入口：通过随机重启 + 多 pass FM 迭代寻找最优 K 路划分
    void my_partition_algorithm(Graph &graph, int K, vector<int> &part_result);

    // 计算拓扑违规数
    int compute_violations(Graph &graph, const vector<int> &part);

    // 获取拓扑邻接矩阵（供 evaluate_violations 使用）
    const vector<vector<bool>>& get_topo_adj() const { return topo_adj; }

    // 读取拓扑文件
    void read_topology(string topo_filename);
    // 读取网表文件和固定节点信息
    void read_netlist_and_fixed(Graph &graph, string netlist_filename);

    // 辅助：将超网图转换为二端图
    void convert_to_two_pin_graph(Graph &graph);

private:
    int K_;
    
    int total_nodes_;
    int max_degree_;
    int gain_offset_;

    // FM 核心数据
    vector<int> part_;                     // 节点当前的块 ID [0, K-1]
    vector<vector<int>> gain_;             // gain_[u][X]: 节点 u 移动到块 X 的增益
    vector<int> max_gain_;                 // max_gain_[u]: 节点 u 到所有合法目标块的最大增益
    vector<int> best_dest_;                // best_dest_[u]: 取得最大增益的目标块
    vector<bool> locked_;                  // 记录节点是否已锁定（FM pass 内锁定）

    vector<vector<int>> net_count_;        // net_count_[net_id][block_id]: 超网在各个块中的节点数
    vector<int> neighbor_seen_;             // 邻居去重用的 epoch token 数组
    int neighbor_epoch_;                    // 当前 epoch
    vector<int> current_size_;             // current_size_[block_id]: 各个块当前的节点总数
    vector<int> min_size_;                 // min_size_[block]: balance lower bound
    vector<int> max_size_;                 // max_size_[block]: balance upper bound
    vector<vector<int>> topo_delta_;        // topo_delta_[node][X]: 节点移到 FPGA X 的拓扑违规变化量
    vector<int> topo_delta_version_;        // topo_delta_version_[node]: epoch of last topo_delta update
    int topo_epoch_;                        // global epoch for lazy topo_delta invalidation

    // K 路桶结构：bucket_heads_[block][gain]
    vector<vector<int>> bucket_heads_;
    vector<int> bucket_next_;
    vector<int> bucket_prev_;
    vector<int> max_gain_in_bucket_;       // max_gain_in_bucket_[block]: 该块内最大的 gain 索引
    vector<bool> in_bucket_;               // 节点是否在桶中

    

    // ===== 拓扑约束相关数据结构 =====
    vector<vector<bool>> topo_adj;         // K x K, topo_adj[i][j] = true 表示 FPGA i 和 j 直连
    vector<vector<int>> topo_dist;         // K x K, 全源最短路径（Floyd-Warshall）
    vector<int> max_dist_;                 // max_dist_[i]: 拓扑图中离 FPGA i 最远的距离
    vector<vector<vector<bool>>> radius_has_;  // radius_has_[f][j][d]: FPGA j 距 FPGA f 不超过 d 跳
    vector<vector<bool>> Cddt_mask;        // Cddt_mask[v][f]: 节点 v 能否放置在 FPGA f
    vector<bool> is_fixed;                 // is_fixed[v]: 节点 v 是否为固定节点
    vector<int> fixed_fpga;                // fixed_fpga[v]: 固定节点 v 被指定的 FPGA

    // 候选FPGA传播（带 Graph 访问版本）
    void propagate_candidates_with_graph(Graph &graph);

    // 计算将节点移动到目标块后拓扑违规数的变化量（负值=减少违规）
    int compute_topo_delta(Graph &graph, int node, int target_block);

    // 初始化划分 (随机将节点分配到 K 个块, 满足平衡约束和拓扑约束)
    // 返回 true 表示成功找到零违规初始划分
    bool init_partition(Graph &graph, mt19937 &rng);
    // 计算所有节点的初始增益
    void compute_initial_gains(Graph &graph);
    // 构建桶结构 (将所有未锁定节点插入对应桶)
    void build_buckets(Graph &graph);
    // 桶操作: 插入节点
    void bucket_insert(int node_id);
    // 桶操作: 移除节点
    void bucket_remove(int node_id);
    // K-way 核心: 在所有 K 个块的桶中寻找全局最大的合法移动 (返回节点 ID, 通过引用返回源块和目标块)
    int bucket_get_best_node(int &from_side, int &to_side, Graph &graph);

    // K-way 核心: 更新受影响邻居的增益 (K-1 Metric)
    void update_gains_after_move(Graph &graph, int moved_node, int from_side, int to_side);
    // 单次 FM Pass
    int fm_pass(Graph &graph, int trial_id, int pass_id, ofstream &csv_file, int &out_best_topo);
    // 计算当前 cut size
    int compute_cut_size(Graph &graph);

};

#endif
