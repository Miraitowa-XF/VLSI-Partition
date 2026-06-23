#include "Graph.h"
#include "Net.h"
#include "Node.h"

// Node *Graph::get_or_create_node(int index) {
//     for(auto node : nodes) {
//         if(node->get_index() == index)  return node;
//     }
//     Node *node = new Node(index);
//     nodes.push_back(node);
//     node_map[index] = node;
//     return node;
// }

Node *Graph::get_or_create_node(int index) {
    auto it = node_map.find(index);
    if(it != node_map.end()) {
        return it->second;
    }

    Node *node = new Node(index);
    nodes.push_back(node);
    node_map[index] = node;
    if (index > max_node_index_) max_node_index_ = index;
    return node;
}

Net *Graph::add_net(int index) {
    Net *net = new Net(index);
    this->nets.push_back(net);
    net_map[index] = net;
    return net;
}