#include "solution.h"

void Solution::read_topology(string topo_filename) {
    ifstream file(topo_filename);
    if (!file.is_open()) { cerr << "Failed to open topology file: " << topo_filename << endl; exit(-1); }
    int V, E;
    file >> V >> E;
    K_ = V;
    topo_adj.assign(K_, vector<bool>(K_, false));
    topo_dist.assign(K_, vector<int>(K_, INT_MAX / 2));
    for (int i = 0; i < K_; i++) topo_dist[i][i] = 0;
    for (int i = 0; i < E; i++) {
        int u, v;
        file >> u >> v;
        topo_adj[u][v] = topo_adj[v][u] = true;
        topo_dist[u][v] = topo_dist[v][u] = 1;
    }
    file.close();
    for (int k = 0; k < K_; k++)
        for (int i = 0; i < K_; i++)
            for (int j = 0; j < K_; j++)
                topo_dist[i][j] = min(topo_dist[i][j], topo_dist[i][k] + topo_dist[k][j]);
    max_dist_.assign(K_, 0);
    for (int i = 0; i < K_; i++)
        for (int j = 0; j < K_; j++)
            max_dist_[i] = max(max_dist_[i], topo_dist[i][j]);
}

void Solution::read_netlist_and_fixed(Graph &graph, string netlist_filename) {
    ifstream file(netlist_filename);
    if (!file.is_open()) {
        cerr << "Failed to open netlist file: " << netlist_filename << endl;
        exit(-1);
    }

    int N, M;
    file >> N >> M;

    // ----- 读取网表边（支持多端超网） -----
    string dummy;
    getline(file, dummy);

    int net_index = 0;
    while (net_index < M && getline(file, dummy)) {
        bool all_space = true;
        for (char c : dummy) {
            if (c != ' ' && c != '\t' && c != '\r' && c != '\n') { all_space = false; break; }
        }
        if (all_space) continue;

        istringstream iss(dummy);
        int node_id;
        vector<int> pin_ids;
        while (iss >> node_id) pin_ids.push_back(node_id);

        if (pin_ids.size() < 2) continue;

        Net *net = graph.add_net(net_index);
        for (int pid : pin_ids) {
            Node *nd = graph.get_or_create_node(pid);
            nd->add_net(net);
            net->add_node(nd);
        }
        net_index++;
    }

    // ----- 确定节点最大索引，分配固定标记向量 -----
    int max_idx = 0;
    for (Node *node : graph.get_nodes()) {
        int idx = node->get_index();
        if (idx > max_idx) max_idx = idx;
    }
    int alloc_size = max_idx + 2;
    is_fixed.assign(alloc_size, false);
    fixed_fpga.assign(alloc_size, -1);

    // ----- 读取固定节点部分 -----
    // 文件格式: 前 M 行为网表边, 之后的每一行对应一个 FPGA (第 i 行的节点固定到 FPGA i)
    vector<vector<int>> fixed_nodes_per_fpga;
    while (getline(file, dummy)) {
        bool all_space = true;
        for (char c : dummy) {
            if (c != ' ' && c != '\t' && c != '\r' && c != '\n') { all_space = false; break; }
        }
        if (all_space) continue;
        istringstream iss(dummy);
        int node_id;
        vector<int> fixed_ids;
        while (iss >> node_id) fixed_ids.push_back(node_id);
        fixed_nodes_per_fpga.push_back(fixed_ids);
    }
    file.close();

    // 确保固定节点也在图中
    for (auto &fpga_fixed : fixed_nodes_per_fpga) {
        for (int id : fpga_fixed) {
            graph.get_or_create_node(id);
        }
    }

    // 重新计算 max_idx（固定节点可能引入更大的索引）
    max_idx = 0;
    for (Node *node : graph.get_nodes()) {
        int idx = node->get_index();
        if (idx > max_idx) max_idx = idx;
    }
    alloc_size = max_idx + 2;
    is_fixed.assign(alloc_size, false);
    fixed_fpga.assign(alloc_size, -1);

    // 设置固定节点
    for (size_t fpga = 0; fpga < fixed_nodes_per_fpga.size(); fpga++) {
        for (int id : fixed_nodes_per_fpga[fpga]) {
            if (id >= 0 && id < alloc_size) {
                is_fixed[id] = true;
                fixed_fpga[id] = (int)fpga;
            }
        }
    }
}

void Solution::read_benchmark(Graph &graph, string benchmark_name) {
    size_t last_dot = benchmark_name.find_last_of('.');
    string topo_name = benchmark_name.substr(0, last_dot) + "_topo" + benchmark_name.substr(last_dot);
    read_topology(topo_name);
    read_netlist_and_fixed(graph, benchmark_name);
}

