#include <bits/stdc++.h>

#include "model/Client.h"
#include "model/Link.h"
#include "model/NetworkGraph.h"
#include "model/Packet.h"
#include "model/Router.h"
#include "services/RoutingService.h"
#include "services/SimulationService.h"
#include "services/TransferService.h"

using namespace std;

namespace {

// Milestone 9: Invariant: A failed node or link is removed from the usable route immediately.
void test_failed_element_removed_immediately() {
    pbl::model::NetworkGraph graph;
    graph.add_client(1, "Client-A");
    graph.add_router(2, "Router-B");
    graph.add_router(3, "Router-C");
    graph.add_router(4, "Router-D");
    graph.add_client(5, "Client-E");

    // Primary route: 1 - 2 - 3 - 5 (latency 5ms per hop)
    graph.add_link(1, 2, 100, 5, 0.0, 0.0, 10);
    graph.add_link(2, 3, 100, 5, 0.0, 0.0, 10);
    graph.add_link(3, 5, 100, 5, 0.0, 0.0, 10);

    // Alternate route: 2 - 4 - 5 (higher latency 20ms per hop)
    graph.add_link(2, 4, 100, 20, 0.0, 0.0, 10);
    graph.add_link(4, 5, 100, 20, 0.0, 0.0, 10);

    pbl::services::RoutingService routing;

    // Initial shortest path should be through Router 3
    pbl::services::RoutingService::Route initial_route = routing.find_route(graph, 1, 5);
    assert(initial_route.found);
    assert(initial_route.path.size() == 4);
    assert(initial_route.path[0] == 1);
    assert(initial_route.path[1] == 2);
    assert(initial_route.path[2] == 3);
    assert(initial_route.path[3] == 5);
    assert(routing.is_route_valid(graph, initial_route));

    // Case 1: Link failure removes link from usable routes immediately
    graph.fail_link(2, 3);
    assert(!routing.is_route_valid(graph, initial_route));

    pbl::services::RoutingService::Route reroute1 = routing.recalculate_route(graph, 1, 5);
    assert(reroute1.found);
    assert(reroute1.path.size() == 4);
    assert(reroute1.path[0] == 1);
    assert(reroute1.path[1] == 2);
    assert(reroute1.path[2] == 4);
    assert(reroute1.path[3] == 5);

    // Case 2: Node failure removes node from usable routes immediately
    graph.fail_node(4);
    assert(!routing.is_route_valid(graph, reroute1));

    pbl::services::RoutingService::Route no_route = routing.recalculate_route(graph, 1, 5);
    // Invariant: Unreachable destinations are explicitly reported
    assert(!no_route.found);
}

// Milestone 9: Invariant: In-transit packets are dropped, never magically moved to a new route.
void test_in_transit_packets_dropped_never_magically_moved() {
    pbl::model::NetworkGraph graph;
    graph.add_client(1, "Client-A");
    graph.add_router(2, "Router-B");
    graph.add_router(3, "Router-C");
    graph.add_router(4, "Router-D");
    graph.add_client(5, "Client-E");

    // Primary route: 1 - 2 - 3 - 5
    graph.add_link(1, 2, 100, 5, 0.0, 0.0, 10);
    graph.add_link(2, 3, 100, 20, 0.0, 0.0, 10); // 20 ms transit latency
    graph.add_link(3, 5, 100, 5, 0.0, 0.0, 10);

    // Alternate route: 2 - 4 - 5
    graph.add_link(2, 4, 100, 10, 0.0, 0.0, 10);
    graph.add_link(4, 5, 100, 10, 0.0, 0.0, 10);

    pbl::services::RoutingService routing;
    pbl::services::SimulationService sim;
    sim.set_network_graph(&graph);
    sim.set_routing_service(&routing);
    sim.start();

    // Send packet from 2 towards 5 along link 2 -> 3
    pbl::model::Packet pkt(pbl::model::Packet::Type::DATA, 1, 5, 100, "InTransitData", 100);
    assert(sim.send_packet(2, 3, pkt));

    // Tick 1: Packet is dispatched on link 2 -> 3 and is in flight (arrival at tick 1 + 1 + 20 = 22)
    sim.tick();
    assert(sim.get_in_flight_count() == 1);
    assert(sim.get_delivered_packets().empty());

    // Advance 5 ticks: Packet is currently midway in flight across link 2 -> 3
    sim.advance_ticks(5);
    assert(sim.get_in_flight_count() == 1);

    // Simulate failure of link 2 -> 3 while packet is in flight
    sim.fail_link_manually(2, 3);

    // Advance ticks through original arrival time
    sim.advance_ticks(30);

    // Invariant: In-transit packet was dropped upon link failure, never magically moved to route 2 -> 4 -> 5!
    assert(sim.get_in_flight_count() == 0);
    assert(sim.get_delivered_packets().empty());
    assert(sim.get_statistics().failure_drop_count >= 1);
    assert(sim.get_statistics().delivered_count == 0);
}

// Milestone 9: Invariant: Acknowledged chunks remain acknowledged; only missing chunks are retried.
void test_acknowledged_chunks_preserved_only_missing_retried() {
    pbl::services::TransferService transfer;
    transfer.init_sender("ChunkA_ChunkB_ChunkC_", 7); // 3 chunks (7, 7, 7)
    assert(transfer.get_sender_chunk_count() == 3);

    // Chunks 0 and 1 are dispatched and acknowledged
    transfer.send_chunk(0, 1, 5, 0);
    transfer.send_chunk(1, 1, 5, 0);
    transfer.process_ack_packet(transfer.create_ack_packet(5, 1, 0));
    transfer.process_ack_packet(transfer.create_ack_packet(5, 1, 1));

    assert(transfer.is_chunk_acknowledged(0));
    assert(transfer.is_chunk_acknowledged(1));
    assert(!transfer.is_chunk_acknowledged(2));

    // Send chunk 2, but link failure occurs and its ACK never arrives
    transfer.send_chunk(2, 1, 5, 0);

    // Invariant: Only unacknowledged/missing chunks are retried after rerouting
    pbl::dsa::DynamicArray<int> unacked = transfer.get_unacknowledged_chunks();
    assert(unacked.size() == 1);
    assert(unacked[0] == 2);

    // Acknowledged chunks 0 and 1 are preserved and cannot be resent
    bool resend_0_rejected = false;
    try {
        transfer.send_chunk(0, 1, 5, 50);
    } catch (const runtime_error&) {
        resend_0_rejected = true;
    }
    assert(resend_0_rejected);
}

// Milestone 9: Review Checkpoint — Complete recovery sequence from failure detection to resumed transfer
// Walkthrough: file transfer starts -> network problem occurs -> failure is detected ->
// route changes -> missing data is retransmitted -> transfer completes successfully.
void test_complete_recovery_sequence_with_alternate_route() {
    pbl::model::NetworkGraph graph;
    graph.add_client(1, "Client-Source");
    graph.add_router(2, "Router-Entry");
    graph.add_router(3, "Router-Primary");
    graph.add_router(4, "Router-Alternate");
    graph.add_client(5, "Client-Dest");

    // Primary path: 1 -> 2 -> 3 -> 5 (low latency = 5ms per hop)
    graph.add_link(1, 2, 100, 5, 0.0, 0.0, 10);
    graph.add_link(2, 3, 100, 5, 0.0, 0.0, 10);
    graph.add_link(3, 5, 100, 5, 0.0, 0.0, 10);

    // Alternate path: 2 -> 4 -> 5 (higher latency = 15ms per hop)
    graph.add_link(2, 4, 100, 15, 0.0, 0.0, 10);
    graph.add_link(4, 5, 100, 15, 0.0, 0.0, 10);

    pbl::services::RoutingService routing;
    pbl::services::SimulationService sim;
    sim.set_network_graph(&graph);
    sim.set_routing_service(&routing);
    sim.start();

    // 1. Prepare file transfer (2 chunks: "Part1_" and "Part2_")
    string original_file = "Part1_Part2_";
    pbl::services::TransferService transfer;
    transfer.init_sender(original_file, 6);
    transfer.init_receiver(2);

    // 2. Discover best initial route: 1 -> 2 -> 3 -> 5
    pbl::services::RoutingService::Route active_route = routing.find_route(graph, 1, 5);
    assert(active_route.found);
    assert(active_route.path[2] == 3); // Via primary router 3

    // 3. Dispatch chunk 0 along active route
    pbl::model::Packet data0 = transfer.send_chunk(0, 1, 5, sim.get_current_tick());
    assert(sim.send_packet(1, active_route.path[1], data0));

    // Advance ticks to allow chunk 0 to reach destination and ACK to return
    sim.advance_ticks(50);

    // Receiver accepts chunk 0
    assert(sim.get_delivered_packets().size() >= 1);
    pbl::model::Packet delivered0 = sim.get_delivered_packets()[0];
    assert(transfer.receive_chunk_packet(delivered0));

    // Receiver generates ACK 0
    pbl::model::Packet ack0 = transfer.create_ack_packet(5, 1, 0);
    // Client 1 receives ACK 0
    assert(transfer.process_ack_packet(ack0));
    assert(transfer.is_chunk_acknowledged(0));

    // 4. Dispatch chunk 1 along primary route
    pbl::model::Packet data1 = transfer.send_chunk(1, 1, 5, sim.get_current_tick());
    assert(sim.send_packet(1, active_route.path[1], data1));

    // 5. Network problem occurs: Router 3 fails while chunk 1 is being forwarded!
    sim.advance_ticks(8); // Chunk 1 is at router 2 / link 2-3
    sim.fail_node_manually(3); // Invariant: Failure takes effect immediately!

    // In-transit packet is dropped by failure
    sim.advance_ticks(50);
    // Destination did NOT receive chunk 1
    assert(!transfer.has_chunk(1));
    assert(!transfer.is_chunk_acknowledged(1));

    // 6. Sender detects timeout for missing chunk 1 (100 ms ACK timeout)
    sim.advance_ticks(60);
    pbl::dsa::DynamicArray<int> timeouts = transfer.check_timeouts(sim.get_current_tick());
    assert(timeouts.size() == 1);
    assert(timeouts[0] == 1);

    // 7. Route recalculation: Router 3 is failed, Dijkstra finds alternate route 1 -> 2 -> 4 -> 5
    pbl::services::RoutingService::Route new_route = routing.recalculate_route(graph, 1, 5);
    assert(new_route.found);
    assert(new_route.path[2] == 4); // Rerouted through Router 4!

    // 8. Invariant: Retransmit ONLY unacknowledged chunk 1 through replacement route
    pbl::dsa::DynamicArray<int> missing = transfer.get_unacknowledged_chunks();
    assert(missing.size() == 1);
    assert(missing[0] == 1);

    pbl::model::Packet retry_data1 = transfer.send_chunk(1, 1, 5, sim.get_current_tick());
    assert(sim.send_packet(1, new_route.path[1], retry_data1));

    // 9. Advance simulation ticks to allow chunk 1 to traverse replacement path and arrive
    int prev_delivered_count = sim.get_delivered_packets().size();
    sim.advance_ticks(80);
    assert(sim.get_delivered_packets().size() > prev_delivered_count);

    // Receiver processes delivered chunk 1
    pbl::model::Packet delivered1 = sim.get_delivered_packets()[sim.get_delivered_packets().size() - 1];
    assert(transfer.receive_chunk_packet(delivered1));

    // Receiver sends ACK 1, sender processes ACK 1
    pbl::model::Packet ack1 = transfer.create_ack_packet(5, 1, 1);
    assert(transfer.process_ack_packet(ack1));

    // 10. Invariant: Recovery succeeds and returns original reconstructed file
    assert(transfer.is_transfer_completed());
    assert(transfer.is_reconstruction_ready());
    assert(transfer.reconstruct_file() == original_file);
}

// Milestone 9: Invariant: Recovery reports failure when no alternate route exists.
void test_recovery_fails_when_no_alternate_route_exists() {
    pbl::model::NetworkGraph graph;
    graph.add_client(1, "Client-Source");
    graph.add_router(2, "Bridge-Router");
    graph.add_client(3, "Client-Dest");

    // Single linear topology: 1 - 2 - 3
    graph.add_link(1, 2, 100, 5, 0.0, 0.0, 10);
    graph.add_link(2, 3, 100, 5, 0.0, 0.0, 10);

    pbl::services::RoutingService routing;
    pbl::services::SimulationService sim;
    sim.set_network_graph(&graph);
    sim.set_routing_service(&routing);
    sim.start();

    pbl::services::TransferService transfer;
    transfer.init_sender("IrrecoverableData", 10);

    // Initial send
    transfer.send_chunk(0, 1, 3, 0);

    // Failure of bridge router cuts off all routes
    sim.fail_node_manually(2);

    // Dijkstra route recalculation confirms no alternative route exists
    pbl::services::RoutingService::Route alt_route = routing.recalculate_route(graph, 1, 3);
    assert(!alt_route.found);

    // Retransmissions fail and reach retry limit
    transfer.check_timeouts(100); // Attempt 1 timeout -> retry 1
    transfer.send_chunk(0, 1, 3, 100);
    transfer.check_timeouts(200); // Attempt 2 timeout -> retry 2
    transfer.send_chunk(0, 1, 3, 200);
    transfer.check_timeouts(300); // Attempt 3 timeout -> maximum reached!

    // Invariant: Recovery fails and preserves evidence in statistics
    assert(transfer.is_transfer_failed());
    assert(transfer.get_statistics().failed_count == 1);
}

} // namespace

int main() {
    test_failed_element_removed_immediately();
    test_in_transit_packets_dropped_never_magically_moved();
    test_acknowledged_chunks_preserved_only_missing_retried();
    test_complete_recovery_sequence_with_alternate_route();
    test_recovery_fails_when_no_alternate_route_exists();

    cout << "All Milestone 9 rerouting and recovery tests passed successfully!" << endl;
    return 0;
}
