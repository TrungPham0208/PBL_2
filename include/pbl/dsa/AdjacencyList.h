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

    void add_node(const NodeId& node) {
        if (find_node(node) == nullptr) {
            nodes_.push_back(Vertex{node, {}});
        }
    }

    bool remove_node(const NodeId& node) {
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

    void add_undirected_edge(const NodeId& first, const NodeId& second, const EdgeValue& value) {
        if (first == second) {
                throw invalid_argument("self-loops are not supported");
        }
        add_node(first);
        add_node(second);
        if (has_edge(first, second)) {
                throw invalid_argument("duplicate adjacency edge");
        }
        find_node(first)->edges.push_back(Edge{second, value});
        find_node(second)->edges.push_back(Edge{first, value});
    }

    bool remove_undirected_edge(const NodeId& first, const NodeId& second) {
        Vertex* first_vertex = find_node(first);
        Vertex* second_vertex = find_node(second);
        if (first_vertex == nullptr || second_vertex == nullptr || !has_edge(first, second)) {
            return false;
        }
        remove_directed_edge(*first_vertex, second);
        remove_directed_edge(*second_vertex, first);
        return true;
    }

    bool has_node(const NodeId& node) const { return find_node(node) != nullptr; }

    bool has_edge(const NodeId& first, const NodeId& second) const {
        const Vertex* vertex = find_node(first);
        if (vertex == nullptr) {
            return false;
        }
        for (int index = 0; index < vertex->edges.size(); ++index) {
            if (vertex->edges[index].destination == second) {
                return true;
            }
        }
        return false;
    }

    const DynamicArray<Edge>& neighbors(const NodeId& node) const {
        const Vertex* vertex = find_node(node);
        if (vertex == nullptr) {
                throw out_of_range("node is not present in adjacency list");
        }
        return vertex->edges;
    }

    int node_count() const { return nodes_.size(); }

private:
    struct Vertex {
        NodeId id;
        DynamicArray<Edge> edges;
    };

    DynamicArray<Vertex> nodes_;

    Vertex* find_node(const NodeId& node) {
        for (int index = 0; index < nodes_.size(); ++index) {
            if (nodes_[index].id == node) {
                return &nodes_[index];
            }
        }
        return nullptr;
    }

    const Vertex* find_node(const NodeId& node) const {
        for (int index = 0; index < nodes_.size(); ++index) {
            if (nodes_[index].id == node) {
                return &nodes_[index];
            }
        }
        return nullptr;
    }

    int node_index(const NodeId& node) const {
        for (int index = 0; index < nodes_.size(); ++index) {
            if (nodes_[index].id == node) {
                return index;
            }
        }
        return nodes_.size();
    }

    void remove_directed_edge(Vertex& vertex, const NodeId& destination) {
        for (int index = 0; index < vertex.edges.size(); ++index) {
            if (vertex.edges[index].destination == destination) {
                vertex.edges[index] = move(vertex.edges[vertex.edges.size() - 1]);
                vertex.edges.pop_back();
                return;
            }
        }
    }
};

}  // namespace dsa
}  // namespace pbl