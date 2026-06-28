#ifndef EVALUATE_H
#define EVALUATE_H
#include <vector>
#include "Graph.h"

int evaluate_kway(Graph &graph, const std::vector<int>& part, int K);
int evaluate(Graph graph, std::string partition_name, int K);

#endif