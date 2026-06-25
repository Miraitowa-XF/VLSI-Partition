#ifndef NET_H
#define NET_H

#include <vector>

class Node;

class Net {
public:
    explicit Net(int index);
    ~Net();

    void add_node(Node *node);
    int get_index() const { return index_; }

    std::vector<Node *> &get_nodes() { return nodes_; }
    const std::vector<Node *> &get_nodes() const { return nodes_; }

private:
    int index_;
    std::vector<Node *> nodes_;
};

#endif
