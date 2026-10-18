#pragma once

#include <bits/stdc++.h>

#include "model/Client.h"
#include "model/Link.h"
#include "model/Node.h"
#include "model/Router.h"
#include "pbl/dsa/AdjacencyList.h"
#include "pbl/dsa/DynamicArray.h"
#include "pbl/dsa/HashMap.h"
#include "pbl/dsa/Queue.h"

using namespace std;

namespace pbl {
namespace model {

// Milestone 3: Core logic — Network graph representation and topology validation.
// This class manages the collection of nodes and undirected links that make up the network.
// It enforces undirected consistency, prevents dangling and duplicate connections,
// and validates topology health (isolated nodes, unreachable pairs, missing alternative routes).
class NetworkGraph {
public:
    // Result object summarizing topology health and any warnings discovered.
    struct ValidationResult {
        bool is_valid;
        dsa::DynamicArray<int> isolated_nodes;
        dsa::DynamicArray<pair<int, int>> unreachable_client_pairs;
        dsa::DynamicArray<pair<int, int>> missing_alternative_routes;
        dsa::DynamicArray<string> warnings;
    };

    NetworkGraph();

    // The destructor cleans up all dynamically allocated Node and Link objects owned by this graph.
    // Memory ownership: NetworkGraph is the sole owner of nodes and links added to it.
    ~NetworkGraph();

    // Release all nodes and links, resetting the graph to an empty state.
    void clear();

    // Node management
    void add_node(Node* node);
    void add_client(int id, string name);
    void add_router(int id, string name);
    bool remove_node(int id);
    bool has_node(int id) const;
    Node* get_node(int id);
    const Node* get_node(int id) const;
    int node_count() const;
    const dsa::DynamicArray<int>& get_node_ids() const;
    dsa::DynamicArray<int> get_client_ids() const;
    dsa::DynamicArray<int> get_router_ids() const;

    // Node availability controls
    bool is_node_active(int id) const;
    void set_node_active(int id, bool active);
    void fail_node(int id);
    void recover_node(int id);

    // Link management (undirected connections)
    void add_link(Link* link);
    void add_link(int first_node_id, int second_node_id, int bandwidth,
                  int latency, double loss_probability = 0.0,
                  double corruption_probability = 0.0, int queue_capacity = 10);
    bool remove_link(int first_node_id, int second_node_id);
    bool has_link(int first_node_id, int second_node_id) const;
    Link* get_link(int first_node_id, int second_node_id);
    const Link* get_link(int first_node_id, int second_node_id) const;
    int link_count() const;
    const dsa::DynamicArray<Link*>& get_all_links() const;

    // Link availability controls
    bool is_link_active(int first_node_id, int second_node_id) const;
    void set_link_active(int first_node_id, int second_node_id, bool active);
    void fail_link(int first_node_id, int second_node_id);
    void recover_link(int first_node_id, int second_node_id);

    // Graph structure and neighbor queries
    dsa::DynamicArray<int> get_neighbors(int node_id) const;
    dsa::DynamicArray<int> get_active_neighbors(int node_id) const;
    const dsa::AdjacencyList<int, Link*>& get_adjacency_list() const;

    // Pathfinding and reachability
    bool is_reachable(int source_id, int destination_id) const;
    dsa::DynamicArray<int> find_path_bfs(int source_id, int destination_id) const;
    bool has_alternative_route(int source_id, int destination_id) const;

    // Topology validation
    ValidationResult validate() const;
    bool is_valid() const;
    dsa::DynamicArray<int> get_isolated_nodes() const;
    dsa::DynamicArray<pair<int, int>> get_unreachable_client_pairs() const;
    dsa::DynamicArray<pair<int, int>> get_missing_alternative_routes() const;

    // Random topology generation with deterministic seed
    void generate_random_topology(int client_count, int router_count,
                                 double connection_density, int seed = 42);

private:
    dsa::HashMap<int, Node*> nodes_;
    dsa::DynamicArray<int> node_ids_;
    dsa::DynamicArray<Link*> links_;
    dsa::AdjacencyList<int, Link*> adjacency_list_;

    // Helper to test reachability excluding a specific undirected edge
    bool is_reachable_excluding_edge(int source_id, int destination_id,
                                    int excluded_first, int excluded_second) const;
};

}
}
