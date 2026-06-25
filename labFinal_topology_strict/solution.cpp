#include "solution.h"
#include <cmath>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <cstdlib>
#include <cstdint>

using std::cerr;
using std::cout;
using std::endl;
using std::ifstream;
using std::istringstream;
using std::mt19937;
using std::ofstream;
using std::size_t;
using std::string;
using std::vector;

namespace {

vector<int> parse_ints_from_line(const string &raw_line) {
    string line = raw_line;
    size_t comment_pos = line.find('#');
    if (comment_pos != string::npos) line = line.substr(0, comment_pos);

    istringstream iss(line);
    vector<int> values;
    int x = 0;
    while (iss >> x) values.push_back(x);
    return values;
}

vector<vector<int>> read_int_lines(const string &filename, bool keep_empty_lines) {
    ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("failed to open file: " + filename);
    }

    vector<vector<int>> lines;
    string line;
    while (std::getline(file, line)) {
        vector<int> values = parse_ints_from_line(line);
        if (keep_empty_lines || !values.empty()) lines.push_back(values);
    }
    while (!lines.empty() && lines.front().empty()) lines.erase(lines.begin());
    return lines;
}


int span_cost_from_parts(const vector<int> &parts, int K) {
    vector<char> seen(static_cast<size_t>(K), 0);
    int span = 0;
    for (int p : parts) {
        if (p < 0 || p >= K) continue;
        if (!seen[static_cast<size_t>(p)]) {
            seen[static_cast<size_t>(p)] = 1;
            ++span;
        }
    }
    return span > 1 ? span - 1 : 0;
}

} // namespace

Solution::Solution()
    : K_(0), r_(0.0), total_nodes_(0), max_degree_(1), gain_offset_(1),
      topology_loaded_(false), topology_enabled_(false), topology_k_(0),
      min_part_size_(0), max_part_size_(0), strict_lower_(0), strict_upper_(0) {}

bool Solution::read_topology(const string &topology_name) {
    vector<vector<int>> lines = read_int_lines(topology_name, false);
    if (lines.empty() || lines[0].size() < 2) {
        throw std::runtime_error("invalid topology file: first line must be 'K E'");
    }

    topology_k_ = lines[0][0];
    int edge_num = lines[0][1];
    if (topology_k_ <= 0 || edge_num < 0) {
        throw std::runtime_error("invalid topology size");
    }

    topo_allowed_.assign(static_cast<size_t>(topology_k_), vector<int>(static_cast<size_t>(topology_k_), 0));
    topo_dist_.assign(static_cast<size_t>(topology_k_), vector<int>(static_cast<size_t>(topology_k_), INF));
    topo_degree_.assign(static_cast<size_t>(topology_k_), 0);

    for (int i = 0; i < topology_k_; ++i) {
        topo_allowed_[static_cast<size_t>(i)][static_cast<size_t>(i)] = 1;
        topo_dist_[static_cast<size_t>(i)][static_cast<size_t>(i)] = 0;
    }

    int actual_edges = 0;
    for (size_t i = 1; i < lines.size() && actual_edges < edge_num; ++i, ++actual_edges) {
        if (lines[i].size() < 2) continue;
        int u = lines[i][0];
        int v = lines[i][1];
        if (u < 0 || u >= topology_k_ || v < 0 || v >= topology_k_) {
            cerr << "Warning: ignore topology edge with invalid endpoint: " << u << " " << v << endl;
            continue;
        }
        if (!topo_allowed_[static_cast<size_t>(u)][static_cast<size_t>(v)]) {
            ++topo_degree_[static_cast<size_t>(u)];
            ++topo_degree_[static_cast<size_t>(v)];
        }
        topo_allowed_[static_cast<size_t>(u)][static_cast<size_t>(v)] = 1;
        topo_allowed_[static_cast<size_t>(v)][static_cast<size_t>(u)] = 1;
        topo_dist_[static_cast<size_t>(u)][static_cast<size_t>(v)] = 1;
        topo_dist_[static_cast<size_t>(v)][static_cast<size_t>(u)] = 1;
    }

    topo_neighbor_mask_.assign(static_cast<size_t>(topology_k_), 0);
    if (topology_k_ <= 63) {
        for (int i = 0; i < topology_k_; ++i) {
            for (int j = 0; j < topology_k_; ++j) {
                if (topo_allowed_[static_cast<size_t>(i)][static_cast<size_t>(j)]) {
                    topo_neighbor_mask_[static_cast<size_t>(i)] |= (1ULL << static_cast<unsigned>(j));
                }
            }
        }
    }

    for (int k = 0; k < topology_k_; ++k) {
        for (int i = 0; i < topology_k_; ++i) {
            for (int j = 0; j < topology_k_; ++j) {
                int ik = topo_dist_[static_cast<size_t>(i)][static_cast<size_t>(k)];
                int kj = topo_dist_[static_cast<size_t>(k)][static_cast<size_t>(j)];
                if (ik < INF && kj < INF && ik + kj < topo_dist_[static_cast<size_t>(i)][static_cast<size_t>(j)]) {
                    topo_dist_[static_cast<size_t>(i)][static_cast<size_t>(j)] = ik + kj;
                }
            }
        }
    }

    topology_loaded_ = true;
    topology_enabled_ = true;
    cout << "Loaded topology: K=" << topology_k_ << ", edges=" << edge_num << endl;
    return true;
}

