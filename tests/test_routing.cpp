#include <bits/stdc++.h>

#include "model/Client.h"
#include "model/Link.h"
#include "model/NetworkGraph.h"
#include "model/Packet.h"
#include "model/Router.h"
#include "services/RoutingService.h"

using namespace std;

namespace {

// Milestone 4: Test centralized routing cost formula and metric influence
void test_cost_formula() {
    pbl::services::RoutingService router_service;

    // Default weights: latency=1.0, bandwidth=1.0, loss=2.0, ref_bw=100.0
    // cost = (1.0 * latency) + (1.0 * (100.0 / bandwidth)) + (2.0 * loss * 100.0)
    double base_cost = router_service.calculate_cost(10, 100, 0.0);
    // latency_term = 10.0, bw_term = 100/100 = 1.0, loss_term = 0 -> total = 11.0
    assert(abs(base_cost - 11.0) < 1e-6);

    // 1. Latency metric influence: increasing latency increases cost
    double higher_latency_cost = router_service.calculate_cost(20, 100, 0.0);
    assert(higher_latency_cost > base_cost);
    assert(abs(higher_latency_cost - 21.0) < 1e-6);

    // 2. Bandwidth metric influence: lower bandwidth increases cost (inversely proportional)
    double lower_bw_cost = router_service.calculate_cost(10, 50, 0.0);
    // latency_term = 10.0, bw_term = 100/50 = 2.0 -> total = 12.0
    assert(lower_bw_cost > base_cost);
    assert(abs(lower_bw_cost - 12.0) < 1e-6);

    // 3. Loss metric influence: higher packet loss adds retransmission penalty
    double higher_loss_cost = router_service.calculate_cost(10, 100, 0.1);
    // loss_term = 2.0 * (0.1 * 100.0) = 20.0 -> total = 31.0
    assert(higher_loss_cost > base_cost);
    assert(abs(higher_loss_cost - 31.0) < 1e-6);

    // Invariant: cost is strictly non-negative
    double zero_cost = router_service.calculate_cost(0, 1000, 0.0);
    assert(zero_cost >= 0.0);

    // Test weight adjustment
    router_service.set_weights(2.0, 3.0, 1.0, 200.0);
    assert(router_service.get_latency_weight() == 2.0);
    assert(router_service.get_bandwidth_weight() == 3.0);
    assert(router_service.get_loss_weight() == 1.0);
    assert(router_service.get_reference_bandwidth() == 200.0);

    // Test rejection of negative weights
    bool rejected_weight = false;
    try {
        router_service.set_weights(-1.0, 1.0, 1.0, 100.0);
    } catch (const invalid_argument&) {
        rejected_weight = true;
    }
    assert(rejected_weight);
}

// Milestone 4: Test basic Dijkstra pathfinding and route reconstruction
void test_basic_dijkstra() {
    pbl::model::NetworkGraph graph;
    graph.add_client(1, "Client-A");
    graph.add_router(2, "Router-R1");
    graph.add_client(3, "Client-B");

    // Link (1, 2): latency 5ms, bw 100 B/ms, loss 0 -> cost = 5 + (100/100) = 6.0
    graph.add_link(1, 2, 100, 5, 0.0);
    // Link (2, 3): latency 10ms, bw 100 B/ms, loss 0 -> cost = 10 + (100/100) = 11.0
    graph.add_link(2, 3, 100, 10, 0.0);

    pbl::services::RoutingService service;
    pbl::services::RoutingService::Route route_1_to_3 = service.find_route(graph, 1, 3);

    assert(route_1_to_3.found);
    assert(route_1_to_3.path.size() == 3);
    assert(route_1_to_3.path[0] == 1);
    assert(route_1_to_3.path[1] == 2);
    assert(route_1_to_3.path[2] == 3);
    assert(abs(route_1_to_3.total_cost - 17.0) < 1e-6);
    assert(route_1_to_3.hop_count() == 2);
    assert(route_1_to_3.get_next_hop() == 2);

    // Reverse direction (undirected consistency)
    pbl::services::RoutingService::Route route_3_to_1 = service.find_route(graph, 3, 1);
    assert(route_3_to_1.found);
    assert(route_3_to_1.path.size() == 3);
    assert(route_3_to_1.path[0] == 3);
    assert(route_3_to_1.path[1] == 2);
    assert(route_3_to_1.path[2] == 1);
    assert(abs(route_3_to_1.total_cost - 17.0) < 1e-6);

    // Trivial case: source is destination
    pbl::services::RoutingService::Route route_same = service.find_route(graph, 1, 1);
    assert(route_same.found);
    assert(route_same.path.size() == 1);
    assert(route_same.path[0] == 1);
    assert(route_same.total_cost == 0.0);
    assert(route_same.hop_count() == 0);
    assert(route_same.get_next_hop() == -1);
}

// Milestone 4: Test that Dijkstra always selects the lowest-cost available route
void test_lowest_cost_selection() {
    pbl::model::NetworkGraph graph;
    graph.add_client(1, "Client-Source");
    graph.add_router(2, "Router-FastPath");
    graph.add_router(3, "Router-SlowPath");
    graph.add_client(4, "Client-Destination");

    // Path A (via Router 2): Fast path, latency 5ms each, cost ~ (5 + 1) * 2 = 12.0
    graph.add_link(1, 2, 100, 5, 0.0);
    graph.add_link(2, 4, 100, 5, 0.0);

    // Path B (via Router 3): Slow path, latency 50ms each, cost ~ (50 + 1) * 2 = 102.0
    graph.add_link(1, 3, 100, 50, 0.0);
    graph.add_link(3, 4, 100, 50, 0.0);

    pbl::services::RoutingService service;
    pbl::services::RoutingService::Route route = service.find_route(graph, 1, 4);

    // Optimal route is Path A
    assert(route.found);
    assert(route.path.size() == 3);
    assert(route.path[0] == 1 && route.path[1] == 2 && route.path[2] == 4);
    assert(abs(route.total_cost - 12.0) < 1e-6);

    // Now introduce high packet loss on Link (1, 2) (loss = 0.8 -> adds 2.0 * 80 = 160.0 to Path A)
    // Path A total cost becomes ~ 172.0, which is worse than Path B's 102.0
    graph.get_link(1, 2)->set_loss_probability(0.8);

    pbl::services::RoutingService::Route new_route = service.find_route(graph, 1, 4);
    assert(new_route.found);
    // Dijkstra switches to Path B (via Router 3) due to loss penalty!
    assert(new_route.path.size() == 3);
    assert(new_route.path[0] == 1 && new_route.path[1] == 3 && new_route.path[2] == 4);
    assert(abs(new_route.total_cost - 102.0) < 1e-6);
}

// Milestone 4: Test invariant — Dijkstra never selects failed nodes
void test_failed_nodes_avoidance() {
    pbl::model::NetworkGraph graph;
    graph.add_client(1, "Client-1");
    graph.add_router(2, "Router-2");
    graph.add_router(3, "Router-3");
    graph.add_client(4, "Client-4");

    // Path A: 1-2-4 (cost 12.0)
    graph.add_link(1, 2, 100, 5, 0.0);
    graph.add_link(2, 4, 100, 5, 0.0);
    // Path B: 1-3-4 (cost 30.0)
    graph.add_link(1, 3, 100, 14, 0.0);
    graph.add_link(3, 4, 100, 14, 0.0);

    pbl::services::RoutingService service;

    // Normal state selects Path A
    assert(service.find_route(graph, 1, 4).path[1] == 2);

    // Fail Router 2: Dijkstra must exclude Router 2 and reroute via Router 3
    graph.fail_node(2);
    pbl::services::RoutingService::Route route_after_node_fail = service.find_route(graph, 1, 4);
    assert(route_after_node_fail.found);
    assert(route_after_node_fail.path[1] == 3);

    // Fail Router 3 as well: no paths remain, so unreachable
    graph.fail_node(3);
    pbl::services::RoutingService::Route no_route = service.find_route(graph, 1, 4);
    assert(!no_route.found);
    assert(no_route.path.empty());

    // Recover Router 2: path via Router 2 becomes available again
    graph.recover_node(2);
    pbl::services::RoutingService::Route recovered_route = service.find_route(graph, 1, 4);
    assert(recovered_route.found);
    assert(recovered_route.path[1] == 2);

    // Fail source node: cannot route
    graph.fail_node(1);
    assert(!service.find_route(graph, 1, 4).found);
    graph.recover_node(1);

    // Fail destination node: cannot route
    graph.fail_node(4);
    assert(!service.find_route(graph, 1, 4).found);
}

// Milestone 4: Test invariant — Dijkstra never selects failed links
void test_failed_links_avoidance() {
    pbl::model::NetworkGraph graph;
    graph.add_client(1, "Client-1");
    graph.add_router(2, "Router-2");
    graph.add_router(3, "Router-3");
    graph.add_client(4, "Client-4");

    // Path A: 1-2-4
    graph.add_link(1, 2, 100, 5, 0.0);
    graph.add_link(2, 4, 100, 5, 0.0);
    // Path B: 1-3-4
    graph.add_link(1, 3, 100, 14, 0.0);
    graph.add_link(3, 4, 100, 14, 0.0);

    pbl::services::RoutingService service;

    // Initially selects Path A (via 2)
    assert(service.find_route(graph, 1, 4).path[1] == 2);

    // Fail link (1, 2): must reroute via Path B (via 3)
    graph.fail_link(1, 2);
    pbl::services::RoutingService::Route route_after_link_fail = service.find_route(graph, 1, 4);
    assert(route_after_link_fail.found);
    assert(route_after_link_fail.path[1] == 3);

    // Fail link (3, 4): all routes severed
    graph.fail_link(3, 4);
    assert(!service.find_route(graph, 1, 4).found);

    // Recover link (1, 2)
    graph.recover_link(1, 2);
    assert(service.find_route(graph, 1, 4).found);
    assert(service.find_route(graph, 1, 4).path[1] == 2);
}

// Milestone 4: Test invariant — unreachable destinations reported explicitly
void test_unreachable_explicitly_reported() {
    pbl::model::NetworkGraph graph;
    // Component 1
    graph.add_client(1, "Client-1");
    graph.add_router(2, "Router-2");
    graph.add_link(1, 2, 100, 5);

    // Component 2 (disconnected)
    graph.add_client(3, "Client-3");
    graph.add_router(4, "Router-4");
    graph.add_link(3, 4, 100, 5);

    pbl::services::RoutingService service;
    pbl::services::RoutingService::Route unreachable = service.find_route(graph, 1, 3);
    assert(!unreachable.found);
    assert(unreachable.path.empty());
    assert(unreachable.total_cost == 0.0);

    // Non-existent node id
    pbl::services::RoutingService::Route invalid_node = service.find_route(graph, 1, 999);
    assert(!invalid_node.found);
    assert(invalid_node.path.empty());
}

// Milestone 4: Test stale priority queue entries and multi-path relaxation
void test_stale_entries_and_multi_path_relaxation() {
    // Triangle graph:
    // 1 -> 2 with direct cost 20
    // 1 -> 3 with cost 2, 3 -> 2 with cost 3
    // Path 1 -> 3 -> 2 has total cost 5, which is cheaper than direct link 1 -> 2 (20).
    // In Dijkstra:
    // When relaxing from 1: node 2 is inserted with cost 20.
    // When relaxing from 3: node 2 is inserted with cost 5.
    // The PriorityQueue extracts cost 5 first and finalizes node 2.
    // Later, the entry for node 2 with cost 20 is extracted, recognized as stale, and discarded!
    pbl::model::NetworkGraph graph;
    graph.add_client(1, "Client-1");
    graph.add_router(2, "Router-2");
    graph.add_router(3, "Router-3");

    // Link (1, 2): latency 19ms -> cost = 19 + 1 = 20.0
    graph.add_link(1, 2, 100, 19, 0.0);
    // Link (1, 3): latency 1ms -> cost = 1 + 1 = 2.0
    graph.add_link(1, 3, 100, 1, 0.0);
    // Link (3, 2): latency 2ms -> cost = 2 + 1 = 3.0
    graph.add_link(3, 2, 100, 2, 0.0);

    pbl::services::RoutingService service;
    pbl::services::RoutingService::Route route = service.find_route(graph, 1, 2);

    assert(route.found);
    assert(route.path.size() == 3);
    assert(route.path[0] == 1 && route.path[1] == 3 && route.path[2] == 2);
    assert(abs(route.total_cost - 5.0) < 1e-6);
}

// Milestone 4: Test routing table generation and route validity checking
void test_routing_table_and_validity() {
    pbl::model::NetworkGraph graph;
    graph.add_client(1, "Client-1");
    graph.add_router(2, "Router-2");
    graph.add_client(3, "Client-3");
    graph.add_link(1, 2, 100, 5);
    graph.add_link(2, 3, 100, 5);

    pbl::services::RoutingService service;

    // Check routing table for Client 1
    pbl::dsa::DynamicArray<pbl::services::RoutingService::RoutingTableEntry> table =
        service.calculate_routing_table(graph, 1);

    assert(table.size() == 2); // Entries for Router 2 and Client 3
    for (int i = 0; i < table.size(); ++i) {
        if (table[i].destination_id == 2) {
            assert(table[i].next_hop_id == 2);
        } else if (table[i].destination_id == 3) {
            assert(table[i].next_hop_id == 2); // Next hop from 1 to 3 is 2
        }
    }

    // Check route validity
    pbl::services::RoutingService::Route valid_route = service.find_route(graph, 1, 3);
    assert(service.is_route_valid(graph, valid_route));

    // Fail Router 2: route must now be flagged invalid
    graph.fail_node(2);
    assert(!service.is_route_valid(graph, valid_route));

    // Recover Router 2: route becomes valid again
    graph.recover_node(2);
    assert(service.is_route_valid(graph, valid_route));

    // Fail link (1, 2): route becomes invalid
    graph.fail_link(1, 2);
    assert(!service.is_route_valid(graph, valid_route));
}

// Milestone 4: Test invariant — route calculation does not mutate packet or queue state
void test_packet_state_non_mutation() {
    pbl::model::NetworkGraph graph;
    graph.add_client(1, "Client-1");
    graph.add_router(2, "Router-2");
    graph.add_link(1, 2, 100, 5);

    pbl::model::Link* link = graph.get_link(1, 2);
    pbl::model::Packet test_packet(pbl::model::Packet::Type::DATA, 1, 2, 1, "test", 4);
    link->enqueue(test_packet);
    assert(link->queue_size() == 1);

    pbl::services::RoutingService service;
    pbl::services::RoutingService::Route route = service.find_route(graph, 1, 2);
    assert(route.found);

    // Queue on link must remain completely unchanged
    assert(link->queue_size() == 1);
    assert(link->front().get_payload() == "test");
}

} // namespace

int main() {
    test_cost_formula();
    test_basic_dijkstra();
    test_lowest_cost_selection();
    test_failed_nodes_avoidance();
    test_failed_links_avoidance();
    test_unreachable_explicitly_reported();
    test_stale_entries_and_multi_path_relaxation();
    test_routing_table_and_validity();
    test_packet_state_non_mutation();
    cout << "All Milestone 4 Dijkstra routing tests passed successfully!" << endl;
    return 0;
}
