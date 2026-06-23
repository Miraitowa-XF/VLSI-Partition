#ifndef DESIGN_H
#define DESIGN_H

#include "Net.h"
#include "Node.h"
#include <map>

class Graph{
    public:
        Graph() : max_node_index_(0) {}
        virtual ~Graph(){}
        vector<Node *> &get_nodes() { return this->nodes; }
        vector<Net *> &get_nets() { return this->nets; }
        int get_node_num() { return this->nodes.size(); }
        int get_net_num() { return this->nets.size(); }
        int get_max_node_index() { return max_node_index_; }
        Node *get_node(int index) {
            auto it = node_map.find(index);
            return (it != node_map.end()) ? it->second : nullptr;
        }
        Net *get_net(int index) {
            auto it = net_map.find(index);
            return (it != net_map.end()) ? it->second : nullptr;
        }
        Node *get_or_create_node(int index);
        Net *add_net(int index);

    private:
        vector<Node *> nodes;
        vector<Net *> nets;
        map<int, Node*> node_map;
        map<int, Net*> net_map;
        int max_node_index_;
};

#endif