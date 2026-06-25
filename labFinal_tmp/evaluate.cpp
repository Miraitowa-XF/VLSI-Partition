#include <iostream>
#include <fstream>
#include <sstream>
#include "evaluate.h"

using namespace std;

int evaluate_kway(Graph &graph, const vector<int>& part, int K) {
    int cut = 0;
    vector<Net *> nets = graph.get_nets();
    for (const auto& net : nets) {
        vector<bool> spans(K, false);
        int span_count = 0;
        for (const auto node : net->get_nodes()) {
            int p = part[node->get_index()];
            if (!spans[p]) {
                spans[p] = true;
                span_count++;
            }
        }
        if (span_count > 1) {
            cut += (span_count - 1);
        }
    }
    return cut;
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