// Algorithm 1 from TopoPart paper: Candidate FPGA Propagation
// 对照论文逐行实现：
//   L1-4:   预处理 maxDist[] 和 S(v_i, x) -> radius_has_
//   L5-9:   初始化 Cddt
//   L10-20: BFS 传播 + 剪枝 + 单例横向扩散
void Solution::propagate_candidates_with_graph(Graph &graph) {

    // 1. 收集所有固定节点及其 FPGA ID
    vector<int> fixed_nodes_list;
    for (int v = 0; v <= total_nodes_; v++) {
        if (is_fixed[v]) fixed_nodes_list.push_back(v);
    }
    int num_fixed = (int)fixed_nodes_list.size();

    // 2. 初始化 Cddt_mask（位掩码替代 unordered_set，避免百万次堆分配）
    Cddt_mask.assign(total_nodes_ + 2, vector<bool>(K_, false));
    for (int v = 0; v <= total_nodes_; v++) {
        if (is_fixed[v]) {
            Cddt_mask[v][fixed_fpga[v]] = true;
        } else {
            for (int f = 0; f < K_; f++) Cddt_mask[v][f] = true;
        }
    }

    // 如果没有固定节点，所有节点的候选集为全部 FPGA，无需传播
    if (num_fixed == 0) {
        return;
    }

    // 3. 预处理 radius_has_: radius_has_[fpga][other_fpga][max_dist] = true
    //    对应论文 L1-4: maxDist 和 S(v_i, x)
    int global_max_dist = 0;
    for (int f = 0; f < K_; f++) global_max_dist = max(global_max_dist, max_dist_[f]);
    radius_has_.assign(K_, vector<vector<bool>>(K_, vector<bool>(global_max_dist + 2, false)));
    for (int f = 0; f < K_; f++) {
        for (int d = 0; d <= max_dist_[f]; d++) {
            for (int j = 0; j < K_; j++) {
                if (topo_dist[f][j] <= d) {
                    radius_has_[f][j][d] = true;
                }
            }
        }
    }

    // 4. dist_to_from_src[u][fpga_id] = 节点 u 从 FPGA 源 fpga_id 出发 BFS 的电路距离
    //    由于 FPGA ID 范围是 [0, K_-1]，用 K_ 维即可
    //    但需要映射：dist_from_fpga[fpga_id][u] = 从该 FPGA 源到达 u 的距离
    //    使用 vector<vector<int>> dist_from_fpga(K_, vector<int>(total_nodes_ + 2, -1))
    vector<vector<int>> dist_from_fpga(K_, vector<int>(total_nodes_ + 2, -1));

    // 5. 多源 BFS：从每个固定节点出发
    // 队列存储 (node, fpga_id) 对
    // node: 电路图中的节点
    // fpga_id: 该节点对应的 FPGA（原始固定节点的 FPGA 或推导出的 FPGA）
    struct QEntry { int node; int fpga_id; };
    queue<QEntry> bfs_q;

    for (int fi = 0; fi < num_fixed; fi++) {
        int fn = fixed_nodes_list[fi];
        int fpga = fixed_fpga[fn];
        dist_from_fpga[fpga][fn] = 0;
        bfs_q.push({fn, fpga});
    }

    // 邻居去重用 token（每个 FPGA 源独立计数）
    vector<vector<int>> neighbor_seen(total_nodes_ + 2, vector<int>(K_, 0));
    vector<int> neighbor_token_vec(K_, 0);

    // 论文 L10: while Q.size() > 0
    while (!bfs_q.empty()) {
        QEntry entry = bfs_q.front();
        bfs_q.pop();
        int u = entry.node;
        int fpga_u = entry.fpga_id;
        int d = dist_from_fpga[fpga_u][u];
        int max_d = max_dist_[fpga_u];

        // 论文 L12: d = maxDist(v̂_i)，L13: 遍历 S(v_i, d)
        if (d >= max_d) continue;

        Node *u_node = graph.get_node(u);
        if (!u_node) continue;

        // 收集邻居（去重），跳过超大纲束避免 BFS 爆炸
        neighbor_token_vec[fpga_u]++;
        vector<int> nbrs;
        const int NET_PIN_THRESHOLD = 500;
        for (Net *net : u_node->get_nets()) {
            if ((int)net->get_nodes().size() > NET_PIN_THRESHOLD) continue;
            for (Node *vnode : net->get_nodes()) {
                int vid = vnode->get_index();
                if (vid == u) continue;
                if (neighbor_seen[vid][fpga_u] != neighbor_token_vec[fpga_u]) {
                    neighbor_seen[vid][fpga_u] = neighbor_token_vec[fpga_u];
                    nbrs.push_back(vid);
                }
            }
        }

        // 论文 L13-15: foreach v_j in S(v_i, d): k = dist(v_j, v_i), Cddt(v_j) ∩= S(v̂_i, k)
        for (int nb : nbrs) {
            int new_d = d + 1;  // k = dist(nb, u) = dist(nb, fpga_src) + 1

            // 首次到达该 FPGA 源时记录距离并入队
            if (dist_from_fpga[fpga_u][nb] == -1) {
                dist_from_fpga[fpga_u][nb] = new_d;
                bfs_q.push({nb, fpga_u});
            }

            // 固定节点的 Cddt 不可修改
            if (is_fixed[nb]) continue;

            // 论文 L15: Cddt(nb) ∩= S(fpga_u, new_d)
            // 先检查是否会清空，避免丢失有效约束
            int remaining = 0;
            int last_fpga = -1;
            for (int f = 0; f < K_; f++) {
                if (!Cddt_mask[nb][f]) continue;
                if (new_d >= (int)radius_has_[fpga_u][f].size()
                    || !radius_has_[fpga_u][f][new_d]) {
                    // would remove f
                } else {
                    remaining++;
                    last_fpga = f;
                }
            }
            if (remaining == 0) continue;  // 跳过会清空 Cddt 的约束

            bool changed = false;
            for (int f = 0; f < K_; f++) {
                if (!Cddt_mask[nb][f]) continue;
                if (new_d >= (int)radius_has_[fpga_u][f].size()
                    || !radius_has_[fpga_u][f][new_d]) {
                    Cddt_mask[nb][f] = false;
                    changed = true;
                }
            }

            if (changed && remaining == 1) {
                dist_from_fpga[last_fpga][nb] = 0;
                bfs_q.push({nb, last_fpga});
            }
        }
    }
}

