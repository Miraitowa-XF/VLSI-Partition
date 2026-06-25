#include "Graph.h"

Graph::Graph() : id_base_(0) {}

Graph::~Graph() {
    clear();
}

void Graph::clear() {
    for (Node *node : nodes_) delete node;
    for (Net *net : nets_) delete net;
    nodes_.clear();
    nets_.clear();
}

void Graph::reset(int node_count, int id_base) {
    if (node_count < 0) {
        throw std::runtime_error("Graph::reset got negative node_count");
    }
    clear();
    id_base_ = id_base;
    nodes_.reserve(static_cast<size_t>(node_count));
    for (int i = 0; i < node_count; ++i) {
        nodes_.push_back(new Node(i, i + id_base_));
    }
}

int Graph::to_internal_id(int original_id) const {
    return original_id - id_base_;
}

int Graph::to_original_id(int internal_id) const {
    return internal_id + id_base_;
}

bool Graph::contains_original_id(int original_id) const {
    int idx = to_internal_id(original_id);
    return idx >= 0 && idx < get_node_num();
}

Node *Graph::get_or_create_node(int original_id) {
    int idx = to_internal_id(original_id);
    if (idx < 0 || idx >= get_node_num()) {
        throw std::runtime_error("node id " + std::to_string(original_id) +
                                 " is out of benchmark range");
    }
    return nodes_[static_cast<size_t>(idx)];
}

Node *Graph::get_node(int internal_index) const {
    if (internal_index < 0 || internal_index >= get_node_num()) {
        throw std::runtime_error("internal node index out of range");
    }
    return nodes_[static_cast<size_t>(internal_index)];
}

Net *Graph::add_net(int index) {
    Net *net = new Net(index);
    nets_.push_back(net);
    return net;
}

Net *Graph::get_net(int index) const {
    if (index < 0 || index >= get_net_num()) {
        throw std::runtime_error("net index out of range");
    }
    return nets_[static_cast<size_t>(index)];
}
