#ifndef EVALUATE_H
#define EVALUATE_H

#include <string>
#include <vector>
#include "Graph.h"

int evaluate_kway(Graph &graph, const std::vector<int> &part, int K);
int evaluate(Graph &graph, const std::string &partition_name, int K);
int evaluate_topology_violations(Graph &graph,
                                 const std::vector<int> &part,
                                 const std::vector<std::vector<int>> &topo_allowed);
long long evaluate_topology_hop_cost(Graph &graph,
                                     const std::vector<int> &part,
                                     const std::vector<std::vector<int>> &topo_dist);

#endif
