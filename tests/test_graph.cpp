#include <bits/stdc++.h>

#include "model/Client.h"
#include "model/Link.h"
#include "model/NetworkGraph.h"
#include "model/Router.h"

using namespace std;

namespace {

// Milestone 3: Test node and link operations, undirection consistency, and cleanup
void test_node_and_link_operations() {
    pbl::model::NetworkGraph graph;

    // Add nodes
    graph.add_client(1, "Client-A");
    graph.add_router(2, "Router-R1");
    graph.add_client(3, "Client-B");

    assert(graph.node_count() == 3);
    assert(graph.has_node(1));
    assert(graph.has_node(2));
    assert(graph.has_node(3));
    assert(!graph.has_node(4));

    assert(graph.get_node(1)->get_type() == "Client");
    assert(graph.get_node(2)->get_type() == "Router");

    pbl::dsa::DynamicArray<int> clients = graph.get_client_ids();
    assert(clients.size() == 2);
    pbl::dsa::DynamicArray<int> routers = graph.get_router_ids();
    assert(routers.size() == 1);

    // Add undirected links
    graph.add_link(1, 2, 100, 5, 0.0, 0.0, 10);
    graph.add_link(2, 3, 200, 10, 0.0, 0.0, 10);

    assert(graph.link_count() == 2);
    assert(graph.has_link(1, 2));
    assert(graph.has_link(2, 1)); // Shared status from both endpoints
    assert(graph.has_link(2, 3));
    assert(graph.has_link(3, 2));
    assert(!graph.has_link(1, 3));

    // Verify both endpoints access the exact same Link object
    pbl::model::Link* link_1_2 = graph.get_link(1, 2);
    pbl::model::Link* link_2_1 = graph.get_link(2, 1);
    assert(link_1_2 != nullptr);
    assert(link_1_2 == link_2_1);

    // Neighbors check
    pbl::dsa::DynamicArray<int> n1 = graph.get_neighbors(1);
    assert(n1.size() == 1 && n1[0] == 2);
    pbl::dsa::DynamicArray<int> n2 = graph.get_neighbors(2);
    assert(n2.size() == 2);

    // Link removal
    assert(graph.remove_link(1, 2));
    assert(graph.link_count() == 1);
    assert(!graph.has_link(1, 2));
    assert(!graph.has_link(2, 1));
    assert(!graph.remove_link(1, 2)); // Duplicate removal returns false

    // Node removal cleans up connected links and adjacency entries
    assert(graph.remove_node(2));
    assert(graph.node_count() == 2);
    assert(!graph.has_node(2));
    assert(graph.link_count() == 0); // Link between 2 and 3 was cleanly removed
    assert(!graph.has_link(2, 3));
    assert(!graph.has_link(3, 2));
}

// Milestone 3: Test prevention of duplicate and dangling connections
void test_duplicate_and_dangling_guards() {
    pbl::model::NetworkGraph graph;
    graph.add_client(1, "Client-A");
    graph.add_client(2, "Client-B");

    // Duplicate node addition rejection
    bool rejected_duplicate_node = false;
    try {
        graph.add_client(1, "Duplicate-Client");
    } catch (const invalid_argument&) {
        rejected_duplicate_node = true;
    }
    assert(rejected_duplicate_node);

    // Null node pointer rejection
    bool rejected_null_node = false;
    try {
        graph.add_node(nullptr);
    } catch (const invalid_argument&) {
        rejected_null_node = true;
    }
    assert(rejected_null_node);

    // Dangling link rejection (endpoint not in graph)
    bool rejected_dangling_link = false;
    try {
        graph.add_link(1, 999, 100, 5);
    } catch (const invalid_argument&) {
        rejected_dangling_link = true;
    }
    assert(rejected_dangling_link);

    // Duplicate link rejection
    graph.add_link(1, 2, 100, 5);
    bool rejected_duplicate_link = false;
    try {
        graph.add_link(2, 1, 100, 5);
    } catch (const invalid_argument&) {
        rejected_duplicate_link = true;
    }
    assert(rejected_duplicate_link);
}

// Milestone 3: Test active and failed state handling for nodes and links
void test_active_state_and_routing_isolation() {
    pbl::model::NetworkGraph graph;
    graph.add_client(1, "Client-1");
    graph.add_router(2, "Router-2");
    graph.add_client(3, "Client-3");

    graph.add_link(1, 2, 100, 5);
    graph.add_link(2, 3, 100, 5);

    // Initially everything is active
    assert(graph.is_node_active(1));
    assert(graph.is_node_active(2));
    assert(graph.is_node_active(3));
    assert(graph.is_link_active(1, 2));
    assert(graph.is_link_active(2, 3));
    assert(graph.is_reachable(1, 3));

    // Fail link (1, 2)
    graph.fail_link(1, 2);
    assert(!graph.is_link_active(1, 2));
    assert(!graph.is_link_active(2, 1)); // Invariant: shared state in both directions
    assert(graph.get_active_neighbors(1).empty()); // Invariant: failed links excluded from routing
    assert(!graph.is_reachable(1, 3));

    // Recover link (1, 2)
    graph.recover_link(1, 2);
    assert(graph.is_link_active(1, 2));
    assert(graph.is_reachable(1, 3));

    // Fail router 2
    graph.fail_node(2);
    assert(!graph.is_node_active(2));
    assert(graph.get_active_neighbors(1).empty()); // Invariant: failed nodes excluded from routing
    assert(!graph.is_reachable(1, 3));

    // Recover router 2
    graph.recover_node(2);
    assert(graph.is_node_active(2));
    assert(graph.is_reachable(1, 3));
}

// Milestone 3: Test topology validation for isolated nodes
void test_validation_isolated_nodes() {
    pbl::model::NetworkGraph graph;
    graph.add_client(1, "Client-A");
    graph.add_client(2, "Client-B");
    graph.add_router(3, "Router-Isolated");

    graph.add_link(1, 2, 100, 5);

    pbl::model::NetworkGraph::ValidationResult result = graph.validate();
    assert(!result.is_valid);
    assert(result.isolated_nodes.size() == 1);
    assert(result.isolated_nodes[0] == 3);
}

// Milestone 3: Test topology validation for unreachable client pairs
void test_validation_unreachable_clients() {
    pbl::model::NetworkGraph graph;
    // Component 1
    graph.add_client(1, "Client-1");
    graph.add_router(2, "Router-2");
    graph.add_link(1, 2, 100, 5);

    // Component 2 (disconnected from Component 1)
    graph.add_client(3, "Client-3");
    graph.add_router(4, "Router-4");
    graph.add_link(3, 4, 100, 5);

    pbl::model::NetworkGraph::ValidationResult result = graph.validate();
    assert(!result.is_valid);
    assert(result.unreachable_client_pairs.size() == 1);
    assert(result.unreachable_client_pairs[0].first == 1);
    assert(result.unreachable_client_pairs[0].second == 3);
}

// Milestone 3: Test detection of missing alternative routes (single point of failure)
void test_validation_alternative_routes() {
    // Single path topology (bridge between clients): Client1 - Router1 - Client2
    pbl::model::NetworkGraph graph_single_path;
    graph_single_path.add_client(1, "Client-1");
    graph_single_path.add_router(2, "Router-2");
    graph_single_path.add_client(3, "Client-3");
    graph_single_path.add_link(1, 2, 100, 5);
    graph_single_path.add_link(2, 3, 100, 5);

    assert(graph_single_path.is_reachable(1, 3));
    assert(!graph_single_path.has_alternative_route(1, 3));

    pbl::model::NetworkGraph::ValidationResult result1 = graph_single_path.validate();
    assert(!result1.is_valid);
    assert(result1.missing_alternative_routes.size() == 1);

    // Fault-tolerant topology with a redundant backup route:
    // Client1 connects to Router2 and Router4
    // Client3 connects to Router2 and Router4
    // Cycle: 1 - 2 - 3 - 4 - 1
    pbl::model::NetworkGraph graph_redundant;
    graph_redundant.add_client(1, "Client-1");
    graph_redundant.add_router(2, "Router-2");
    graph_redundant.add_client(3, "Client-3");
    graph_redundant.add_router(4, "Router-4");
    graph_redundant.add_link(1, 2, 100, 5);
    graph_redundant.add_link(2, 3, 100, 5);
    graph_redundant.add_link(3, 4, 100, 5);
    graph_redundant.add_link(4, 1, 100, 5);

    assert(graph_redundant.is_reachable(1, 3));
    assert(graph_redundant.has_alternative_route(1, 3));

    pbl::model::NetworkGraph::ValidationResult result2 = graph_redundant.validate();
    assert(result2.is_valid);
    assert(result2.isolated_nodes.empty());
    assert(result2.unreachable_client_pairs.empty());
    assert(result2.missing_alternative_routes.empty());
}

// Milestone 3: Test random topology generation reproducibility
void test_random_topology_generation() {
    pbl::model::NetworkGraph graph1;
    graph1.generate_random_topology(2, 3, 0.5, 12345);

    pbl::model::NetworkGraph graph2;
    graph2.generate_random_topology(2, 3, 0.5, 12345);

    // With the exact same seed, topology structure must be identical
    assert(graph1.node_count() == graph2.node_count());
    assert(graph1.link_count() == graph2.link_count());

    for (int i = 0; i < graph1.node_count(); ++i) {
        int id = graph1.get_node_ids()[i];
        assert(graph2.has_node(id));
        pbl::dsa::DynamicArray<int> n1 = graph1.get_neighbors(id);
        pbl::dsa::DynamicArray<int> n2 = graph2.get_neighbors(id);
        assert(n1.size() == n2.size());
    }
}

} // namespace

int main() {
    test_node_and_link_operations();
    test_duplicate_and_dangling_guards();
    test_active_state_and_routing_isolation();
    test_validation_isolated_nodes();
    test_validation_unreachable_clients();
    test_validation_alternative_routes();
    test_random_topology_generation();
    cout << "All Milestone 3 graph tests passed successfully!" << endl;
    return 0;
}