void Solution::read_benchmark(Graph &graph, const string &benchmark_name) {
    vector<vector<int>> lines = read_int_lines(benchmark_name, true);
    if (lines.empty()) {
        throw std::runtime_error("empty benchmark file: " + benchmark_name);
    }

    bool topopart_format = false;
    int node_num = 0;
    int edge_num = 0;
    int first_edge_line = 0;
    int id_base = 1;

    if (lines[0].size() == 1 && lines.size() >= 2 && lines[1].size() == 1) {
        topopart_format = true;
        node_num = lines[0][0];
        edge_num = lines[1][0];
        first_edge_line = 2;
        id_base = 0;
    } else if (lines[0].size() >= 2) {
        topopart_format = false;
        edge_num = lines[0][0];
        node_num = lines[0][1];
        first_edge_line = 1;

        // IBM .hgr 通常是 1-based；若真实出现 0，则自动改为 0-based。
        bool has_zero = false;
        for (int i = 0; i < edge_num && first_edge_line + i < static_cast<int>(lines.size()); ++i) {
            for (int x : lines[static_cast<size_t>(first_edge_line + i)]) {
                if (x == 0) has_zero = true;
            }
        }
        id_base = has_zero ? 0 : 1;
    } else {
        throw std::runtime_error("unrecognized benchmark format");
    }

    if (node_num <= 0 || edge_num < 0) {
        throw std::runtime_error("invalid benchmark size");
    }
    if (first_edge_line + edge_num > static_cast<int>(lines.size())) {
        throw std::runtime_error("benchmark ended before all edge/net lines were read");
    }

    graph.reset(node_num, id_base);
    fixed_part_.assign(static_cast<size_t>(node_num), -1);

    for (int i = 0; i < edge_num; ++i) {
        const vector<int> &pins = lines[static_cast<size_t>(first_edge_line + i)];
        if (pins.empty()) continue;
        Net *net = graph.add_net(graph.get_net_num());
        for (int original_id : pins) {
            if (!graph.contains_original_id(original_id)) {
                cerr << "Warning: ignore out-of-range node id " << original_id
                     << " in net " << i << endl;
                continue;
            }
            Node *node = graph.get_or_create_node(original_id);
            node->add_net(net);
            net->add_node(node);
        }
    }

    if (topopart_format && topology_loaded_) {
        int fixed_start = first_edge_line + edge_num;
        int loaded_fixed = 0;
        for (int fpga = 0; fixed_start + fpga < static_cast<int>(lines.size()); ++fpga) {
            if (fpga >= topology_k_) {
                // 兼容“用 MFS2 数据迁移到 MFS1 时忽略多出来的固定行”。
                continue;
            }
            for (int original_id : lines[static_cast<size_t>(fixed_start + fpga)]) {
                if (!graph.contains_original_id(original_id)) {
                    cerr << "Warning: ignore fixed node id out of range: " << original_id << endl;
                    continue;
                }
                int internal_id = graph.to_internal_id(original_id);
                int &slot = fixed_part_[static_cast<size_t>(internal_id)];
                if (slot != -1 && slot != fpga) {
                    cerr << "Warning: conflicting fixed constraint on node " << original_id
                         << ", keep FPGA " << slot << " and ignore FPGA " << fpga << endl;
                    continue;
                }
                if (slot == -1) ++loaded_fixed;
                slot = fpga;
            }
        }
        cout << "Loaded fixed constraints: " << loaded_fixed << " fixed nodes" << endl;
    }

    cout << "Loaded benchmark: nodes=" << graph.get_node_num()
         << ", nets=" << graph.get_net_num()
         << ", id_base=" << graph.get_id_base() << endl;
}

void Solution::compute_balance_bounds() {
    if (K_ <= 0 || total_nodes_ <= 0) {
        throw std::runtime_error("invalid K or node count");
    }
    if (r_ < 0.0) r_ = 0.0;
    double max_r = 1.0 / static_cast<double>(K_);
    if (r_ > max_r) {
        cerr << "Warning: r is larger than 1/K; clamp r from " << r_ << " to " << max_r << endl;
        r_ = max_r;
    }

    strict_lower_ = total_nodes_ / K_;
    strict_upper_ = (total_nodes_ + K_ - 1) / K_;

    if (std::fabs(r_ * static_cast<double>(K_) - 1.0) < 1e-10) {
        min_part_size_ = strict_lower_;
        max_part_size_ = strict_upper_;
    } else {
        min_part_size_ = static_cast<int>(std::floor(static_cast<double>(total_nodes_) * r_ + 1e-12));
        if (min_part_size_ < 0) min_part_size_ = 0;
        max_part_size_ = total_nodes_ - (K_ - 1) * min_part_size_;
        if (max_part_size_ < strict_upper_) max_part_size_ = strict_upper_;
    }
}

void Solution::initialize_storage(Graph &graph, int K, double r) {
    K_ = K;
    r_ = r;
    total_nodes_ = graph.get_node_num();
    if (K_ <= 0) throw std::runtime_error("K must be positive");
    if (topology_enabled_ && K_ != topology_k_) {
        throw std::runtime_error("K must equal topology node count when topology mode is enabled");
    }
    if (static_cast<int>(fixed_part_.size()) != total_nodes_) {
        fixed_part_.assign(static_cast<size_t>(total_nodes_), -1);
    }

    max_degree_ = 1;
    for (const Node *node : graph.get_nodes()) {
        max_degree_ = std::max(max_degree_, static_cast<int>(node->get_nets().size()));
    }
    gain_offset_ = max_degree_;
    compute_balance_bounds();

    part_.assign(static_cast<size_t>(total_nodes_), -1);
    gain_.assign(static_cast<size_t>(total_nodes_), vector<int>(static_cast<size_t>(K_), 0));
    max_gain_.assign(static_cast<size_t>(total_nodes_), NEG_INF);
    best_dest_.assign(static_cast<size_t>(total_nodes_), -1);
    locked_.assign(static_cast<size_t>(total_nodes_), 0);

    bucket_next_.assign(static_cast<size_t>(total_nodes_), -1);
    bucket_prev_.assign(static_cast<size_t>(total_nodes_), -1);

    net_count_.assign(static_cast<size_t>(graph.get_net_num()), vector<int>(static_cast<size_t>(K_), 0));
    current_size_.assign(static_cast<size_t>(K_), 0);
    bucket_heads_.assign(static_cast<size_t>(K_), vector<int>(static_cast<size_t>(2 * max_degree_ + 1), -1));
    max_gain_in_bucket_.assign(static_cast<size_t>(K_), -max_degree_ - 1);
}

bool Solution::is_fixed(int node_id) const {
    return node_id >= 0 && node_id < static_cast<int>(fixed_part_.size()) &&
           fixed_part_[static_cast<size_t>(node_id)] >= 0;
}

void Solution::assign_fixed_nodes() {
    std::fill(part_.begin(), part_.end(), -1);
    std::fill(current_size_.begin(), current_size_.end(), 0);
    for (int u = 0; u < total_nodes_; ++u) {
        int p = fixed_part_[static_cast<size_t>(u)];
        if (p < 0) continue;
        if (p >= K_) {
            cerr << "Warning: fixed part " << p << " is outside current K; ignore node " << u << endl;
            fixed_part_[static_cast<size_t>(u)] = -1;
            continue;
        }
        part_[static_cast<size_t>(u)] = p;
        ++current_size_[static_cast<size_t>(p)];
    }
}