void Solution::bucket_insert(int node_id) {
    int P = part_[node_id];
    int idx = max_gain_[node_id] + gain_offset_;
    bucket_prev_[node_id] = -1;
    bucket_next_[node_id] = bucket_heads_[P][idx];
    if (bucket_heads_[P][idx] != -1) bucket_prev_[bucket_heads_[P][idx]] = node_id;
    bucket_heads_[P][idx] = node_id;
    if (max_gain_[node_id] > max_gain_in_bucket_[P]) max_gain_in_bucket_[P] = max_gain_[node_id];
    in_bucket_[node_id] = true;
}

void Solution::bucket_remove(int node_id) {
    if (!in_bucket_[node_id]) return;
    in_bucket_[node_id] = false;
    int P = part_[node_id];
    int idx = max_gain_[node_id] + gain_offset_;
    if (bucket_prev_[node_id] != -1) bucket_next_[bucket_prev_[node_id]] = bucket_next_[node_id];
    else {
        bucket_heads_[P][idx] = bucket_next_[node_id];
        if (bucket_heads_[P][idx] == -1 && max_gain_[node_id] == max_gain_in_bucket_[P]) {
            while (max_gain_in_bucket_[P] >= -max_degree_ && bucket_heads_[P][max_gain_in_bucket_[P] + gain_offset_] == -1) max_gain_in_bucket_[P]--;
        }
    }
    if (bucket_next_[node_id] != -1) bucket_prev_[bucket_next_[node_id]] = bucket_prev_[node_id];
    bucket_next_[node_id] = -1; bucket_prev_[node_id] = -1;
}

int Solution::compute_topo_delta(Graph &graph, int node, int target_block) {
    int old_block = part_[node];
    if (old_block == target_block) return 0;
    int delta = 0;
    Node *n = graph.get_node(node);
    if (!n) return 0;
    for (Net *net : n->get_nets()) {
        int nid = net->get_index();
        int old_v = 0, new_v = 0;
        for (int f = 0; f < K_; f++) {
            if (f == old_block) continue;
            if (!topo_adj[old_block][f]) old_v += net_count_[nid][f];
        }
        for (int f = 0; f < K_; f++) {
            if (f == target_block) continue;
            int cnt = net_count_[nid][f];
            if (f == old_block) cnt--;
            if (!topo_adj[target_block][f]) new_v += cnt;
        }
        delta += new_v - old_v;
    }
    return delta;
}

