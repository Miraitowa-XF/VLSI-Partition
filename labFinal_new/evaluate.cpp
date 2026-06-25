#include "evaluate.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

using std::ifstream;
using std::string;
using std::vector;

int evaluate_kway(Graph &graph, const vector<int> &part, int K) {
    int cut = 0;
    vector<int> mark(static_cast<size_t>(K), 0);
    int stamp = 1;

    for (const Net *net : graph.get_nets()) {
        int span_count = 0;
        for (const Node *node : net->get_nodes()) {
            int p = part.at(static_cast<size_t>(node->get_index()));
            if (p < 0 || p >= K) {
                throw std::runtime_error("partition id out of range in evaluate_kway");
            }
            if (mark[static_cast<size_t>(p)] != stamp) {
                mark[static_cast<size_t>(p)] = stamp;
                ++span_count;
            }
        }
        if (span_count > 1) cut += (span_count - 1);
        ++stamp;
        if (stamp == 0x3fffffff) {
            std::fill(mark.begin(), mark.end(), 0);
            stamp = 1;
        }
    }
    return cut;
}

int evaluate(Graph &graph, const string &partition_name, int K) {
    vector<int> part(static_cast<size_t>(graph.get_node_num()), 0);
    ifstream partition_file(partition_name);
    if (!partition_file.is_open()) {
        throw std::runtime_error("failed to open partition file: " + partition_name);
    }

    string line;
    int i = 0;
    while (i < graph.get_node_num() && std::getline(partition_file, line)) {
        std::istringstream iss(line);
        iss >> part[static_cast<size_t>(i)];
        ++i;
    }
    if (i != graph.get_node_num()) {
        throw std::runtime_error("partition file line count is smaller than node count");
    }
    return evaluate_kway(graph, part, K);
}

int evaluate_topology_violations(Graph &graph,
                                 const vector<int> &part,
                                 const vector<vector<int>> &topo_allowed) {
    int violations = 0;
    const int K = static_cast<int>(topo_allowed.size());
    vector<int> used;
    vector<int> mark(static_cast<size_t>(K), 0);
    int stamp = 1;

    for (const Net *net : graph.get_nets()) {
        used.clear();
        for (const Node *node : net->get_nodes()) {
            int p = part.at(static_cast<size_t>(node->get_index()));
            if (p < 0 || p >= K) continue;
            if (mark[static_cast<size_t>(p)] != stamp) {
                mark[static_cast<size_t>(p)] = stamp;
                used.push_back(p);
            }
        }
        bool ok = true;
        for (size_t i = 0; i < used.size() && ok; ++i) {
            for (size_t j = i + 1; j < used.size(); ++j) {
                if (topo_allowed[static_cast<size_t>(used[i])][static_cast<size_t>(used[j])] == 0) {
                    ok = false;
                    break;
                }
            }
        }
        if (!ok) ++violations;
        ++stamp;
        if (stamp == 0x3fffffff) {
            std::fill(mark.begin(), mark.end(), 0);
            stamp = 1;
        }
    }
    return violations;
}

long long evaluate_topology_hop_cost(Graph &graph,
                                     const vector<int> &part,
                                     const vector<vector<int>> &topo_dist) {
    long long hop_cost = 0;
    const int K = static_cast<int>(topo_dist.size());
    vector<int> used;
    vector<int> mark(static_cast<size_t>(K), 0);
    int stamp = 1;

    for (const Net *net : graph.get_nets()) {
        used.clear();
        for (const Node *node : net->get_nodes()) {
            int p = part.at(static_cast<size_t>(node->get_index()));
            if (p < 0 || p >= K) continue;
            if (mark[static_cast<size_t>(p)] != stamp) {
                mark[static_cast<size_t>(p)] = stamp;
                used.push_back(p);
            }
        }
        for (size_t i = 0; i < used.size(); ++i) {
            for (size_t j = i + 1; j < used.size(); ++j) {
                int d = topo_dist[static_cast<size_t>(used[i])][static_cast<size_t>(used[j])];
                if (d > 0 && d < 1000000000) hop_cost += d;
            }
        }
        ++stamp;
        if (stamp == 0x3fffffff) {
            std::fill(mark.begin(), mark.end(), 0);
            stamp = 1;
        }
    }
    return hop_cost;
}
