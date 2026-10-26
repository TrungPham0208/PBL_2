#pragma once

#include <bits/stdc++.h>

#include "model/Link.h"
#include "model/NetworkGraph.h"
#include "pbl/dsa/DynamicArray.h"
#include "pbl/dsa/HashMap.h"
#include "pbl/dsa/PriorityQueue.h"

using namespace std;

namespace pbl {
namespace services {

// Milestone 4: Core logic — Dijkstra routing
// RoutingService calculates the lowest-cost path between nodes using Dijkstra's algorithm.
// It centralizes the non-negative combined link cost formula (latency, bandwidth, loss probability),
// uses the custom PriorityQueue for min-priority scheduling via inverted priorities,
// properly discards stale priority queue entries, and guarantees that failed nodes or links
// are never selected.
class RoutingService {
public:
    // Route result for pathfinding between a source and destination
    struct Route {
        bool found;
        double total_cost;
        dsa::DynamicArray<int> path; // Sequence of node IDs: [source, ..., destination]

        Route() {
            found = false;
            total_cost = 0.0;
        }

        int hop_count() const {
            if (path.size() <= 1) {
                return 0;
            }
            return path.size() - 1;
        }

        int get_next_hop() const {
            if (path.size() >= 2) {
                return path[1];
            }
            return -1;
        }
    };

    // Entry in a routing table mapping a destination to the next hop neighbor
    struct RoutingTableEntry {
        int destination_id;
        int next_hop_id;
        double total_cost;
        dsa::DynamicArray<int> full_path;

        RoutingTableEntry() {
            destination_id = -1;
            next_hop_id = -1;
            total_cost = 0.0;
        }
    };

    // Internal entry for priority queue in Dijkstra's algorithm
    struct DijkstraEntry {
        int node_id;
        double cost;

        DijkstraEntry() {
            node_id = 0;
            cost = 0.0;
        }

        DijkstraEntry(int id, double c) {
            node_id = id;
            cost = c;
        }
    };

    RoutingService();
    // RoutingService does not own dynamically allocated memory, so no custom destructor is needed.

    // Centralized cost formula:
    // cost = (weight_latency * latency) + (weight_bandwidth * (reference_bandwidth / bandwidth)) + (weight_loss * loss_probability * 100.0)
    double calculate_link_cost(const model::Link& link) const;
    double calculate_cost(int latency, int bandwidth, double loss_probability) const;

    void set_weights(double latency_weight, double bandwidth_weight, double loss_weight, double reference_bandwidth);
    double get_latency_weight() const;
    double get_bandwidth_weight() const;
    double get_loss_weight() const;
    double get_reference_bandwidth() const;

    // Dijkstra shortest path calculation
    // Invariant: Failed nodes and failed links are never traversed.
    // Invariant: Route costs are strictly non-negative.
    // Invariant: Unreachable destinations are explicitly reported with found = false.
    // Invariant: Route calculation does not mutate packet or queue state.
    Route find_route(const model::NetworkGraph& graph, int source_id, int destination_id) const;

    // Calculate routing table for source_id to all reachable active nodes
    dsa::DynamicArray<RoutingTableEntry> calculate_routing_table(const model::NetworkGraph& graph, int source_id) const;

    // Checks whether an existing route is still fully active and valid in the graph
    bool is_route_valid(const model::NetworkGraph& graph, const Route& route) const;

    // Recalculates route after a network failure
    Route recalculate_route(const model::NetworkGraph& graph, int source_id, int destination_id) const;

private:
    double latency_weight_;
    double bandwidth_weight_;
    double loss_weight_;
    double reference_bandwidth_;

    // Helper to scale floating-point cost into an integer priority for PriorityQueue
    int cost_to_priority(double cost) const;
};

}
}
