#include "solution.h"
#include <queue>

void Solution::read_benchmark(Graph &graph, string benchmark_name) {
    // 兼容原有的无拓扑读取
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

void Solution::read_topo_file(string topo_file) {
    ifstream f(topo_file);
    if (!f.is_open()) { cerr << "Failed to open Topo file!" << endl; exit(-1); }
    int edges;
    f >> K_ >> edges; 
    topo_adj_.assign(K_, vector<bool>(K_, false));
    for (int i = 0; i < K_; i++) topo_adj_[i][i] = true; 
    
    for (int i = 0; i < edges; i++) {
        int u, v; f >> u >> v;
        topo_adj_[u][v] = true;
        topo_adj_[v][u] = true;
    }
    f.close();
}

void Solution::read_topopart_benchmark(Graph &graph, string bench_file) {
    ifstream f(bench_file);
    if (!f.is_open()) { cerr << "Failed to open Benchmark file!" << endl; exit(-1); }
    int node_num, edge_num;
    f >> node_num >> edge_num;
    
    for (int i = 0; i < edge_num; i++) {
        int u, v; f >> u >> v;
        Net *net = graph.add_net(i);
        Node *nu = graph.get_or_create_node(u);
        Node *nv = graph.get_or_create_node(v);
        nu->add_net(net); nv->add_net(net);
        net->add_node(nu); net->add_node(nv);
    }
    
    int max_idx = graph.get_max_node_index();
    is_fixed_.assign(max_idx + 1, false);
    fixed_part_.assign(max_idx + 1, -1);

    string line;
    getline(f, line); 
    for (int k = 0; k < K_; k++) {
        if (!getline(f, line)) break;
        istringstream iss(line);
        int node_id;
        while (iss >> node_id) {
            is_fixed_[node_id] = true;
            fixed_part_[node_id] = k;
        }
    }
    f.close();
}

// 拓扑一票否决函数
bool Solution::is_move_valid_topo(Graph &graph, int u, int to_side) {
    Node *node = graph.get_node(u);
    if (node == nullptr) return true;
    for (Net *net : node->get_nets()) {
        for (Node *v_node : net->get_nodes()) {
            int v = v_node->get_index();
            if (v == u) continue; 
            int v_part = part_[v];
            if (!topo_adj_[to_side][v_part]) {
                return false;
            }
        }
    }
    return true;
}

// 核心修复 1：拒绝将彻底无路可走的节点放入桶中，杜绝数组越界！
void Solution::bucket_insert(int node_id) {
    if (max_gain_[node_id] <= -1e8) return; // 被拓扑锁死，禁止入桶
    
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

// 核心修复 2：同步保护移除逻辑
void Solution::bucket_remove(int node_id) {
    if (max_gain_[node_id] <= -1e8) return; // 根本不在桶里，安全返回
    
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

int Solution::bucket_get_best_node(Graph &graph, int &from_side, int &to_side) {
    int best_node = -1;
    int max_valid_gain = -1e9;

    for (int k = 0; k < K_; ++k) {
        for (int g = max_gain_in_bucket_[k]; g >= -max_degree_; --g) {
            if (g <= max_valid_gain) break; 
            int curr = bucket_heads_[k][g + gain_offset_];

            while (curr != -1) {
                for (int to = 0; to < K_; ++to) {
                    if (to == k) continue;
                    if (current_size_[k] > min_part_size_ && current_size_[to] < max_part_size_) {
                        if (gain_[curr][to] > max_valid_gain) {
                            if (is_move_valid_topo(graph, curr, to)) {
                                max_valid_gain = gain_[curr][to];
                                best_node = curr;
                                from_side = k;
                                to_side = to;
                            }
                        }
                    }
                }
                if (max_valid_gain == g) break;
                curr = bucket_next_[curr];
            }
            if (max_valid_gain == g) break;
        }
    }
    return best_node;
}

// 核心修复 3：完美处理孤岛网络的 BFS 遍历
void Solution::init_partition_topo(Graph &graph) {
    fill(current_size_.begin(), current_size_.end(), 0);
    fill(part_.begin(), part_.end(), -1);
    
    queue<int> q;
    
    auto bfs_step = [&]() {
        while (!q.empty()) {
            int u = q.front(); q.pop();
            Node *node = graph.get_node(u);
            if (node == nullptr) continue;
            int u_part = part_[u];

            for (Net *net : node->get_nets()) {
                for (Node *v_node : net->get_nodes()) {
                    int v = v_node->get_index();
                    if (part_[v] == -1) { 
                        int best_k = -1;
                        if (current_size_[u_part] < max_part_size_) best_k = u_part;
                        else {
                            for (int k = 0; k < K_; k++) {
                                if (topo_adj_[u_part][k] && current_size_[k] < max_part_size_) {
                                    best_k = k; break;
                                }
                            }
                        }
                        if (best_k == -1) {
                            for (int k = 0; k < K_; k++) {
                                if (current_size_[k] < max_part_size_) { best_k = k; break; }
                            }
                        }
                        part_[v] = best_k;
                        current_size_[best_k]++;
                        q.push(v);
                    }
                }
            }
        }
    };

    for (int i = 1; i <= total_nodes_; i++) {
        if (is_fixed_[i]) {
            int k = fixed_part_[i];
            part_[i] = k;
            current_size_[k]++;
            q.push(i);
        }
    }
    bfs_step(); // 扩散固定节点
    
    for (int i = 1; i <= total_nodes_; i++) {
        if (part_[i] == -1) {
            for (int k = 0; k < K_; k++) {
                if (current_size_[k] < max_part_size_) {
                    part_[i] = k; current_size_[k]++; break;
                }
            }
            q.push(i);
            bfs_step(); // 处理没有连接到固定节点的孤岛网络
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

    for (int nid = 1; nid <= total_nodes_; nid++) {
        int P = part_[nid];
        max_gain_[nid] = -1e9;
        best_dest_[nid] = -1;

        Node *node = graph.get_node(nid);
        if (node == nullptr) {
            for (int X = 0; X < K_; ++X) gain_[nid][X] = 0;
            max_gain_[nid] = 0;
            best_dest_[nid] = (P + 1) % K_;
            continue;
        }

        for (int X = 0; X < K_; ++X) {
            if (X == P) { gain_[nid][X] = 0; continue; }
            int g = 0;
            for (Net *net : node->get_nets()) {
                int net_id = net->get_index();
                if (net_count_[net_id][P] == 1) g++;
                if (net_count_[net_id][X] == 0) g--;
            }
            gain_[nid][X] = g;
            
            // 计算最大合法增益
            if (is_move_valid_topo(graph, nid, X)) {
                if (g > max_gain_[nid]) {
                    max_gain_[nid] = g;
                    best_dest_[nid] = X;
                }
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
        if (!locked_[i] && !is_fixed_[i]) bucket_insert(i);
    }
}

void Solution::update_gains_after_move(Graph &graph, int moved_node, int from_side, int to_side) {
    Node *base = graph.get_node(moved_node);
    if (base == nullptr) return;

    for (Net *net : base->get_nets()) {
        int net_id = net->get_index();

        for (Node *cell : net->get_nodes()) {
            int cid = cell->get_index();
            if (locked_[cid]) continue;
            bucket_remove(cid); // 使用旧的 max_gain 移除
            
            int P = part_[cid];
            for (int X = 0; X < K_; ++X) {
                if (X == P) continue;
                int old_contrib = (net_count_[net_id][P] == 1 ? 1 : 0) - (net_count_[net_id][X] == 0 ? 1 : 0);
                gain_[cid][X] -= old_contrib;
            }
        }

        net_count_[net_id][from_side]--;
        net_count_[net_id][to_side]++;

        for (Node *cell : net->get_nodes()) {
            int cid = cell->get_index();
            if (locked_[cid]) continue;
            int P = part_[cid];
            max_gain_[cid] = -1e9;
            
            for (int X = 0; X < K_; ++X) {
                if (X == P) continue;
                int new_contrib = (net_count_[net_id][P] == 1 ? 1 : 0) - (net_count_[net_id][X] == 0 ? 1 : 0);
                gain_[cid][X] += new_contrib;
                
                // 重新评估最高合法增益
                if (is_move_valid_topo(graph, cid, X)) {
                    if (gain_[cid][X] > max_gain_[cid]) {
                        max_gain_[cid] = gain_[cid][X];
                        best_dest_[cid] = X;
                    }
                }
            }
            bucket_insert(cid); // 若变为合法，则重新入桶
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
        if (span > 1) cut += (span - 1); 
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
        int chosen = bucket_get_best_node(graph, from_side, to_side);

        if (chosen == -1) break; 

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

void Solution::my_partition_algorithm(Graph &graph, int K, double r, vector<int> &part_result, string topo_file, string bench_file) {
    read_topo_file(topo_file);
    read_topopart_benchmark(graph, bench_file);

    K_ = K;
    r_ = r;
    total_nodes_ = graph.get_max_node_index();
    
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

    min_part_size_ = floor(total_nodes_ * r_);
    max_part_size_ = total_nodes_ - (K_ - 1) * min_part_size_;

    int best_cut = INT_MAX;
    vector<int> best_part(total_nodes_ + 1, 0);

    int num_restarts = 5; // 先跑 5 次验证，速度快了以后随时可以调到 30
    ofstream csv_file("fm_all_logs.csv");
    csv_file << "Trial,Pass,Step,Cumulative_Gain\n";

    for (int trial = 0; trial < num_restarts; trial++) {
        mt19937 rng(trial * 12345 + 42); 
        init_partition_topo(graph);
        
        for (int i = 0; i < num_nets; ++i) fill(net_count_[i].begin(), net_count_[i].end(), 0);
        for (Node *node : graph.get_nodes()) {
            int P = part_[node->get_index()];
            for (Net *net : node->get_nets()) net_count_[net->get_index()][P]++;
        }

        int pass = 0;
        while (true) {
            int prev_cut = compute_cut_size(graph);
            int best_pass_gain = fm_pass(graph, trial, pass, csv_file);
            
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
        cout << "Trial " << trial << " final cut = " << final_cut << endl;
        if (final_cut < best_cut) {
            best_cut = final_cut;
            best_part = part_;
        }
    }
    
    csv_file.close();
    part_result = best_part;
}