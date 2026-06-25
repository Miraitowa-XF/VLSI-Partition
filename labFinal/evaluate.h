#ifndef EVALUATE_H
#define EVALUATE_H
#include <vector>
#include "Graph.h"

int evaluate_kway(Graph &graph, const std::vector<int>& part, int K);
int evaluate(Graph graph, std::string partition_name, int K);

// 计算拓扑违规数：遍历所有二端边，检查 (part[u], part[v]) 是否在 topo_adj 中
int evaluate_violations(Graph &graph, const std::vector<int>& part, int K,
                        const std::vector<std::vector<bool>>& topo_adj);

#endif
