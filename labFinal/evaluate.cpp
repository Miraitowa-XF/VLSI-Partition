#include <iostream>
#include <fstream>
#include <sstream>
#include "evaluate.h"

using namespace std;

int evaluate_kway(Graph &graph, const vector<int>& part, int K) {
    long long cut = 0;
    size_t part_sz = part.size();
    for (const auto& net : graph.get_nets()) {
        const vector<Node *>& nodes = net->get_nodes();
        if (nodes.size() < 2) continue;
        int anchor_idx = nodes[0]->get_index();
        if (anchor_idx < 0 || anchor_idx >= (int)part_sz) continue;
        int anchor_p = part[anchor_idx];
        if (anchor_p < 0 || anchor_p >= K) continue;
        for (size_t j = 1; j < nodes.size(); ++j) {
            int idx = nodes[j]->get_index();
            if (idx < 0 || idx >= (int)part_sz) continue;
            int pj = part[idx];
            if (pj >= 0 && pj < K && pj != anchor_p) cut++;
        }
    }
    return (int)cut;
}

int evaluate(Graph graph, string partition_name, int K) {
    vector<int> part(graph.get_node_num() + 1, 0);
    ifstream partition_file(partition_name);
    int i = 1;
    string line;
    while(getline(partition_file, line)) {
        istringstream iss(line);
        iss >> part[i];
        i++;
    }
    return evaluate_kway(graph, part, K);
}

// 计算拓扑违规数（二端度量：first-pin 星形展开后逐二端边统计）
int evaluate_violations(Graph &graph, const vector<int>& part, int K,
                        const vector<vector<bool>>& topo_adj) {
    int violations = 0;
    size_t part_sz = part.size();
    for (const auto& net : graph.get_nets()) {
        const vector<Node *>& nodes = net->get_nodes();
        if (nodes.size() < 2) continue;
        int anchor_idx = nodes[0]->get_index();
        if (anchor_idx < 0 || anchor_idx >= (int)part_sz) continue;
        int anchor_p = part[anchor_idx];
        if (anchor_p < 0 || anchor_p >= K) continue;
        for (size_t j = 1; j < nodes.size(); ++j) {
            int idx = nodes[j]->get_index();
            if (idx < 0 || idx >= (int)part_sz) continue;
            int pj = part[idx];
            if (pj < 0 || pj >= K || pj == anchor_p) continue;
            if (!topo_adj[anchor_p][pj]) violations++;
        }
    }
    return violations;
}
