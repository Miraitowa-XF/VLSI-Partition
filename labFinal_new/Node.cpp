#include "Node.h"

Node::Node(int internal_index, int original_id)
    : internal_index_(internal_index), original_id_(original_id) {}

Node::~Node() {}

void Node::add_net(Net *net) {
    nets_.push_back(net);
}
