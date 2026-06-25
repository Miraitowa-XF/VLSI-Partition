#include "Net.h"

Net::Net(int index) : index_(index) {}

Net::~Net() {}

void Net::add_node(Node *node) {
    nodes_.push_back(node);
}