int Solution::bucket_get_best_node(int &from_side, int &to_side, Graph &graph) {
    int best_node = -1;
    int max_valid_gain = -1e9;
    int best_violation_delta = 1e9;
    int nodes_checked = 0;
    const int MAX_NODES_CHECK = 5000;
    for (int k = 0; k < K_; ++k) {
        if (current_size_[k] - 1 < min_size_[k]) continue;
        for (int g = max_gain_in_bucket_[k]; g >= -max_degree_; --g) {
            if (g < max_valid_gain - 1) break;
            if (nodes_checked >= MAX_NODES_CHECK) return best_node;
            int curr = bucket_heads_[k][g + gain_offset_];
            while (curr != -1) {
                if (++nodes_checked > MAX_NODES_CHECK) return best_node;
                if (is_fixed[curr]) { curr = bucket_next_[curr]; continue; }
                int P = part_[curr];
                if (topo_delta_version_[curr] != topo_epoch_) {
                    for (int X = 0; X < K_; ++X) {
                        if (X == P) { topo_delta_[curr][X] = 0; continue; }
                        if (!Cddt_mask[curr][X]) { topo_delta_[curr][X] = 1e9; continue; }
                        topo_delta_[curr][X] = compute_topo_delta(graph, curr, X);
                    }
                    topo_delta_version_[curr] = topo_epoch_;
                }
                for (int X = 0; X < K_; ++X) {
                    if (X == k) continue;
                    if (!Cddt_mask[curr][X]) continue;
                    if (current_size_[X] + 1 > max_size_[X]) continue;
                    int v_delta = topo_delta_[curr][X];
                    if (v_delta > 0) continue;
                    int score = gain_[curr][X];
                    if (v_delta < best_violation_delta ||
                        (v_delta == best_violation_delta && score > max_valid_gain)) {
                        best_violation_delta = v_delta;
                        max_valid_gain = score;
                        best_node = curr; from_side = k; to_side = X;
                    }
                }
                curr = bucket_next_[curr];
            }
        }
    }
    return best_node;
}

bool Solution::init_partition(Graph &graph, mt19937 &rng) {
    (void)rng;
    fill(current_size_.begin(), current_size_.end(), 0);
    // 将所有节点放在拥有最多固定节点的 FPGA 上（保证零违规）
    int best_fpga = 0, best_cnt = -1;
    for (int f = 0; f < K_; f++) {
        int c = 0;
        for (Node *node : graph.get_nodes()) {
            int v = node->get_index();
            if (is_fixed[v] && fixed_fpga[v] == f) c++;
        }
        if (c > best_cnt) { best_cnt = c; best_fpga = f; }
    }
    for (Node *node : graph.get_nodes()) {
        int v = node->get_index();
        if (is_fixed[v]) { part_[v] = fixed_fpga[v]; current_size_[fixed_fpga[v]]++; }
        else { part_[v] = best_fpga; current_size_[best_fpga]++; }
    }

    // 贪心重平衡：从过载 FPGA 移动节点到相邻且未满的 FPGA
    // 初始化 net_count_ 供拓扑检查使用
    for (int i = 0; i < (int)net_count_.size(); ++i) fill(net_count_[i].begin(), net_count_[i].end(), 0);
    for (Node *node : graph.get_nodes()) {
        int v = node->get_index();
        for (Net *net : node->get_nets()) net_count_[net->get_index()][part_[v]]++;
    }

    for (int iter = 0; iter < K_ * K_; iter++) {
        int over_fpga = -1, over_size = -1;
        for (int f = 0; f < K_; f++) {
            if (current_size_[f] > max_size_[f] && current_size_[f] > over_size) {
                over_size = current_size_[f]; over_fpga = f;
            }
        }
        if (over_fpga == -1) break;

        int move_node = -1, move_target = -1;
        for (Node *node : graph.get_nodes()) {
            int v = node->get_index();
            if (is_fixed[v] || part_[v] != over_fpga) continue;
            for (int t = 0; t < K_; t++) {
                if (t == over_fpga) continue;
                if (current_size_[t] >= max_size_[t]) continue;
                bool ok = true;
                for (Net *net : node->get_nets()) {
                    int nid = net->get_index();
                    for (int g = 0; g < K_; g++) {
                        if (g == t) continue;
                        int cnt = net_count_[nid][g];
                        if (g == over_fpga) cnt--;
                        if (cnt > 0 && !topo_adj[g][t]) { ok = false; break; }
                    }
                    if (!ok) break;
                }
                if (ok) { move_node = v; move_target = t; break; }
            }
            if (move_node != -1) break;
        }
        if (move_node == -1) break;
        int old_f = part_[move_node];
        part_[move_node] = move_target;
        current_size_[old_f]--; current_size_[move_target]++;
        Node *mn = graph.get_node(move_node);
        if (mn) for (Net *net : mn->get_nets()) {
            net_count_[net->get_index()][old_f]--;
            net_count_[net->get_index()][move_target]++;
        }
    }
    return true;
}