void Solution::init_partition(Graph &graph, mt19937 &rng) {
    (void)graph;
    assign_fixed_nodes();

    vector<int> free_nodes;
    free_nodes.reserve(static_cast<size_t>(total_nodes_));
    for (int i = 0; i < total_nodes_; ++i) {
        if (part_[static_cast<size_t>(i)] < 0) free_nodes.push_back(i);
    }
    std::shuffle(free_nodes.begin(), free_nodes.end(), rng);

    vector<int> target(static_cast<size_t>(K_), strict_lower_);
    for (int i = 0; i < total_nodes_ % K_; ++i) ++target[static_cast<size_t>(i)];

    for (int node_id : free_nodes) {
        int best = -1;
        for (int p = 0; p < K_; ++p) {
            if (current_size_[static_cast<size_t>(p)] < target[static_cast<size_t>(p)] &&
                (best < 0 || current_size_[static_cast<size_t>(p)] < current_size_[static_cast<size_t>(best)])) {
                best = p;
            }
        }
        if (best < 0) {
            for (int p = 0; p < K_; ++p) {
                if (current_size_[static_cast<size_t>(p)] < max_part_size_ &&
                    (best < 0 || current_size_[static_cast<size_t>(p)] < current_size_[static_cast<size_t>(best)])) {
                    best = p;
                }
            }
        }
        if (best < 0) {
            best = static_cast<int>(std::min_element(current_size_.begin(), current_size_.end()) - current_size_.begin());
        }
        part_[static_cast<size_t>(node_id)] = best;
        ++current_size_[static_cast<size_t>(best)];
    }
}


std::uint64_t Solution::neighbor_union_mask(std::uint64_t mask) const {
    if (K_ > 63) return ~0ULL;
    std::uint64_t out = 0;
    for (int p = 0; p < K_; ++p) {
        if (mask & (1ULL << static_cast<unsigned>(p))) {
            out |= topo_neighbor_mask_[static_cast<size_t>(p)];
        }
    }
    return out;
}

std::vector<int> Solution::mask_to_vector(std::uint64_t mask) const {
    std::vector<int> out;
    for (int p = 0; p < K_; ++p) {
        if (mask & (1ULL << static_cast<unsigned>(p))) out.push_back(p);
    }
    return out;
}

void Solution::build_topology_candidate_masks(Graph &graph) {
    if (!topology_enabled_ || K_ > 63) return;

    const std::uint64_t all_mask = (K_ == 64) ? ~0ULL : ((1ULL << static_cast<unsigned>(K_)) - 1ULL);
    topology_candidate_mask_.assign(static_cast<size_t>(total_nodes_), all_mask);

    for (int u = 0; u < total_nodes_; ++u) {
        if (is_fixed(u)) {
            int p = fixed_part_[static_cast<size_t>(u)];
            topology_candidate_mask_[static_cast<size_t>(u)] = (1ULL << static_cast<unsigned>(p));
        }
    }

    // Arc-consistency style candidate propagation.  For an edge (u,v), u can only use
    // FPGAs that are adjacent to at least one candidate FPGA of v, and vice versa.
    // This is the lightweight version of the candidate FPGA propagation used by TopoPart/EasyPart.
    int changed_nodes = 0;
    bool changed = true;
    int rounds = 0;
    const int max_rounds = 64;
    while (changed && rounds < max_rounds) {
        changed = false;
        ++rounds;
        for (const Net *net : graph.get_nets()) {
            const std::vector<Node *> &pins = net->get_nodes();
            if (pins.size() < 2) continue;
            for (size_t i = 0; i < pins.size(); ++i) {
                int u = pins[i]->get_index();
                for (size_t j = i + 1; j < pins.size(); ++j) {
                    int v = pins[j]->get_index();
                    std::uint64_t cu = topology_candidate_mask_[static_cast<size_t>(u)];
                    std::uint64_t cv = topology_candidate_mask_[static_cast<size_t>(v)];
                    std::uint64_t nu = cu & neighbor_union_mask(cv);
                    std::uint64_t nv = cv & neighbor_union_mask(cu);
                    // Do not collapse a candidate set to empty.  Empty means the fixed constraints
                    // already conflict locally; keeping the previous set lets the repair stage find
                    // a minimum-violation solution instead of producing an unusable assignment.
                    if (nu != 0 && nu != cu) {
                        topology_candidate_mask_[static_cast<size_t>(u)] = nu;
                        changed = true;
                        ++changed_nodes;
                    }
                    if (nv != 0 && nv != cv) {
                        topology_candidate_mask_[static_cast<size_t>(v)] = nv;
                        changed = true;
                        ++changed_nodes;
                    }
                }
            }
        }
    }

    long long singleton = 0;
    long long total_pop = 0;
    for (std::uint64_t mask : topology_candidate_mask_) {
        if ((mask & (mask - 1ULL)) == 0) ++singleton;
        total_pop += __builtin_popcountll(mask);
    }
    double avg_pop = total_nodes_ > 0 ? static_cast<double>(total_pop) / static_cast<double>(total_nodes_) : 0.0;
    cout << "Candidate propagation rounds=" << rounds
         << ", changed=" << changed_nodes
         << ", singleton=" << singleton
         << ", avg_candidates=" << avg_pop << endl;
}

vector<int> Solution::topology_candidates_from_assigned(const Graph &graph, int node_id) const {
    std::uint64_t mask = 0;
    if (!topology_candidate_mask_.empty() && K_ <= 63) {
        mask = topology_candidate_mask_[static_cast<size_t>(node_id)];
    } else if (K_ <= 63) {
        mask = (1ULL << static_cast<unsigned>(K_)) - 1ULL;
    }

    if (K_ <= 63) {
        for (const Net *net : graph.get_node(node_id)->get_nets()) {
            for (const Node *pin : net->get_nodes()) {
                int v = pin->get_index();
                if (v == node_id) continue;
                int q = part_[static_cast<size_t>(v)];
                if (q < 0 || q >= K_) continue;
                mask &= topo_neighbor_mask_[static_cast<size_t>(q)];
            }
        }
        vector<int> candidates = mask_to_vector(mask);
        if (!candidates.empty()) return candidates;

        // If the already assigned neighbors over-constrain this node, fall back to the
        // propagated list candidates; choose_best_initial_part will then minimize local violations.
        if (!topology_candidate_mask_.empty()) {
            candidates = mask_to_vector(topology_candidate_mask_[static_cast<size_t>(node_id)]);
            if (!candidates.empty()) return candidates;
        }
    }

    vector<char> ok(static_cast<size_t>(K_), 1);
    for (const Net *net : graph.get_node(node_id)->get_nets()) {
        for (const Node *pin : net->get_nodes()) {
            int v = pin->get_index();
            if (v == node_id) continue;
            int q = part_[static_cast<size_t>(v)];
            if (q < 0) continue;
            for (int p = 0; p < K_; ++p) {
                if (!topo_allowed_[static_cast<size_t>(p)][static_cast<size_t>(q)]) {
                    ok[static_cast<size_t>(p)] = 0;
                }
            }
        }
    }

    vector<int> candidates;
    for (int p = 0; p < K_; ++p) {
        if (ok[static_cast<size_t>(p)]) candidates.push_back(p);
    }
    return candidates;
}

