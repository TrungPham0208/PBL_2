#include <bits/stdc++.h>

#include "services/RoutingService.h"

using namespace std;

namespace pbl {
namespace services {

// Milestone 4: Core logic — Dijkstra routing
// Constructor initializes default formula weights inside the constructor body.
// We avoid high-level member initializer lists (: weight(1.0)) to keep code beginner-friendly.
RoutingService::RoutingService() {
    latency_weight_ = 1.0;
    bandwidth_weight_ = 1.0;
    loss_weight_ = 2.0;
    reference_bandwidth_ = 100.0;
}

// Sets the weights used in the centralized routing cost formula.
void RoutingService::set_weights(double latency_weight, double bandwidth_weight,
                                 double loss_weight, double reference_bandwidth) {
    if (latency_weight < 0.0 || bandwidth_weight < 0.0 || loss_weight < 0.0 || reference_bandwidth <= 0.0) {
        throw invalid_argument("routing weights and reference bandwidth must be non-negative");
    }
    latency_weight_ = latency_weight;
    bandwidth_weight_ = bandwidth_weight;
    loss_weight_ = loss_weight;
    reference_bandwidth_ = reference_bandwidth;
}

double RoutingService::get_latency_weight() const {
    return latency_weight_;
}

double RoutingService::get_bandwidth_weight() const {
    return bandwidth_weight_;
}

double RoutingService::get_loss_weight() const {
    return loss_weight_;
}

double RoutingService::get_reference_bandwidth() const {
    return reference_bandwidth_;
}

// Centralized link cost calculation from a Link reference.
// Protects invariant: route costs are non-negative.
double RoutingService::calculate_link_cost(const model::Link& link) const {
    return calculate_cost(link.get_latency(), link.get_bandwidth(), link.get_loss_probability());
}

// Documented cost formula combining latency, bandwidth, and loss probability:
// cost = (weight_latency * latency) + (weight_bandwidth * (reference_bandwidth / bandwidth)) + (weight_loss * loss_probability * 100.0)
// Why each metric affects the score:
// 1. Latency (ms): Higher latency directly delays round-trip times and packet forwarding.
// 2. Bandwidth (bytes/ms): Lower bandwidth increases transmission time (S / B), so cost is inversely proportional to bandwidth.
// 3. Loss probability [0.0, 1.0]: Loss triggers timeouts and retransmissions, adding an expected delay penalty.
double RoutingService::calculate_cost(int latency, int bandwidth, double loss_probability) const {
    if (latency < 0) {
        latency = 0;
    }
    if (bandwidth <= 0) {
        bandwidth = 1; // Prevent division by zero
    }
    if (loss_probability < 0.0) {
        loss_probability = 0.0;
    }

    double latency_term = latency_weight_ * latency;
    double bandwidth_term = bandwidth_weight_ * (reference_bandwidth_ / bandwidth);
    double loss_term = loss_weight_ * (loss_probability * 100.0);

    double total_cost = latency_term + bandwidth_term + loss_term;
    if (total_cost < 0.0) {
        total_cost = 0.0; // Invariant: costs must remain non-negative
    }
    return total_cost;
}

// Converts a non-negative floating point cost to an integer priority for PriorityQueue.
// PriorityQueue is a max-heap (larger priority integer is extracted first).
// By negating the scaled cost (-cost), the minimum cost becomes the largest number,
// allowing our custom PriorityQueue to behave as a min-heap for Dijkstra's algorithm.
int RoutingService::cost_to_priority(double cost) const {
    const double SCALE = 1000.0;
    int scaled = static_cast<int>(cost * SCALE);
    return -scaled;
}

// Dijkstra shortest path calculation.
// Guarantees:
// - Failed nodes and links are never selected.
// - Lowest cost path is discovered.
// - Unreachable destinations are explicitly reported.
// - Link queues and packet states are never mutated.
RoutingService::Route RoutingService::find_route(const model::NetworkGraph& graph,
                                                int source_id, int destination_id) const {
    Route result;

    // Check node existence
    if (!graph.has_node(source_id) || !graph.has_node(destination_id)) {
        return result; // Not found
    }

    // Invariant: Dijkstra never selects failed nodes
    if (!graph.is_node_active(source_id) || !graph.is_node_active(destination_id)) {
        return result; // Not found if source or destination is failed
    }

    // Trivial case: source is destination
    if (source_id == destination_id) {
        result.found = true;
        result.total_cost = 0.0;
        result.path.push_back(source_id);
        return result;
    }

    // Data structures for Dijkstra's algorithm using custom DSA
    dsa::HashMap<int, double> min_cost;
    dsa::HashMap<int, int> predecessor;
    dsa::HashMap<int, bool> finalized;
    dsa::PriorityQueue<DijkstraEntry> pq;

    min_cost.put(source_id, 0.0);
    pq.push(DijkstraEntry(source_id, 0.0), cost_to_priority(0.0));

    while (!pq.empty()) {
        DijkstraEntry current = pq.pop();
        int u = current.node_id;
        double current_cost = current.cost;

        // Invariant: never traverse through a failed node
        if (!graph.is_node_active(u)) {
            continue;
        }

        // Stale entry check:
        // When multiple paths to node u are relaxed, multiple entries for u are pushed to pq.
        // If we already finalized u or discovered a shorter cost, this entry is stale and discarded.
        if (finalized.contains(u) && finalized.at(u)) {
            continue;
        }
        if (min_cost.contains(u) && current_cost > min_cost.at(u)) {
            continue;
        }

        // Mark node u as finalized
        finalized.put(u, true);

        // Optimal stopping: edge weights are non-negative, so the first time
        // destination_id is extracted from pq, its shortest path is guaranteed.
        if (u == destination_id) {
            break;
        }

        // Examine active neighbors
        dsa::DynamicArray<int> neighbors = graph.get_neighbors(u);
        for (int i = 0; i < neighbors.size(); ++i) {
            int v = neighbors[i];

            // Invariant: Dijkstra never selects failed nodes or failed links
            if (!graph.is_node_active(v)) {
                continue;
            }
            if (!graph.is_link_active(u, v)) {
                continue;
            }
            if (finalized.contains(v) && finalized.at(v)) {
                continue;
            }

            const model::Link* link = graph.get_link(u, v);
            if (link == nullptr) {
                continue;
            }

            double edge_cost = calculate_link_cost(*link);
            double new_cost = current_cost + edge_cost;

            // Relaxation step
            if (!min_cost.contains(v) || new_cost < min_cost.at(v)) {
                min_cost.put(v, new_cost);
                predecessor.put(v, u);
                pq.push(DijkstraEntry(v, new_cost), cost_to_priority(new_cost));
            }
        }
    }

    // If destination was not reached, return unreached result
    if (!min_cost.contains(destination_id) || !finalized.contains(destination_id)) {
        return result;
    }

    // Reconstruct route by following predecessors backwards from destination to source
    dsa::DynamicArray<int> reverse_path;
    int curr = destination_id;
    while (curr != source_id) {
        reverse_path.push_back(curr);
        if (!predecessor.contains(curr)) {
            return result; // Break on inconsistency
        }
        curr = predecessor.at(curr);
    }
    reverse_path.push_back(source_id);

    result.found = true;
    result.total_cost = min_cost.at(destination_id);

    // Invert reverse_path to produce [source, ..., destination]
    for (int i = reverse_path.size() - 1; i >= 0; --i) {
        result.path.push_back(reverse_path[i]);
    }

    return result;
}

// Calculates the routing table for source_id to all reachable active nodes in the network
dsa::DynamicArray<RoutingService::RoutingTableEntry>
RoutingService::calculate_routing_table(const model::NetworkGraph& graph, int source_id) const {
    dsa::DynamicArray<RoutingTableEntry> table;

    if (!graph.has_node(source_id) || !graph.is_node_active(source_id)) {
        return table;
    }

    const dsa::DynamicArray<int>& all_nodes = graph.get_node_ids();
    for (int i = 0; i < all_nodes.size(); ++i) {
        int destination_id = all_nodes[i];
        if (destination_id == source_id) {
            continue;
        }
        if (!graph.is_node_active(destination_id)) {
            continue;
        }

        Route route = find_route(graph, source_id, destination_id);
        if (route.found) {
            RoutingTableEntry entry;
            entry.destination_id = destination_id;
            entry.next_hop_id = route.get_next_hop();
            entry.total_cost = route.total_cost;
            entry.full_path = route.path;
            table.push_back(entry);
        }
    }

    return table;
}

// Verifies if an existing route remains fully usable (no failed nodes or links along its path)
bool RoutingService::is_route_valid(const model::NetworkGraph& graph, const Route& route) const {
    if (!route.found || route.path.empty()) {
        return false;
    }

    // Check all nodes in the path
    for (int i = 0; i < route.path.size(); ++i) {
        int node_id = route.path[i];
        if (!graph.has_node(node_id) || !graph.is_node_active(node_id)) {
            return false;
        }
    }

    // Check all link segments in the path
    for (int i = 0; i < route.path.size() - 1; ++i) {
        int u = route.path[i];
        int v = route.path[i + 1];
        if (!graph.has_link(u, v) || !graph.is_link_active(u, v)) {
            return false;
        }
    }

    return true;
}

// Recalculates route after a network failure
RoutingService::Route RoutingService::recalculate_route(const model::NetworkGraph& graph,
                                                        int source_id, int destination_id) const {
    return find_route(graph, source_id, destination_id);
}

}
}
