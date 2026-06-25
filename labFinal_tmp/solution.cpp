#include "solution.h"

void Solution::read_benchmark(Graph &graph, string benchmark_name) {
    ifstream file(benchmark_name);
    if (!file.is_open()) { cerr << "Failed to open the file!" << endl; exit(-1); }
    int edge_num, node_num;
    string line;
    getline(file >> ws, line);
    istringstream iss(line);
    iss >> edge_num >> node_num;
    for (int i = 0; i < edge_num; i++) {
        getline(file, line);
        istringstream iss(line);
        int node_id;
        Net *net = graph.add_net(i);
        while (iss >> node_id) {
            Node *node = graph.get_or_create_node(node_id);
            node->add_net(net);
            net->add_node(node);
        }
    }
    file.close();
}

void Solution::bucket_insert(int node_id) {
    int P = part_[node_id];
    int idx = max_gain_[node_id] + gain_offset_;
    bucket_prev_[node_id] = -1;
    bucket_next_[node_id] = bucket_heads_[P][idx];
    if (bucket_heads_[P][idx] != -1) {
        bucket_prev_[bucket_heads_[P][idx]] = node_id;
    }
    bucket_heads_[P][idx] = node_id;
    if (max_gain_[node_id] > max_gain_in_bucket_[P]) {
        max_gain_in_bucket_[P] = max_gain_[node_id];
    }
}

void Solution::bucket_remove(int node_id) {
    int P = part_[node_id];
    int idx = max_gain_[node_id] + gain_offset_;
    if (bucket_prev_[node_id] != -1) {
        bucket_next_[bucket_prev_[node_id]] = bucket_next_[node_id];
    } else {
        bucket_heads_[P][idx] = bucket_next_[node_id];
        if (bucket_heads_[P][idx] == -1 && max_gain_[node_id] == max_gain_in_bucket_[P]) {
            while (max_gain_in_bucket_[P] >= -max_degree_ &&
                   bucket_heads_[P][max_gain_in_bucket_[P] + gain_offset_] == -1) {
                max_gain_in_bucket_[P]--;
            }
        }
    }
    if (bucket_next_[node_id] != -1) bucket_prev_[bucket_next_[node_id]] = bucket_prev_[node_id];
    bucket_next_[node_id] = -1;
    bucket_prev_[node_id] = -1;
}

int Solution::bucket_get_best_node(int &from_side, int &to_side) {
    int best_node = -1;
    int max_valid_gain = -1e9;

    // 遍历所有可能的源块 k
    for (int k = 0; k < K_; ++k) {
        for (int g = max_gain_in_bucket_[k]; g >= -max_degree_; --g) {
            // 极限剪枝：如果当前桶的最高增益都小于我们已经找到的合法增益，直接跳过整个桶！
            if (g <= max_valid_gain) break; 

            int curr = bucket_heads_[k][g + gain_offset_];
            // bool found_in_this_bucket = false;

            while (curr != -1) {
                // 评估当前节点移动到所有目标的收益
                for (int to = 0; to < K_; ++to) {
                    if (to == k) continue;
                    // 平衡约束检查
                    if (current_size_[k] > min_part_size_ && current_size_[to] < max_part_size_) {
                        if (gain_[curr][to] > max_valid_gain) {
                            max_valid_gain = gain_[curr][to];
                            best_node = curr;
                            from_side = k;
                            to_side = to;
                            // found_in_this_bucket = true;
                        }
                    }
                }
                // 如果发现了一个能够直接达到理论上限 g 的移动，无需继续扫描链表
                if (max_valid_gain == g) break;
                curr = bucket_next_[curr];
            }
            if (max_valid_gain == g) break;
        }
    }
    return best_node;
}

void Solution::init_partition(Graph &graph, mt19937 &rng) {
    (void)graph;
    vector<int> perm(total_nodes_);
    iota(perm.begin(), perm.end(), 1);
    shuffle(perm.begin(), perm.end(), rng);
    
    fill(current_size_.begin(), current_size_.end(), 0);
    int node_per_block = total_nodes_ / K_;
    int remainder = total_nodes_ % K_;

    int cur_idx = 0;
    for (int i = 0; i < K_; ++i) {
        int alloc_size = node_per_block + (i < remainder ? 1 : 0);
        for (int j = 0; j < alloc_size; ++j) {
            part_[perm[cur_idx]] = i;
            current_size_[i]++;
            cur_idx++;
        }
    }
}