int Solution::local_cut_delta_if_assign(const Graph &graph, int node_id, int to_side) const {
    int delta = 0;
    vector<int> parts_before;
    vector<int> parts_after;
    for (const Net *net : graph.get_node(node_id)->get_nets()) {
        parts_before.clear();
        parts_after.clear();
        for (const Node *pin : net->get_nodes()) {
            int v = pin->get_index();
            int p = part_[static_cast<size_t>(v)];
            if (v == node_id) {
                parts_after.push_back(to_side);
            } else if (p >= 0) {
                parts_before.push_back(p);
                parts_after.push_back(p);
            }
        }
        delta += span_cost_from_parts(parts_after, K_) - span_cost_from_parts(parts_before, K_);
    }
    return delta;
}

int Solution::count_incident_topology_violations_after_move(const Graph &graph, int node_id, int to_side) const {
    int cnt = 0;
    for (const Net *net : graph.get_node(node_id)->get_nets()) {
        if (!is_net_topology_valid_after_move(net, node_id, to_side)) ++cnt;
    }
    return cnt;
}

int Solution::count_incident_topology_violations(const Graph &graph, int node_id) const {
    int cnt = 0;
    for (const Net *net : graph.get_node(node_id)->get_nets()) {
        if (!is_net_topology_valid(net)) ++cnt;
    }
    return cnt;
}

int Solution::choose_best_initial_part(Graph &graph, int node_id, const vector<int> &candidates) const {
    vector<int> usable;
    for (int p : candidates) {
        if (p >= 0 && p < K_ && current_size_[static_cast<size_t>(p)] < max_part_size_) {
            usable.push_back(p);
        }
    }
    if (usable.empty()) usable = candidates;
    if (usable.empty()) {
        usable.resize(static_cast<size_t>(K_));
        std::iota(usable.begin(), usable.end(), 0);
    }

    long long best_score = LLONG_MAX;
    int best_part = usable.front();
    for (int p : usable) {
        int vio = count_incident_topology_violations_after_move(graph, node_id, p);
        int cut_delta = local_cut_delta_if_assign(graph, node_id, p);
        int overflow = std::max(0, current_size_[static_cast<size_t>(p)] + 1 - max_part_size_);
        int balance_gap = std::abs((current_size_[static_cast<size_t>(p)] + 1) - strict_upper_);
        long long score = static_cast<long long>(vio) * 1000000000LL +
                          static_cast<long long>(overflow) * 10000000LL +
                          static_cast<long long>(cut_delta) * 10000LL +
                          static_cast<long long>(balance_gap) * 10LL -
                          static_cast<long long>(topo_degree_[static_cast<size_t>(p)]);
        if (score < best_score ||
            (score == best_score && current_size_[static_cast<size_t>(p)] < current_size_[static_cast<size_t>(best_part)])) {
            best_score = score;
            best_part = p;
        }
    }
    return best_part;
}

void Solution::init_topology_partition(Graph &graph, mt19937 &rng) {
    assign_fixed_nodes();
    build_topology_candidate_masks(graph);

    struct OrderItem {
        int node;
        int cand_count;
        int fixed_neighbors;
        int degree;
        int random_key;
    };

    vector<OrderItem> order;
    order.reserve(static_cast<size_t>(total_nodes_));
    std::uniform_int_distribution<int> random_key_dist(0, INT_MAX);

    for (int u = 0; u < total_nodes_; ++u) {
        if (part_[static_cast<size_t>(u)] >= 0) continue;
        int fixed_neighbors = 0;
        for (const Net *net : graph.get_node(u)->get_nets()) {
            for (const Node *pin : net->get_nodes()) {
                int v = pin->get_index();
                if (v != u && is_fixed(v)) ++fixed_neighbors;
            }
        }
        int cand_count = K_;
        if (!topology_candidate_mask_.empty() && K_ <= 63) {
            cand_count = __builtin_popcountll(topology_candidate_mask_[static_cast<size_t>(u)]);
        }
        order.push_back({u, cand_count, fixed_neighbors, static_cast<int>(graph.get_node(u)->get_nets().size()), random_key_dist(rng)});
    }

    std::sort(order.begin(), order.end(), [](const OrderItem &a, const OrderItem &b) {
        if (a.cand_count != b.cand_count) return a.cand_count < b.cand_count;
        if (a.fixed_neighbors != b.fixed_neighbors) return a.fixed_neighbors > b.fixed_neighbors;
        if (a.degree != b.degree) return a.degree > b.degree;
        return a.random_key < b.random_key;
    });

    for (const OrderItem &item : order) {
        int u = item.node;
        vector<int> candidates = topology_candidates_from_assigned(graph, u);
        int chosen = choose_best_initial_part(graph, u, candidates);
        part_[static_cast<size_t>(u)] = chosen;
        ++current_size_[static_cast<size_t>(chosen)];
    }

    int repaired = repair_topology_violations(graph, 6);
    if (repaired > 0) {
        cout << "Topology repair moved " << repaired << " nodes during initialization" << endl;
    }
    rebalance_initial_solution(graph);
}

bool Solution::is_net_topology_valid_after_move(const Net *net, int moved_node, int to_side) const {
    vector<int> used;
    vector<char> seen(static_cast<size_t>(K_), 0);
    for (const Node *pin : net->get_nodes()) {
        int u = pin->get_index();
        int p = (u == moved_node) ? to_side : part_[static_cast<size_t>(u)];
        if (p < 0 || p >= K_) continue;
        if (!seen[static_cast<size_t>(p)]) {
            seen[static_cast<size_t>(p)] = 1;
            used.push_back(p);
        }
    }
    for (size_t i = 0; i < used.size(); ++i) {
        for (size_t j = i + 1; j < used.size(); ++j) {
            if (!topo_allowed_[static_cast<size_t>(used[i])][static_cast<size_t>(used[j])]) return false;
        }
    }
    return true;
}

bool Solution::is_net_topology_valid(const Net *net) const {
    return is_net_topology_valid_after_move(net, -1, -1);
}

bool Solution::is_topology_move_legal(const Graph &graph, int node_id, int to_side) const {
    if (!topology_enabled_) return true;
    for (const Net *net : graph.get_node(node_id)->get_nets()) {
        if (!is_net_topology_valid_after_move(net, node_id, to_side)) return false;
    }
    return true;
}

