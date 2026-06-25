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
    // 修复：使用已有的 node_map 进行 O(log N) 快速查找
    auto it = node_map.find(index);
    if(it != node_map.end()) {
        return it->second;
    }
    
    // 如果没找到，再新建
    Node *node = new Node(index);
    nodes.push_back(node);
    node_map[index] = node;
    return node;
}

Net *Graph::add_net(int index) {
    Net *net = new Net(index);
    this->nets.push_back(net);
    net_map[index] = net;
    return net;
}

void Graph::clear_maps() {
    node_map.clear();
    net_map.clear();
}