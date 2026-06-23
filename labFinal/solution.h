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
    void my_partition_algorithm(Graph &graph, int K, double r, vector<int> &part_result, string topo_file, string bench_file);

private:
    int K_;
    double r_;
    int total_nodes_;
    int max_degree_;
    int gain_offset_;

    // FM 核心数据
    vector<int> part_;                     
    vector<vector<int>> gain_;             
    vector<int> max_gain_;                 
    vector<int> best_dest_;                
    vector<bool> locked_;                  

    vector<vector<int>> net_count_;        
    vector<int> current_size_;             

    // K 路桶结构
    vector<vector<int>> bucket_heads_;
    vector<int> bucket_next_;
    vector<int> bucket_prev_;
    vector<int> max_gain_in_bucket_;       

    // 拓扑与固定约束
    vector<vector<bool>> topo_adj_;        
    vector<bool> is_fixed_;                
    vector<int> fixed_part_;               

    int min_part_size_;
    int max_part_size_;

    void read_topo_file(string topo_file);
    void read_topopart_benchmark(Graph &graph, string bench_file);
    void init_partition_topo(Graph &graph);
    bool is_move_valid_topo(Graph &graph, int u, int to_side);

    void compute_initial_gains(Graph &graph);
    void build_buckets();
    void bucket_insert(int node_id);
    void bucket_remove(int node_id);
    
    int bucket_get_best_node(Graph &graph, int &from_side, int &to_side);
    void update_gains_after_move(Graph &graph, int moved_node, int from_side, int to_side);
    int fm_pass(Graph &graph, int trial_id, int pass_id, ofstream &csv_file);
    int compute_cut_size(Graph &graph);
};

#endif