bool Solution::is_balance_move_legal(int from_side, int to_side) const {
    if (from_side == to_side) return false;
    if (from_side < 0 || from_side >= K_ || to_side < 0 || to_side >= K_) return false;
    return current_size_[static_cast<size_t>(from_side)] > min_part_size_ &&
           current_size_[static_cast<size_t>(to_side)] < max_part_size_;
}

void Solution::rebalance_initial_solution(Graph &graph) {
    const int max_iter = std::max(1000, total_nodes_ * 2);
    int iter = 0;
    while (iter++ < max_iter) {
        int under = -1;
        for (int p = 0; p < K_; ++p) {
            if (current_size_[static_cast<size_t>(p)] < min_part_size_) {
                under = p;
                break;
            }
        }
        if (under < 0) break;

        int best_node = -1;
        int best_delta = INT_MAX;
        for (int u = 0; u < total_nodes_; ++u) {
            int from = part_[static_cast<size_t>(u)];
            if (from < 0 || from == under || is_fixed(u)) continue;
            if (current_size_[static_cast<size_t>(from)] <= min_part_size_) continue;
            if (!is_topology_move_legal(graph, u, under)) continue;
            int delta = local_cut_delta_if_move(graph, u, under);
            if (delta < best_delta) {
                best_delta = delta;
                best_node = u;
            }
        }
        if (best_node < 0) break;
        int from = part_[static_cast<size_t>(best_node)];
        part_[static_cast<size_t>(best_node)] = under;
        --current_size_[static_cast<size_t>(from)];
        ++current_size_[static_cast<size_t>(under)];
    }
}


std::vector<int> Solution::topology_legal_destinations_for_node(const Graph &graph, int node_id) const {
    std::vector<char> ok(static_cast<size_t>(K_), 1);
    int from = part_[static_cast<size_t>(node_id)];

    // 对于 node_id 的每条关联 net，目标 FPGA 必须与该 net 中其它已经分配的 FPGA 直接相连或相同。
    // 这相当于把可选 FPGA 限制为所有邻居 FPGA 的拓扑邻接集合交集。
    for (const Net *net : graph.get_node(node_id)->get_nets()) {
        for (const Node *pin : net->get_nodes()) {
            int v = pin->get_index();
            if (v == node_id) continue;
            int q = part_[static_cast<size_t>(v)];
            if (q < 0 || q >= K_) continue;
            for (int p = 0; p < K_; ++p) {
                if (!topo_allowed_[static_cast<size_t>(p)][static_cast<size_t>(q)]) {
                    ok[static_cast<size_t>(p)] = 0;
                }
            }
        }
    }

    std::vector<int> dests;
    for (int p = 0; p < K_; ++p) {
        if (p == from) continue;
        if (!ok[static_cast<size_t>(p)]) continue;
        if (!is_balance_move_legal(from, p)) continue;
        dests.push_back(p);
    }
    return dests;
}

int Solution::fast_topology_refine(Graph &graph, int max_sweeps) {
    if (!topology_enabled_ || max_sweeps <= 0) return 0;
    int total_moved = 0;
    rebuild_net_counts(graph);

    for (int sweep = 0; sweep < max_sweeps; ++sweep) {
        int moved_this_sweep = 0;
        long long gain_this_sweep = 0;

        for (int u = 0; u < total_nodes_; ++u) {
            if (is_fixed(u)) continue;
            int from = part_[static_cast<size_t>(u)];
            if (from < 0 || from >= K_) continue;

            std::vector<int> dests = topology_legal_destinations_for_node(graph, u);
            int best_to = -1;
            int best_gain = 0;
            for (int to : dests) {
                int delta = local_cut_delta_if_move(graph, u, to);
                int gain = -delta;
                if (gain > best_gain ||
                    (gain == best_gain && gain > 0 && current_size_[static_cast<size_t>(to)] < current_size_[static_cast<size_t>(best_to < 0 ? to : best_to)])) {
                    best_gain = gain;
                    best_to = to;
                }
            }

            if (best_to < 0 || best_gain <= 0) continue;

            part_[static_cast<size_t>(u)] = best_to;
            --current_size_[static_cast<size_t>(from)];
            ++current_size_[static_cast<size_t>(best_to)];
            for (const Net *net : graph.get_node(u)->get_nets()) {
                int net_id = net->get_index();
                --net_count_[static_cast<size_t>(net_id)][static_cast<size_t>(from)];
                ++net_count_[static_cast<size_t>(net_id)][static_cast<size_t>(best_to)];
            }

            ++moved_this_sweep;
            ++total_moved;
            gain_this_sweep += best_gain;
        }

        cout << "Fast topology sweep " << sweep
             << ": moved=" << moved_this_sweep
             << ", estimated_cut_gain=" << gain_this_sweep << endl;
        if (moved_this_sweep == 0) break;
    }
    return total_moved;
}

int Solution::repair_topology_violations(Graph &graph, int max_rounds) {
    if (!topology_enabled_) return 0;
    int total_moves = 0;
    for (int round = 0; round < max_rounds; ++round) {
        bool improved = false;
        for (const Net *net : graph.get_nets()) {
            if (is_net_topology_valid(net)) continue;

            int best_node = -1;
            int best_to = -1;
            int best_gain = 0;
            for (const Node *pin : net->get_nodes()) {
                int u = pin->get_index();
                if (is_fixed(u)) continue;
                int from = part_[static_cast<size_t>(u)];
                int before = count_incident_topology_violations(graph, u);
                for (int to = 0; to < K_; ++to) {
                    if (to == from) continue;
                    if (current_size_[static_cast<size_t>(to)] >= max_part_size_ &&
                        current_size_[static_cast<size_t>(from)] <= min_part_size_) {
                        continue;
                    }
                    int after = count_incident_topology_violations_after_move(graph, u, to);
                    int vio_gain = before - after;
                    int cut_gain = -local_cut_delta_if_move(graph, u, to);
                    int score = vio_gain * 100000 + cut_gain;
                    if (score > best_gain) {
                        best_gain = score;
                        best_node = u;
                        best_to = to;
                    }
                }
            }
            if (best_node >= 0 && best_to >= 0) {
                int from = part_[static_cast<size_t>(best_node)];
                part_[static_cast<size_t>(best_node)] = best_to;
                --current_size_[static_cast<size_t>(from)];
                ++current_size_[static_cast<size_t>(best_to)];
                if (!net_count_.empty()) {
                    for (const Net *affected : graph.get_node(best_node)->get_nets()) {
                        int net_id = affected->get_index();
                        if (from >= 0 && from < K_) --net_count_[static_cast<size_t>(net_id)][static_cast<size_t>(from)];
                        if (best_to >= 0 && best_to < K_) ++net_count_[static_cast<size_t>(net_id)][static_cast<size_t>(best_to)];
                    }
                }
                ++total_moves;
                improved = true;
            }
        }
        if (!improved || count_topology_violations(graph) == 0) break;
    }
    return total_moves;
}

