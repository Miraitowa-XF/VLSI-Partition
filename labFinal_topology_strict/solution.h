#ifndef SOLUTION_H
#define SOLUTION_H

#include <algorithm>
#include <climits>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>
#include <cstdint>
#include "Graph.h"

class Solution {
public:
    Solution();

    // 读取 IBM .hgr 或 TopoPart benchmark。若已经 read_topology()，会同时解析固定约束行。
    void read_benchmark(Graph &graph, const std::string &benchmark_name);

    // 读取 MFS/FPGA 拓扑图：第一行 K E，后续 E 行为 FPGA 间无向边。
    bool read_topology(const std::string &topology_name);
    void enable_topology(bool enabled) { topology_enabled_ = enabled && topology_loaded_; }

    int get_topology_K() const { return topology_k_; }
    bool topology_loaded() const { return topology_loaded_; }
    const std::vector<std::vector<int>> &get_topology_allowed() const { return topo_allowed_; }
    const std::vector<std::vector<int>> &get_topology_dist() const { return topo_dist_; }
    const std::vector<int> &get_fixed_part() const { return fixed_part_; }

    // K: 划分路数, r: 每个分区最小占比。r=1/K 时自动使用 floor/ceil 严格均衡。
    void my_partition_algorithm(Graph &graph, int K, double r, std::vector<int> &part_result);

    int count_topology_violations(Graph &graph) const;
    long long compute_topology_hop_cost(Graph &graph) const;

private:
    static constexpr int NEG_INF = -1000000000;
    static constexpr int INF = 1000000000;

    int K_;
    double r_;
    int total_nodes_;
    int max_degree_;
    int gain_offset_;

    bool topology_loaded_;
    bool topology_enabled_;
    int topology_k_;
    std::vector<std::vector<int>> topo_allowed_; // self + physical edge = 1
    std::vector<std::vector<int>> topo_dist_;    // all-pairs shortest path on MFS graph
    std::vector<int> topo_degree_;
    std::vector<std::uint64_t> topo_neighbor_mask_;      // bit p: current FPGA can directly connect to p, including itself
    std::vector<std::uint64_t> topology_candidate_mask_; // topology list candidates propagated from fixed nodes
    std::vector<int> fixed_part_;                // -1 means movable; otherwise fixed FPGA id

    std::vector<int> part_;                      // 内部节点 id -> 分区 id [0, K-1]
    std::vector<std::vector<int>> gain_;         // gain_[u][to]
    std::vector<int> max_gain_;
    std::vector<int> best_dest_;
    std::vector<char> locked_;

    std::vector<std::vector<int>> net_count_;    // net_count_[net_id][part]
    std::vector<int> current_size_;

    std::vector<std::vector<int>> bucket_heads_; // bucket_heads_[from][gain+offset]
    std::vector<int> bucket_next_;
    std::vector<int> bucket_prev_;
    std::vector<int> max_gain_in_bucket_;

    int min_part_size_;
    int max_part_size_;
    int strict_lower_;
    int strict_upper_;

    void initialize_storage(Graph &graph, int K, double r);
    void compute_balance_bounds();

    void init_partition(Graph &graph, std::mt19937 &rng);
    void init_topology_partition(Graph &graph, std::mt19937 &rng);
    void assign_fixed_nodes();
    void build_topology_candidate_masks(Graph &graph);
    std::uint64_t neighbor_union_mask(std::uint64_t mask) const;
    std::vector<int> mask_to_vector(std::uint64_t mask) const;
    std::vector<int> topology_candidates_from_assigned(const Graph &graph, int node_id) const;
    int choose_best_initial_part(Graph &graph, int node_id, const std::vector<int> &candidates) const;
    void rebalance_initial_solution(Graph &graph);
    int repair_topology_violations(Graph &graph, int max_rounds);
    std::vector<int> topology_legal_destinations_for_node(const Graph &graph, int node_id) const;
    int fast_topology_refine(Graph &graph, int max_sweeps);

    void rebuild_net_counts(Graph &graph);
    void compute_initial_gains(Graph &graph);
    void build_buckets();
    void bucket_insert(int node_id);
    void bucket_remove(int node_id);
    int bucket_get_best_node(Graph &graph, int &from_side, int &to_side);
    void update_gains_after_move(Graph &graph, int moved_node, int from_side, int to_side);

    bool is_fixed(int node_id) const;
    bool is_balance_move_legal(int from_side, int to_side) const;
    bool is_topology_move_legal(const Graph &graph, int node_id, int to_side) const;
    bool is_net_topology_valid_after_move(const Net *net, int moved_node, int to_side) const;
    bool is_net_topology_valid(const Net *net) const;
    int count_incident_topology_violations_after_move(const Graph &graph, int node_id, int to_side) const;
    int count_incident_topology_violations(const Graph &graph, int node_id) const;
    int local_cut_delta_if_move(const Graph &graph, int node_id, int to_side) const;
    int local_cut_delta_if_assign(const Graph &graph, int node_id, int to_side) const;

    int fm_pass(Graph &graph, int trial_id, int pass_id, std::ofstream &csv_file);
    int compute_cut_size(Graph &graph) const;
};

#endif
