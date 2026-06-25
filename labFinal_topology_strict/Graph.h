#ifndef GRAPH_H
#define GRAPH_H

#include <stdexcept>
#include <string>
#include <vector>
#include "Node.h"
#include "Net.h"

class Graph {
public:
    Graph();
    ~Graph();

    Graph(const Graph &) = delete;
    Graph &operator=(const Graph &) = delete;

    void reset(int node_count, int id_base);
    Net *add_net(int index);
    Node *get_or_create_node(int original_id);

    Node *get_node(int internal_index) const;
    Net *get_net(int index) const;

    std::vector<Node *> &get_nodes() { return nodes_; }
    std::vector<Net *> &get_nets() { return nets_; }
    const std::vector<Node *> &get_nodes() const { return nodes_; }
    const std::vector<Net *> &get_nets() const { return nets_; }

    int get_node_num() const { return static_cast<int>(nodes_.size()); }
    int get_net_num() const { return static_cast<int>(nets_.size()); }
    int get_id_base() const { return id_base_; }

    int to_internal_id(int original_id) const;
    int to_original_id(int internal_id) const;
    bool contains_original_id(int original_id) const;

private:
    void clear();

    std::vector<Node *> nodes_;
    std::vector<Net *> nets_;
    int id_base_;
};

#endif