void Solution::rebuild_net_counts(Graph &graph) {
    for (vector<int> &row : net_count_) std::fill(row.begin(), row.end(), 0);
    for (const Node *node : graph.get_nodes()) {
        int p = part_[static_cast<size_t>(node->get_index())];
        if (p < 0 || p >= K_) continue;
        for (const Net *net : node->get_nets()) {
            ++net_count_[static_cast<size_t>(net->get_index())][static_cast<size_t>(p)];
        }
    }
}

void Solution::compute_initial_gains(Graph &graph) {
    rebuild_net_counts(graph);

    for (const Node *node : graph.get_nodes()) {
        int nid = node->get_index();
        int P = part_[static_cast<size_t>(nid)];
        max_gain_[static_cast<size_t>(nid)] = -max_degree_;
        best_dest_[static_cast<size_t>(nid)] = -1;

        if (P < 0 || P >= K_ || is_fixed(nid)) continue;

        for (int X = 0; X < K_; ++X) {
            if (X == P) {
                gain_[static_cast<size_t>(nid)][static_cast<size_t>(X)] = 0;
                continue;
            }
            int g = 0;
            for (const Net *net : node->get_nets()) {
                int net_id = net->get_index();
                if (net_count_[static_cast<size_t>(net_id)][static_cast<size_t>(P)] == 1) ++g;
                if (net_count_[static_cast<size_t>(net_id)][static_cast<size_t>(X)] == 0) --g;
            }
            gain_[static_cast<size_t>(nid)][static_cast<size_t>(X)] = g;
            if (g > max_gain_[static_cast<size_t>(nid)]) {
                max_gain_[static_cast<size_t>(nid)] = g;
                best_dest_[static_cast<size_t>(nid)] = X;
            }
        }
    }
}

void Solution::bucket_insert(int node_id) {
    if (node_id < 0 || node_id >= total_nodes_ || locked_[static_cast<size_t>(node_id)] || is_fixed(node_id)) return;
    int P = part_[static_cast<size_t>(node_id)];
    if (P < 0 || P >= K_) return;
    int g = std::max(-max_degree_, std::min(max_degree_, max_gain_[static_cast<size_t>(node_id)]));
    max_gain_[static_cast<size_t>(node_id)] = g;
    int idx = g + gain_offset_;

    bucket_prev_[static_cast<size_t>(node_id)] = -1;
    bucket_next_[static_cast<size_t>(node_id)] = bucket_heads_[static_cast<size_t>(P)][static_cast<size_t>(idx)];
    if (bucket_heads_[static_cast<size_t>(P)][static_cast<size_t>(idx)] != -1) {
        int old_head = bucket_heads_[static_cast<size_t>(P)][static_cast<size_t>(idx)];
        bucket_prev_[static_cast<size_t>(old_head)] = node_id;
    }
    bucket_heads_[static_cast<size_t>(P)][static_cast<size_t>(idx)] = node_id;
    if (g > max_gain_in_bucket_[static_cast<size_t>(P)]) {
        max_gain_in_bucket_[static_cast<size_t>(P)] = g;
    }
}

void Solution::bucket_remove(int node_id) {
    if (node_id < 0 || node_id >= total_nodes_) return;
    int P = part_[static_cast<size_t>(node_id)];
    if (P < 0 || P >= K_) return;
    int g = std::max(-max_degree_, std::min(max_degree_, max_gain_[static_cast<size_t>(node_id)]));
    int idx = g + gain_offset_;

    int prev = bucket_prev_[static_cast<size_t>(node_id)];
    int next = bucket_next_[static_cast<size_t>(node_id)];
    if (prev != -1) {
        bucket_next_[static_cast<size_t>(prev)] = next;
    } else if (bucket_heads_[static_cast<size_t>(P)][static_cast<size_t>(idx)] == node_id) {
        bucket_heads_[static_cast<size_t>(P)][static_cast<size_t>(idx)] = next;
    }
    if (next != -1) bucket_prev_[static_cast<size_t>(next)] = prev;
    bucket_next_[static_cast<size_t>(node_id)] = -1;
    bucket_prev_[static_cast<size_t>(node_id)] = -1;

    if (max_gain_in_bucket_[static_cast<size_t>(P)] == g) {
        while (max_gain_in_bucket_[static_cast<size_t>(P)] >= -max_degree_) {
            int probe = max_gain_in_bucket_[static_cast<size_t>(P)] + gain_offset_;
            if (bucket_heads_[static_cast<size_t>(P)][static_cast<size_t>(probe)] != -1) break;
            --max_gain_in_bucket_[static_cast<size_t>(P)];
        }
    }
}

void Solution::build_buckets() {
    for (int s = 0; s < K_; ++s) {
        std::fill(bucket_heads_[static_cast<size_t>(s)].begin(), bucket_heads_[static_cast<size_t>(s)].end(), -1);
        max_gain_in_bucket_[static_cast<size_t>(s)] = -max_degree_ - 1;
    }
    std::fill(bucket_next_.begin(), bucket_next_.end(), -1);
    std::fill(bucket_prev_.begin(), bucket_prev_.end(), -1);

    for (int i = 0; i < total_nodes_; ++i) {
        if (!locked_[static_cast<size_t>(i)] && !is_fixed(i)) bucket_insert(i);
    }
}

int Solution::bucket_get_best_node(Graph &graph, int &from_side, int &to_side) {
    int best_node = -1;
    int best_gain = NEG_INF;
    from_side = -1;
    to_side = -1;

    for (int from = 0; from < K_; ++from) {
        for (int g = max_gain_in_bucket_[static_cast<size_t>(from)]; g >= -max_degree_; --g) {
            if (g < best_gain) break;
            int curr = bucket_heads_[static_cast<size_t>(from)][static_cast<size_t>(g + gain_offset_)];
            while (curr != -1) {
                int next = bucket_next_[static_cast<size_t>(curr)];
                if (!locked_[static_cast<size_t>(curr)] && !is_fixed(curr)) {
                    for (int to = 0; to < K_; ++to) {
                        if (to == from) continue;
                        if (!is_balance_move_legal(from, to)) continue;
                        if (!is_topology_move_legal(graph, curr, to)) continue;
                        int move_gain = gain_[static_cast<size_t>(curr)][static_cast<size_t>(to)];
                        if (move_gain > best_gain ||
                            (move_gain == best_gain && current_size_[static_cast<size_t>(to)] < current_size_[static_cast<size_t>(to_side < 0 ? to : to_side)])) {
                            best_gain = move_gain;
                            best_node = curr;
                            from_side = from;
                            to_side = to;
                        }
                    }
                }
                curr = next;
            }
        }
    }
    return best_node;
}