void Solution::compute_initial_gains(Graph &graph) {
    for (int i = 0; i < graph.get_net_num(); ++i) {
        fill(net_count_[i].begin(), net_count_[i].end(), 0);
    }
    for (Node *node : graph.get_nodes()) {
        int P = part_[node->get_index()];
        for (Net *net : node->get_nets()) {
            net_count_[net->get_index()][P]++;
        }
    }

    for (Node *node : graph.get_nodes()) {
        int nid = node->get_index();
        int P = part_[nid];
        max_gain_[nid] = -1e9;
        
        for (int X = 0; X < K_; ++X) {
            if (X == P) { gain_[nid][X] = 0; continue; }
            int g = 0;
            for (Net *net : node->get_nets()) {
                int net_id = net->get_index();
                // (K-1) metric 的绝对真理：
                // 如果源块只有一个该超网的节点，移走后割代价减 1
                if (net_count_[net_id][P] == 1) g++;
                // 如果目标块之前没有该超网的节点，移入后割代价加 1 (增益 -1)
                if (net_count_[net_id][X] == 0) g--;
            }
            gain_[nid][X] = g;
            if (g > max_gain_[nid]) {
                max_gain_[nid] = g;
                best_dest_[nid] = X;
            }
        }
    }
}

void Solution::build_buckets() {
    for (int s = 0; s < K_; s++) {
        fill(bucket_heads_[s].begin(), bucket_heads_[s].end(), -1);
        max_gain_in_bucket_[s] = -max_degree_ - 1;
    }
    fill(bucket_next_.begin(), bucket_next_.end(), -1);
    fill(bucket_prev_.begin(), bucket_prev_.end(), -1);

    for (int i = 1; i <= total_nodes_; i++) {
        if (!locked_[i]) bucket_insert(i);
    }
}

void Solution::update_gains_after_move(Graph &graph, int moved_node, int from_side, int to_side) {
    Node *base = graph.get_node(moved_node);
    
    for (Net *net : base->get_nets()) {
        int net_id = net->get_index();

        // 1. 移出旧增量：对邻居解除基于旧分布的计算
        for (Node *cell : net->get_nodes()) {
            int cid = cell->get_index();
            if (locked_[cid]) continue;
            int P = part_[cid];
            bucket_remove(cid);
            for (int X = 0; X < K_; ++X) {
                if (X == P) continue;
                int old_contrib = (net_count_[net_id][P] == 1 ? 1 : 0) - (net_count_[net_id][X] == 0 ? 1 : 0);
                gain_[cid][X] -= old_contrib;
            }
        }

        // 2. 更新网表分布
        net_count_[net_id][from_side]--;
        net_count_[net_id][to_side]++;

        // 3. 加上新增量：依据新分布更新邻居增益
        for (Node *cell : net->get_nodes()) {
            int cid = cell->get_index();
            if (locked_[cid]) continue;
            int P = part_[cid];
            max_gain_[cid] = -1e9;
            for (int X = 0; X < K_; ++X) {
                if (X == P) continue;
                int new_contrib = (net_count_[net_id][P] == 1 ? 1 : 0) - (net_count_[net_id][X] == 0 ? 1 : 0);
                gain_[cid][X] += new_contrib;
                
                if (gain_[cid][X] > max_gain_[cid]) {
                    max_gain_[cid] = gain_[cid][X];
                    best_dest_[cid] = X;
                }
            }
            bucket_insert(cid);
        }
    }
}

int Solution::compute_cut_size(Graph &graph) {
    int cut = 0;
    for (int nid = 0; nid < graph.get_net_num(); nid++) {
        int span = 0;
        for (int k = 0; k < K_; k++) {
            if (net_count_[nid][k] > 0) span++;
        }
        if (span > 1) cut += (span - 1); // ICCAD EasyPart (K-1) metric
    }
    return cut;
}

