#pragma once

#include <bits/stdc++.h>

#include "pbl/dsa/DynamicArray.h"

namespace pbl {
namespace dsa {

using namespace std;

template <typename NodeId, typename EdgeValue>
class AdjacencyList {
public:
    struct Edge {
        NodeId destination;
        EdgeValue value;
    };
    void add_node(const NodeId& node);
    bool remove_node(const NodeId& node);
    void add_undirected_edge(const NodeId& first, const NodeId& second, const EdgeValue& value);
    bool remove_undirected_edge(const NodeId& first, const NodeId& second);
    bool has_node(const NodeId& node) const;
    bool has_edge(const NodeId& first, const NodeId& second) const;
    const DynamicArray<Edge>& neighbors(const NodeId& node) const;
    int node_count() const;

private:
    struct Vertex {
        NodeId id;
        DynamicArray<Edge> edges;
    };

    DynamicArray<Vertex> nodes_;

    int node_index(const NodeId& node) const;
    void remove_directed_edge(Vertex& vertex, const NodeId& destination);

};

template <typename NodeId, typename EdgeValue>
void AdjacencyList<NodeId, EdgeValue>::add_node(const NodeId& node) {
    if (node_index(node) == nodes_.size()) {
        nodes_.push_back(Vertex{node, {}});
    }
}

template <typename NodeId, typename EdgeValue>
bool AdjacencyList<NodeId, EdgeValue>::remove_node(const NodeId& node) {
    const int index = node_index(node);
    if (index == nodes_.size()) {
        return false;
    }
    nodes_[index] = move(nodes_[nodes_.size() - 1]);
    nodes_.pop_back();
    for (int vertex = 0; vertex < nodes_.size(); ++vertex) {
        remove_directed_edge(nodes_[vertex], node);
    }
    return true;
}

template <typename NodeId, typename EdgeValue>
void AdjacencyList<NodeId, EdgeValue>::add_undirected_edge(const NodeId& first, const NodeId& second, const EdgeValue& value) {
    if (first == second) {
        throw invalid_argument("self-loops are not supported");
    }
    add_node(first);
    add_node(second);
    if (has_edge(first, second)) {
        throw invalid_argument("duplicate adjacency edge");
    }
    int first_index = node_index(first);
    int second_index = node_index(second);
    nodes_[first_index].edges.push_back(Edge{second, value});
    nodes_[second_index].edges.push_back(Edge{first, value});
}

template <typename NodeId, typename EdgeValue>
bool AdjacencyList<NodeId, EdgeValue>::remove_undirected_edge(const NodeId& first, const NodeId& second) {
    int first_index = node_index(first);
    int second_index = node_index(second);
    if (first_index == nodes_.size() || second_index == nodes_.size() || !has_edge(first, second)) {
        return false;
    }
    remove_directed_edge(nodes_[first_index], second);
    remove_directed_edge(nodes_[second_index], first);
    return true;
}

template <typename NodeId, typename EdgeValue>
bool AdjacencyList<NodeId, EdgeValue>::has_node(const NodeId& node) const {
    return node_index(node) != nodes_.size();
}

template <typename NodeId, typename EdgeValue>
bool AdjacencyList<NodeId, EdgeValue>::has_edge(const NodeId& first, const NodeId& second) const {
    int vertex_index = node_index(first);
    if (vertex_index == nodes_.size()) {
        return false;
    }
    for (int index = 0; index < nodes_[vertex_index].edges.size(); ++index) {
        if (nodes_[vertex_index].edges[index].destination == second) {
            return true;
        }
    }
    return false;
}

template <typename NodeId, typename EdgeValue>
const DynamicArray<typename AdjacencyList<NodeId, EdgeValue>::Edge>&
AdjacencyList<NodeId, EdgeValue>::neighbors(const NodeId& node) const {
    int vertex_index = node_index(node);
    if (vertex_index == nodes_.size()) {
        throw out_of_range("node is not present in adjacency list");
    }
    return nodes_[vertex_index].edges;
}

template <typename NodeId, typename EdgeValue>
int AdjacencyList<NodeId, EdgeValue>::node_count() const {
    return nodes_.size();
}

template <typename NodeId, typename EdgeValue>
int AdjacencyList<NodeId, EdgeValue>::node_index(const NodeId& node) const {
    for (int index = 0; index < nodes_.size(); ++index) {
        if (nodes_[index].id == node) {
            return index;
        }
    }
    return nodes_.size();
}

template <typename NodeId, typename EdgeValue>
void AdjacencyList<NodeId, EdgeValue>::remove_directed_edge(Vertex& vertex, const NodeId& destination) {
    for (int index = 0; index < vertex.edges.size(); ++index) {
        if (vertex.edges[index].destination == destination) {
            // replaces the removed node with the last node
            vertex.edges[index] = move(vertex.edges[vertex.edges.size() - 1]);
            vertex.edges.pop_back();
            return;
        }
    }
}

}  // namespace dsa
}  // namespace pbl