void Solution::update_gains_after_move(Graph &graph, int moved_node, int from_side, int to_side) {
    Node *base = graph.get_node(moved_node);

    for (Net *net : base->get_nets()) {
        int net_id = net->get_index();

        for (Node *cell : net->get_nodes()) {
            int cid = cell->get_index();
            if (locked_[static_cast<size_t>(cid)] || is_fixed(cid)) continue;
            int P = part_[static_cast<size_t>(cid)];
            bucket_remove(cid);
            for (int X = 0; X < K_; ++X) {
                if (X == P) continue;
                int old_contrib = (net_count_[static_cast<size_t>(net_id)][static_cast<size_t>(P)] == 1 ? 1 : 0) -
                                  (net_count_[static_cast<size_t>(net_id)][static_cast<size_t>(X)] == 0 ? 1 : 0);
                gain_[static_cast<size_t>(cid)][static_cast<size_t>(X)] -= old_contrib;
            }
        }

        --net_count_[static_cast<size_t>(net_id)][static_cast<size_t>(from_side)];
        ++net_count_[static_cast<size_t>(net_id)][static_cast<size_t>(to_side)];

        for (Node *cell : net->get_nodes()) {
            int cid = cell->get_index();
            if (locked_[static_cast<size_t>(cid)] || is_fixed(cid)) continue;
            int P = part_[static_cast<size_t>(cid)];
            max_gain_[static_cast<size_t>(cid)] = -max_degree_;
            best_dest_[static_cast<size_t>(cid)] = -1;
            for (int X = 0; X < K_; ++X) {
                if (X == P) continue;
                int new_contrib = (net_count_[static_cast<size_t>(net_id)][static_cast<size_t>(P)] == 1 ? 1 : 0) -
                                  (net_count_[static_cast<size_t>(net_id)][static_cast<size_t>(X)] == 0 ? 1 : 0);
                gain_[static_cast<size_t>(cid)][static_cast<size_t>(X)] += new_contrib;
                if (gain_[static_cast<size_t>(cid)][static_cast<size_t>(X)] > max_gain_[static_cast<size_t>(cid)]) {
                    max_gain_[static_cast<size_t>(cid)] = gain_[static_cast<size_t>(cid)][static_cast<size_t>(X)];
                    best_dest_[static_cast<size_t>(cid)] = X;
                }
            }
            bucket_insert(cid);
        }
    }
}

int Solution::compute_cut_size(Graph &graph) const {
    int cut = 0;
    for (int net_id = 0; net_id < graph.get_net_num(); ++net_id) {
        int span = 0;
        for (int k = 0; k < K_; ++k) {
            if (net_count_[static_cast<size_t>(net_id)][static_cast<size_t>(k)] > 0) ++span;
        }
        if (span > 1) cut += (span - 1);
    }
    return cut;
}

int Solution::local_cut_delta_if_move(const Graph &graph, int node_id, int to_side) const {
    int from_side = part_[static_cast<size_t>(node_id)];
    if (from_side == to_side) return 0;
    int delta = 0;
    for (const Net *net : graph.get_node(node_id)->get_nets()) {
        int net_id = net->get_index();
        int before_span = 0;
        for (int p = 0; p < K_; ++p) {
            if (net_count_[static_cast<size_t>(net_id)][static_cast<size_t>(p)] > 0) ++before_span;
        }
        int from_count = net_count_[static_cast<size_t>(net_id)][static_cast<size_t>(from_side)] - 1;
        int to_count = net_count_[static_cast<size_t>(net_id)][static_cast<size_t>(to_side)] + 1;
        int after_span = before_span;
        if (net_count_[static_cast<size_t>(net_id)][static_cast<size_t>(from_side)] > 0 && from_count == 0) --after_span;
        if (net_count_[static_cast<size_t>(net_id)][static_cast<size_t>(to_side)] == 0 && to_count > 0) ++after_span;
        delta += std::max(0, after_span - 1) - std::max(0, before_span - 1);
    }
    return delta;
}

int Solution::fm_pass(Graph &graph, int trial_id, int pass_id, ofstream &csv_file) {
    std::fill(locked_.begin(), locked_.end(), 0);
    compute_initial_gains(graph);
    build_buckets();

    vector<int> moves_node;
    vector<int> moves_from;
    vector<int> moves_to;
    vector<int> gains_at_step;
    int cumulative_gain = 0;
    int best_gain = 0;
    int best_step = 0;

    while (true) {
        int from_side = -1;
        int to_side = -1;
        int chosen = bucket_get_best_node(graph, from_side, to_side);
        if (chosen == -1) break;

        bucket_remove(chosen);
        part_[static_cast<size_t>(chosen)] = to_side;
        locked_[static_cast<size_t>(chosen)] = 1;
        --current_size_[static_cast<size_t>(from_side)];
        ++current_size_[static_cast<size_t>(to_side)];

        int step_gain = gain_[static_cast<size_t>(chosen)][static_cast<size_t>(to_side)];
        cumulative_gain += step_gain;
        moves_node.push_back(chosen);
        moves_from.push_back(from_side);
        moves_to.push_back(to_side);
        gains_at_step.push_back(cumulative_gain);

        if (cumulative_gain > best_gain) {
            best_gain = cumulative_gain;
            best_step = static_cast<int>(moves_node.size());
        }
        update_gains_after_move(graph, chosen, from_side, to_side);
    }

    for (int i = static_cast<int>(moves_node.size()) - 1; i >= best_step; --i) {
        int node_id = moves_node[static_cast<size_t>(i)];
        int from = moves_from[static_cast<size_t>(i)];
        int to = moves_to[static_cast<size_t>(i)];
        part_[static_cast<size_t>(node_id)] = from;
        --current_size_[static_cast<size_t>(to)];
        ++current_size_[static_cast<size_t>(from)];
    }

    if (csv_file.is_open()) {
        for (size_t i = 0; i < gains_at_step.size(); ++i) {
            csv_file << trial_id << ',' << pass_id << ',' << i << ',' << gains_at_step[i] << '\n';
        }
    }
    return best_gain;
}

int Solution::count_topology_violations(Graph &graph) const {
    if (!topology_enabled_) return 0;
    int violations = 0;
    for (const Net *net : graph.get_nets()) {
        if (!is_net_topology_valid(net)) ++violations;
    }
    return violations;
}