int Solution::fm_pass(Graph &graph, int trial_id, int pass_id, ofstream &csv_file) {
    fill(locked_.begin(), locked_.end(), false);
    compute_initial_gains(graph);
    build_buckets();

    vector<int> moves_node, moves_from, moves_to, gains_at_step;
    int cumulative_gain = 0, best_gain = 0, best_step = 0;

    while (true) {
        int from_side = -1, to_side = -1;
        int chosen = bucket_get_best_node(from_side, to_side);

        if (chosen == -1) break; // 无法再进行合法的移动

        bucket_remove(chosen);
        part_[chosen] = to_side;
        locked_[chosen] = true;
        current_size_[from_side]--;
        current_size_[to_side]++;

        cumulative_gain += gain_[chosen][to_side];
        moves_node.push_back(chosen);
        moves_from.push_back(from_side);
        moves_to.push_back(to_side);
        gains_at_step.push_back(cumulative_gain);

        if (cumulative_gain > best_gain) {
            best_gain = cumulative_gain;
            best_step = moves_node.size();
        }
        update_gains_after_move(graph, chosen, from_side, to_side);
    }

    // 回溯
    for (int i = (int)moves_node.size() - 1; i >= best_step; i--) {
        int node_id = moves_node[i];
        int from = moves_from[i];
        int to = moves_to[i];
        part_[node_id] = from;
        current_size_[to]--;
        current_size_[from]++;
    }

    if (csv_file.is_open()) {
        for (size_t i = 0; i < gains_at_step.size(); i++) {
            csv_file << trial_id << "," << pass_id << "," << i << "," << gains_at_step[i] << "\n";
        }
    }
    return best_gain;
}

void Solution::my_partition_algorithm(Graph &graph, int K, double r, vector<int> &part_result) {
    K_ = K;
    r_ = r;
    total_nodes_ = graph.get_node_num();
    
    max_degree_ = 0;
    for (Node *node : graph.get_nodes()) max_degree_ = max(max_degree_, (int)node->get_nets().size());
    if (max_degree_ < 1) max_degree_ = 1;
    gain_offset_ = max_degree_;

    part_.assign(total_nodes_ + 1, 0);
    gain_.assign(total_nodes_ + 1, vector<int>(K_, 0));
    max_gain_.assign(total_nodes_ + 1, -1e9);
    best_dest_.assign(total_nodes_ + 1, -1);
    locked_.assign(total_nodes_ + 1, false);
    
    bucket_next_.assign(total_nodes_ + 1, -1);
    bucket_prev_.assign(total_nodes_ + 1, -1);

    int num_nets = graph.get_net_num();
    net_count_.assign(num_nets, vector<int>(K_, 0));
    current_size_.assign(K_, 0);

    bucket_heads_.assign(K_, vector<int>(2 * max_degree_ + 1, -1));
    max_gain_in_bucket_.assign(K_, -max_degree_ - 1);

    // 严苛平衡边界控制：确保每个块不低于 min，同时不吃掉其他块的名额
    min_part_size_ = floor(total_nodes_ * r_);
    max_part_size_ = total_nodes_ - (K_ - 1) * min_part_size_;

    int best_cut = INT_MAX;
    vector<int> best_part(total_nodes_ + 1, 0);

    int num_restarts = 30; // 并行化时，你的队友可以直接把这里改写！
    ofstream csv_file("fm_all_logs.csv");
    csv_file << "Trial,Pass,Step,Cumulative_Gain\n";

    for (int trial = 0; trial < num_restarts; trial++) {
        mt19937 rng(trial * 12345 + 42); 
        init_partition(graph, rng);
        
        // 重建超网分布计数
        for (int i = 0; i < num_nets; ++i) fill(net_count_[i].begin(), net_count_[i].end(), 0);
        for (Node *node : graph.get_nodes()) {
            int P = part_[node->get_index()];
            for (Net *net : node->get_nets()) net_count_[net->get_index()][P]++;
        }

        int pass = 0;
        while (true) {
            int prev_cut = compute_cut_size(graph);
            int best_pass_gain = fm_pass(graph, trial, pass, csv_file);
            
            // Re-sync
            for (int i = 0; i < num_nets; ++i) fill(net_count_[i].begin(), net_count_[i].end(), 0);
            for (Node *node : graph.get_nodes()) {
                int P = part_[node->get_index()];
                for (Net *net : node->get_nets()) net_count_[net->get_index()][P]++;
            }

            int curr_cut = compute_cut_size(graph);
            if (pass == 0 && trial == 0) {
                cout << "Trial " << trial << " Pass " << pass << ": cut = " << curr_cut 
                     << ", improvement = " << (prev_cut - curr_cut) << endl;
            }
            if (best_pass_gain <= 0 || (prev_cut - curr_cut) <= 0) break;
            pass++;
        }

        int final_cut = compute_cut_size(graph);
        if (final_cut < best_cut) {
            best_cut = final_cut;
            best_part = part_;
        }
    }
    
    csv_file.close();
    part_result = best_part;
}