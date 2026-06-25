#ifndef NODE_H
#define NODE_H

#include <vector>

class Net;

class Node {
public:
    Node(int internal_index, int original_id);
    ~Node();

    int get_index() const { return internal_index_; }      // 内部编号：0..N-1
    int get_original_id() const { return original_id_; }   // 文件中的原始编号：0-based 或 1-based

    void add_net(Net *net);
    std::vector<Net *> &get_nets() { return nets_; }
    const std::vector<Net *> &get_nets() const { return nets_; }

private:
    int internal_index_;
    int original_id_;
    std::vector<Net *> nets_;
};

#endif