void Solution::compute_initial_gains(Graph &graph) {
    for (int i = 0; i < (int)net_count_.size(); ++i) fill(net_count_[i].begin(), net_count_[i].end(), 0);
    for (Node *node : graph.get_nodes()) {
        int nid = node->get_index(); int P = part_[nid];
        for (Net *net : node->get_nets()) net_count_[net->get_index()][P]++;
    }
    for (Node *node : graph.get_nodes()) {
        int nid = node->get_index(); int P = part_[nid];
        max_gain_[nid] = -1e9;
        for (int X = 0; X < K_; ++X) {
            if (X == P) { gain_[nid][X] = 0; topo_delta_[nid][X] = 0; continue; }
            if (!Cddt_mask[nid][X]) { gain_[nid][X] = -1e9; topo_delta_[nid][X] = 1e9; continue; }
            int g = 0;
            int td = 0;
            for (Net *net : node->get_nets()) {
                int net_id = net->get_index();
                if (net_count_[net_id][P] == 1) g++;
                if (net_count_[net_id][X] == 0) g--;
                int old_v = 0, new_v = 0;
                for (int f = 0; f < K_; f++) {
                    if (f != P && !topo_adj[P][f]) old_v += net_count_[net_id][f];
                }
                for (int f = 0; f < K_; f++) {
                    if (f == X) continue;
                    int cnt = net_count_[net_id][f];
                    if (f == P) cnt--;
                    if (!topo_adj[X][f]) new_v += cnt;
                }
                td += new_v - old_v;
            }
            gain_[nid][X] = g;
            topo_delta_[nid][X] = td;
            if (g > max_gain_[nid]) { max_gain_[nid] = g; best_dest_[nid] = X; }
        }
        topo_delta_version_[nid] = topo_epoch_;
    }
}

void Solution::build_buckets(Graph &graph) {
    for (int s = 0; s < K_; s++) { fill(bucket_heads_[s].begin(), bucket_heads_[s].end(), -1); max_gain_in_bucket_[s] = -max_degree_ - 1; }
    fill(bucket_next_.begin(), bucket_next_.end(), -1);
    fill(bucket_prev_.begin(), bucket_prev_.end(), -1);
    for (Node *node : graph.get_nodes()) {
        int i = node->get_index();
        if (!locked_[i] && max_gain_[i] > -1e8) bucket_insert(i);
    }
}

void Solution::update_gains_after_move(Graph &graph, int moved_node, int from_side, int to_side) {
    neighbor_epoch_++;
    const int NET_PIN_THRESHOLD = 500;

    // 收集所有受影响的节点并从桶中移除
    vector<int> affected;
    for (Net *net : graph.get_node(moved_node)->get_nets()) {
        if ((int)net->get_nodes().size() > NET_PIN_THRESHOLD) continue;
        for (Node *cell : net->get_nodes()) {
            int cid = cell->get_index();
            if (locked_[cid]) continue;
            if (neighbor_seen_[cid] != neighbor_epoch_) {
                neighbor_seen_[cid] = neighbor_epoch_;
                if (cid != moved_node) affected.push_back(cid);
            }
        }
    }
    for (int cid : affected) bucket_remove(cid);

    // 逐网更新：先更新 net_count（所有网），再更新增益（仅小网）
    for (Net *net : graph.get_node(moved_node)->get_nets()) {
        int net_id = net->get_index();
        int net_size = (int)net->get_nodes().size();

        // 减去旧贡献（仅小网）
        if (net_size <= NET_PIN_THRESHOLD) {
            for (Node *cell : net->get_nodes()) {
                int cid = cell->get_index();
                if (locked_[cid]) continue;
                int P = part_[cid];
                for (int X = 0; X < K_; ++X) {
                    if (X == P) continue;
                    if (!Cddt_mask[cid][X]) { gain_[cid][X] = -1e9; continue; }
                    int old_contrib = (net_count_[net_id][P] == 1 ? 1 : 0) - (net_count_[net_id][X] == 0 ? 1 : 0);
                    gain_[cid][X] -= old_contrib;
                }
            }
        }

        // 始终更新 net_count
        net_count_[net_id][from_side]--; net_count_[net_id][to_side]++;

        // 加上新贡献（仅小网）
        if (net_size <= NET_PIN_THRESHOLD) {
            for (Node *cell : net->get_nodes()) {
                int cid = cell->get_index();
                if (locked_[cid]) continue;
                int P = part_[cid];
                for (int X = 0; X < K_; ++X) {
                    if (X == P) continue;
                    if (!Cddt_mask[cid][X]) { gain_[cid][X] = -1e9; continue; }
                    int new_contrib = (net_count_[net_id][P] == 1 ? 1 : 0) - (net_count_[net_id][X] == 0 ? 1 : 0);
                    gain_[cid][X] += new_contrib;
                }
            }
        }
    }

    // 重新计算 max_gain 并重新插入桶
    for (int cid : affected) {
        int P = part_[cid];
        max_gain_[cid] = -1e9; bool has_valid = false;
        for (int X = 0; X < K_; ++X) {
            if (X == P) continue;
            if (!Cddt_mask[cid][X]) { gain_[cid][X] = -1e9; continue; }
            has_valid = true;
            if (gain_[cid][X] > max_gain_[cid]) { max_gain_[cid] = gain_[cid][X]; best_dest_[cid] = X; }
        }
        if (has_valid) bucket_insert(cid);
        topo_delta_version_[cid] = -1;
    }
}

