// node.hpp
#ifndef NODE_HPP
#define NODE_HPP

struct Node {
    int x, y;
    float gCost, hCost, fCost;
    Node* parent;

    bool operator>(const Node& other) const {
        return fCost > other.fCost;
    }
};

#endif