long long Solution::compute_topology_hop_cost(Graph &graph) const {
    if (!topology_enabled_) return 0;
    long long hop_cost = 0;
    vector<int> used;
    vector<char> seen(static_cast<size_t>(K_), 0);
    for (const Net *net : graph.get_nets()) {
        std::fill(seen.begin(), seen.end(), 0);
        used.clear();
        for (const Node *pin : net->get_nodes()) {
            int p = part_[static_cast<size_t>(pin->get_index())];
            if (p < 0 || p >= K_) continue;
            if (!seen[static_cast<size_t>(p)]) {
                seen[static_cast<size_t>(p)] = 1;
                used.push_back(p);
            }
        }
        for (size_t i = 0; i < used.size(); ++i) {
            for (size_t j = i + 1; j < used.size(); ++j) {
                int d = topo_dist_[static_cast<size_t>(used[i])][static_cast<size_t>(used[j])];
                if (d < INF) hop_cost += d;
            }
        }
    }
    return hop_cost;
}

void Solution::my_partition_algorithm(Graph &graph, int K, double r, vector<int> &part_result) {
    initialize_storage(graph, K, r);

    cout << "Balance bounds: min=" << min_part_size_ << ", max=" << max_part_size_ << endl;
    if (topology_enabled_) {
        int fixed_cnt = 0;
        for (int p : fixed_part_) if (p >= 0) ++fixed_cnt;
        cout << "Topology mode enabled, fixed nodes=" << fixed_cnt << endl;
    }

    int best_cut = INT_MAX;
    int best_violation = INT_MAX;
    long long best_hop = LLONG_MAX;
    vector<int> best_part(static_cast<size_t>(total_nodes_), 0);

    const bool large_topology_case = topology_enabled_ &&
        (total_nodes_ >= 100000 || graph.get_net_num() >= 300000);
    const bool force_full_fm = (std::getenv("VLSI_FULL_FM") != nullptr);
    const bool use_fast_topology_mode = large_topology_case && !force_full_fm;

    int num_restarts = topology_enabled_ ? 12 : 30;
    int max_passes = 30;
    int fast_topology_sweeps = 1;
    bool topo_sweeps_specified = false;
    int topology_repair_rounds = 12;

    if (const char *env = std::getenv("VLSI_RESTARTS")) {
        num_restarts = std::max(1, std::atoi(env));
    }
    if (const char *env = std::getenv("VLSI_MAX_PASSES")) {
        max_passes = std::max(0, std::atoi(env));
    }
    if (const char *env = std::getenv("VLSI_TOPO_SWEEPS")) {
        fast_topology_sweeps = std::max(0, std::atoi(env));
        topo_sweeps_specified = true;
    }
    if (const char *env = std::getenv("VLSI_REPAIR_ROUNDS")) {
        topology_repair_rounds = std::max(0, std::atoi(env));
    }

    if (use_fast_topology_mode) {
        // TopoPart 的大规模 case1 为 30 万节点/74.9 万边，完整 K-way FM 会非常慢。
        // 默认采用“拓扑驱动初始化 + 一轮拓扑合法贪心 refinement”，保证几分钟内出结果。
        num_restarts = std::min(num_restarts, 1);
        max_passes = std::min(max_passes, 0);
        if (!topo_sweeps_specified) fast_topology_sweeps = 1;
        cout << "Large topology benchmark detected: use fast topology mode "
             << "(restarts=" << num_restarts
             << ", FM passes=" << max_passes
             << ", greedy sweeps=" << fast_topology_sweeps << ")." << endl;
        cout << "Set VLSI_FULL_FM=1 to force the slower full FM refinement." << endl;
    }

    ofstream csv_file("fm_all_logs.csv");
    csv_file << "Trial,Pass,Step,Cumulative_Gain\n";
    ofstream trial_file("fm_trial_results.csv");
    trial_file << "Trial,Final_Cut,Topology_Violations,Hop_Cost\n";

    for (int trial = 0; trial < num_restarts; ++trial) {
        mt19937 rng(static_cast<unsigned int>(trial * 12345 + 42));
        if (topology_enabled_) {
            init_topology_partition(graph, rng);
        } else {
            init_partition(graph, rng);
        }

        rebuild_net_counts(graph);
        if (use_fast_topology_mode) {
            fast_topology_refine(graph, fast_topology_sweeps);
            int repaired_after_refine = repair_topology_violations(graph, topology_repair_rounds);
            if (repaired_after_refine > 0) {
                cout << "Topology repair moved " << repaired_after_refine << " nodes after fast refinement" << endl;
                rebuild_net_counts(graph);
            }
        } else {
            int pass = 0;
            while (pass < max_passes) {
                int prev_cut = compute_cut_size(graph);
                int best_pass_gain = fm_pass(graph, trial, pass, csv_file);
                rebuild_net_counts(graph);
                int curr_cut = compute_cut_size(graph);
                if (trial == 0) {
                    cout << "Trial " << trial << " Pass " << pass
                         << ": cut=" << curr_cut
                         << ", improvement=" << (prev_cut - curr_cut)
                         << ", best_pass_gain=" << best_pass_gain << endl;
                }
                if (best_pass_gain <= 0 || curr_cut >= prev_cut) break;
                ++pass;
            }
        }

        int final_cut = compute_cut_size(graph);
        int topo_vio = count_topology_violations(graph);
        long long hop_cost = compute_topology_hop_cost(graph);
        trial_file << trial << ',' << final_cut << ',' << topo_vio << ',' << hop_cost << '\n';

        bool better = false;
        if (topology_enabled_) {
            if (topo_vio < best_violation) better = true;
            else if (topo_vio == best_violation && final_cut < best_cut) better = true;
            else if (topo_vio == best_violation && final_cut == best_cut && hop_cost < best_hop) better = true;
        } else {
            better = final_cut < best_cut;
        }

        if (better) {
            best_cut = final_cut;
            best_violation = topo_vio;
            best_hop = hop_cost;
            best_part = part_;
            ofstream best_id("best_trial_id.txt");
            best_id << trial << endl;
        }
    }

    csv_file.close();
    trial_file.close();

    part_ = best_part;
    rebuild_net_counts(graph);
    part_result = best_part;

    cout << "Best internal cut: " << best_cut << endl;
    if (topology_enabled_) {
        cout << "Best topology violations: " << best_violation << endl;
        cout << "Best topology hop cost: " << best_hop << endl;
        if (best_violation > 0) {
            cout << "Warning: a strictly non-hop solution was not found; result is the minimum-violation solution among trials." << endl;
        }
    }
}