int Solution::compute_cut_size(Graph &graph) {
    long long cut = 0;
    for (Net *net : graph.get_nets()) {
        const vector<Node *>& nodes = net->get_nodes();
        if (nodes.size() < 2) continue;
        int anchor = part_[nodes[0]->get_index()];
        for (size_t j = 1; j < nodes.size(); ++j) {
            if (part_[nodes[j]->get_index()] != anchor) cut++;
        }
    }
    return (int)cut;
}

int Solution::compute_violations(Graph &graph, const vector<int> &part) {
    int violations = 0;
    size_t part_sz = part.size();
    for (Net *net : graph.get_nets()) {
        vector<Node *> nodes = net->get_nodes();
        if (nodes.size() < 2) continue;
        int anchor_idx = nodes[0]->get_index();
        if (anchor_idx < 0 || anchor_idx >= (int)part_sz) continue;
        int anchor_p = part[anchor_idx];
        if (anchor_p < 0 || anchor_p >= K_) continue;
        for (size_t j = 1; j < nodes.size(); ++j) {
            int idx = nodes[j]->get_index();
            if (idx < 0 || idx >= (int)part_sz) continue;
            int pj = part[idx];
            if (pj < 0 || pj >= K_ || pj == anchor_p) continue;
            if (!topo_adj[anchor_p][pj]) violations++;
        }
    }
    return violations;
}

int Solution::fm_pass(Graph &graph, int trial_id, int pass_id, ofstream &csv_file, int &out_best_topo) {
    fill(locked_.begin(), locked_.end(), false);
    compute_initial_gains(graph);
    build_buckets(graph);
    vector<int> moves_node, moves_from, moves_to;
    int cumulative_gain = 0, cumulative_topo = 0;
    int best_gain = 0, best_topo = 0, best_step = 0;
    int free_count = 0;
    for (Node *node : graph.get_nodes())
        if (!is_fixed[node->get_index()]) free_count++;
    int move_count = 0;
    int report_interval = max(1, free_count / 50);
    std::cout << "  Pass " << pass_id << ": free_nodes=" << free_count << " [" << std::flush;
    while (true) {
        int from_side = -1, to_side = -1;
        int chosen = bucket_get_best_node(from_side, to_side, graph);
        if (chosen == -1) break;
        move_count++;
        bucket_remove(chosen); part_[chosen] = to_side; locked_[chosen] = true;
        current_size_[from_side]--; current_size_[to_side]++;
        cumulative_gain += gain_[chosen][to_side];
        cumulative_topo += topo_delta_[chosen][to_side];
        moves_node.push_back(chosen); moves_from.push_back(from_side); moves_to.push_back(to_side);
        // 最优步选择：优先违规最少，其次增益最大
        if (cumulative_topo < best_topo ||
            (cumulative_topo == best_topo && cumulative_gain > best_gain)) {
            best_topo = cumulative_topo; best_gain = cumulative_gain; best_step = moves_node.size();
        }
        update_gains_after_move(graph, chosen, from_side, to_side);
        if (move_count % report_interval == 0) {
            int pct = (int)(move_count * 100LL / free_count);
            std::cout << "\r  Pass " << pass_id << ": " << pct << "% (" << move_count << "/" << free_count << ")"
                      << " gain=" << cumulative_gain << " topo=" << cumulative_topo << std::flush;
        }
    }
    std::cout << "\r  Pass " << pass_id << ": done, " << move_count << " moves, best_gain=" << best_gain
              << ", best_topo=" << best_topo << ", best_step=" << best_step << std::endl;
    for (int i = (int)moves_node.size() - 1; i >= best_step; i--) {
        part_[moves_node[i]] = moves_from[i]; current_size_[moves_to[i]]--; current_size_[moves_from[i]]++;
    }
    if (csv_file.is_open()) {
        for (size_t i = 0; i < moves_node.size(); i++)
            csv_file << trial_id << "," << pass_id << "," << i << "," << moves_node[i] << "\n";
    }
    out_best_topo = best_topo;
    return best_gain;
}

void Solution::my_partition_algorithm(Graph &graph, int K, vector<int> &part_result) {
    if (K != K_) {
        cerr << "Warning: K parameter (" << K << ") != topology FPGA count (" << K_ << "). Using topology count." << endl;
    }
    int max_node_idx = 0;
    for (Node *node : graph.get_nodes()) { int idx = node->get_index(); if (idx > max_node_idx) max_node_idx = idx; }
    total_nodes_ = max_node_idx;
    int alloc_size = total_nodes_ + 2;

    max_degree_ = 0;
    for (Node *node : graph.get_nodes()) max_degree_ = max(max_degree_, (int)node->get_nets().size());
    if (max_degree_ < 1) max_degree_ = 1;
    gain_offset_ = max_degree_;

    part_.assign(alloc_size, -1); gain_.assign(alloc_size, vector<int>(K_, 0));
    neighbor_seen_.assign(alloc_size, 0); neighbor_epoch_ = 0;
    max_gain_.assign(alloc_size, -1e9); best_dest_.assign(alloc_size, -1);
    locked_.assign(alloc_size, false); bucket_next_.assign(alloc_size, -1); bucket_prev_.assign(alloc_size, -1);
    if ((int)is_fixed.size() < alloc_size) { is_fixed.assign(alloc_size, false); fixed_fpga.assign(alloc_size, -1); }

    min_size_.assign(K_, 0); max_size_.assign(K_, 0);
    {
        int free_cnt = 0;
        for (Node *node : graph.get_nodes())
            if (!is_fixed[node->get_index()]) free_cnt++;
        int base = free_cnt / K_;
        for (int k = 0; k < K_; k++) {
            min_size_[k] = max(0, base - (base / 5));
            max_size_[k] = base + (base / 5);
        }
    }

    int num_nets = graph.get_net_num();
    net_count_.assign(num_nets, vector<int>(K_, 0)); current_size_.assign(K_, 0);
    topo_delta_.assign(alloc_size, vector<int>(K_, 0));
    topo_delta_version_.assign(alloc_size, -1); topo_epoch_ = 0;
    bucket_heads_.assign(K_, vector<int>(2 * max_degree_ + 1, -1)); max_gain_in_bucket_.assign(K_, -max_degree_ - 1);
    in_bucket_.assign(alloc_size, false);
    

    // 检测不可行固定节点：同一网内的固定节点位于不直连的 FPGA 上
    int num_unfixed = 0;
    for (Net *net : graph.get_nets()) {
        vector<int> fpgas_in_net;
        for (Node *node : net->get_nodes()) {
            int nid = node->get_index();
            if (nid < (int)is_fixed.size() && is_fixed[nid]) {
                fpgas_in_net.push_back(fixed_fpga[nid]);
            }
        }
        bool has_conflict = false;
        for (size_t i = 0; i < fpgas_in_net.size() && !has_conflict; i++) {
            for (size_t j = i + 1; j < fpgas_in_net.size(); j++) {
                if (fpgas_in_net[i] != fpgas_in_net[j] && !topo_adj[fpgas_in_net[i]][fpgas_in_net[j]]) {
                    has_conflict = true; break;
                }
            }
        }
        if (has_conflict) {
            for (Node *node : net->get_nodes()) {
                int nid = node->get_index();
                if (nid < (int)is_fixed.size() && is_fixed[nid]) {
                    is_fixed[nid] = false;
                    fixed_fpga[nid] = -1;
                    num_unfixed++;
                }
            }
        }
    }
    if (num_unfixed > 0) {
        cout << "Warning: Unfixed " << num_unfixed << " fixed nodes in infeasible nets." << endl;
    }

    propagate_candidates_with_graph(graph);
    cout << "Propagation done." << endl;

    int best_cut = INT_MAX, best_violations = INT_MAX;
    vector<int> best_part(alloc_size, 0);
    int num_restarts = 1;
    ofstream csv_file("fm_all_logs.csv"); csv_file << "Trial,Pass,Step,Cumulative_Gain\n";

    for (int trial = 0; trial < num_restarts; trial++) {
        mt19937 rng(trial * 12345 + 42);
        bool ok = init_partition(graph, rng);
        if (!ok) {
            cout << "Trial " << trial << ": init_partition failed, retrying..." << endl;
            continue;
        }
        for (int i = 0; i < num_nets; ++i) fill(net_count_[i].begin(), net_count_[i].end(), 0);
        for (Node *node : graph.get_nodes()) { int P = part_[node->get_index()]; for (Net *net : node->get_nets()) net_count_[net->get_index()][P]++; }
        {	vector<int> tp(alloc_size); for (int i = 0; i <= total_nodes_; i++) tp[i] = part_[i];
            cout << "Trial " << trial << ": init violations=" << compute_violations(graph, tp) << endl;
        }
        int pass = 0;
        while (true) {
            int prev_cut = compute_cut_size(graph);
            int best_pass_topo = 0;
            int best_pass_gain = fm_pass(graph, trial, pass, csv_file, best_pass_topo);
            for (int i = 0; i < num_nets; ++i) fill(net_count_[i].begin(), net_count_[i].end(), 0);
            for (Node *node : graph.get_nodes()) { int P = part_[node->get_index()]; for (Net *net : node->get_nets()) net_count_[net->get_index()][P]++; }
            int curr_cut = compute_cut_size(graph);
            cout << "Trial " << trial << " Pass " << pass << ": prev_cut=" << prev_cut << ", curr_cut=" << curr_cut
                 << ", improvement=" << (prev_cut - curr_cut) << ", best_pass_gain=" << best_pass_gain
                 << ", best_pass_topo=" << best_pass_topo << endl;
            if (best_pass_topo >= 0 && best_pass_gain <= 0) break;
            pass++;
        }

        // 违规修复阶段：回溯式修复 – 对每个违规网尝试所有兼容 FPGA，选最优 target
        for (int repair_iter = 0; repair_iter < 20; repair_iter++) {
            int repaired = 0;
            for (Net *net : graph.get_nets()) {
                vector<Node *> nodes = net->get_nodes();
                if (nodes.size() < 2) continue;
                bool fp[K_]; for (int f = 0; f < K_; f++) fp[f] = false;
                for (Node *n : nodes) { int p = part_[n->get_index()]; if (p >= 0 && p < K_) fp[p] = true; }
                bool violated = false;
                for (int f1 = 0; f1 < K_ && !violated; f1++) if (fp[f1])
                    for (int f2 = f1+1; f2 < K_; f2++) if (fp[f2] && !topo_adj[f1][f2]) { violated = true; break; }
                if (!violated) continue;

                int best_target = -1, best_viol_after = INT_MAX;
                vector<int> best_state_part;
                vector<int> best_state_size;
                vector<vector<int>> best_state_netcount;

                for (int target = 0; target < K_; target++) {
                    bool compatible = true;
                    for (Node *n : nodes) {
                        int nid = n->get_index();
                        if (nid < (int)is_fixed.size() && is_fixed[nid]) {
                            int ff = fixed_fpga[nid];
                            if (ff != target && !topo_adj[ff][target]) { compatible = false; break; }
                        }
                    }
                    if (!compatible) continue;

                    vector<int> saved_part = part_;
                    vector<int> saved_size = current_size_;
                    vector<vector<int>> saved_netcount = net_count_;
                    bool any_moved = false;

                    for (Node *n : nodes) {
                        int nid = n->get_index();
                        if (nid < (int)is_fixed.size() && is_fixed[nid]) continue;
                        int old = part_[nid];
                        if (old == target) continue;
                        bool safe = true;
                        for (Net *nt : n->get_nets()) {
                            if (nt == net) continue;
                            for (Node *n2 : nt->get_nodes()) {
                                if (n2->get_index() == nid) continue;
                                int g = saved_part[n2->get_index()];
                                if (g != target && !topo_adj[g][target]) { safe = false; break; }
                            }
                            if (!safe) break;
                        }
                        if (!safe) continue;
                        part_[nid] = target;
                        current_size_[old]--; current_size_[target]++;
                        for (Net *nt : n->get_nets()) {
                            net_count_[nt->get_index()][old]--;
                            net_count_[nt->get_index()][target]++;
                        }
                        any_moved = true;
                    }
                    if (!any_moved) {
                        part_ = saved_part; current_size_ = saved_size; net_count_ = saved_netcount;
                        continue;
                    }
                    int viol = compute_violations(graph, part_);
                    if (viol < best_viol_after) {
                        best_viol_after = viol; best_target = target;
                        best_state_part = part_;
                        best_state_size = current_size_;
                        best_state_netcount = net_count_;
                        if (viol == 0) break;
                    }
                    part_ = saved_part; current_size_ = saved_size; net_count_ = saved_netcount;
                }

                if (best_target != -1) {
                    part_ = best_state_part;
                    current_size_ = best_state_size;
                    net_count_ = best_state_netcount;
                    repaired++;
                }
            }
            if (repaired == 0) break;
        }

        int final_cut = compute_cut_size(graph);
        vector<int> temp_part(alloc_size);
        for (int i = 0; i <= total_nodes_; i++) temp_part[i] = part_[i];
        int violations = compute_violations(graph, temp_part);
        if (violations == 0 && final_cut < best_cut) { best_cut = final_cut; best_violations = violations; best_part = part_; }
        else if (violations < best_violations) { best_cut = final_cut; best_violations = violations; best_part = part_; }
    }
    csv_file.close(); part_result = best_part;
}

void Solution::convert_to_two_pin_graph(Graph &graph) {
    vector<int> all_nodes;
    for (Node *n : graph.get_nodes()) all_nodes.push_back(n->get_index());
    vector<vector<int>> hyperedges;
    for (Net *net : graph.get_nets()) {
        vector<int> pins;
        for (Node *n : net->get_nodes()) pins.push_back(n->get_index());
        if (pins.size() >= 2) hyperedges.push_back(pins);
    }

    for (auto node : graph.get_nodes()) delete node;
    for (auto net : graph.get_nets()) delete net;
    graph.get_nodes().clear();
    graph.get_nets().clear();
    graph.clear_maps();

    for (int id : all_nodes) graph.get_or_create_node(id);
    int net_id = 0;
    for (auto &pins : hyperedges) {
        int anchor = pins[0];
        for (size_t j = 1; j < pins.size(); ++j) {
            Net *net = graph.add_net(net_id++);
            Node *u = graph.get_or_create_node(anchor);
            Node *v = graph.get_or_create_node(pins[j]);
            net->add_node(u);
            net->add_node(v);
            u->add_net(net);
            v->add_net(net);
        }
    }
}