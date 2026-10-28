#include <bits/stdc++.h>

#include "model/Client.h"
#include "model/Link.h"
#include "model/NetworkGraph.h"
#include "model/Packet.h"
#include "model/Router.h"
#include "services/SimulationService.h"

using namespace std;

namespace {

// Milestone 5: Test queue priority scheduling and same-priority FIFO behavior
void test_link_queue_priority_and_fifo() {
    pbl::model::Link link(1, 2, 100, 5, 0.0, 0.0, 10);

    // Enqueue in reverse order: DATA (p=1), HEARTBEAT (p=2), ROUTE_UPDATE (p=2), ACK (p=3), ROUTE_PROBE (p=3)
    pbl::model::Packet p_data1(pbl::model::Packet::Type::DATA, 1, 2, 101, "data1", 100);
    pbl::model::Packet p_hb(pbl::model::Packet::Type::HEARTBEAT, 1, 2, 201, "hb", 10);
    pbl::model::Packet p_ru(pbl::model::Packet::Type::ROUTE_UPDATE, 1, 2, 202, "ru", 10);
    pbl::model::Packet p_ack(pbl::model::Packet::Type::ACK, 1, 2, 301, "ack", 0);
    pbl::model::Packet p_probe(pbl::model::Packet::Type::ROUTE_PROBE, 1, 2, 302, "probe", 0);
    pbl::model::Packet p_data2(pbl::model::Packet::Type::DATA, 1, 2, 102, "data2", 100);

    link.enqueue(p_data1);
    link.enqueue(p_hb);
    link.enqueue(p_ru);
    link.enqueue(p_ack);
    link.enqueue(p_probe);
    link.enqueue(p_data2);

    assert(link.queue_size() == 6);

    // 1. Highest priority packets (ACK and ROUTE_PROBE, priority 3) must come out first in FIFO order
    pbl::model::Packet out1 = link.dequeue();
    assert(out1.get_type() == pbl::model::Packet::Type::ACK);
    assert(out1.get_sequence_number() == 301);

    pbl::model::Packet out2 = link.dequeue();
    assert(out2.get_type() == pbl::model::Packet::Type::ROUTE_PROBE);
    assert(out2.get_sequence_number() == 302);

    // 2. Medium priority packets (HEARTBEAT and ROUTE_UPDATE, priority 2) must come out next in FIFO order
    pbl::model::Packet out3 = link.dequeue();
    assert(out3.get_type() == pbl::model::Packet::Type::HEARTBEAT);
    assert(out3.get_sequence_number() == 201);

    pbl::model::Packet out4 = link.dequeue();
    assert(out4.get_type() == pbl::model::Packet::Type::ROUTE_UPDATE);
    assert(out4.get_sequence_number() == 202);

    // 3. Lowest priority packets (DATA, priority 1) must come out last in FIFO order
    pbl::model::Packet out5 = link.dequeue();
    assert(out5.get_type() == pbl::model::Packet::Type::DATA);
    assert(out5.get_sequence_number() == 101);

    pbl::model::Packet out6 = link.dequeue();
    assert(out6.get_type() == pbl::model::Packet::Type::DATA);
    assert(out6.get_sequence_number() == 102);

    assert(link.queue_empty());
}

// Milestone 5: Test bounded capacity behavior
void test_bounded_queue_capacity() {
    pbl::model::Link link(1, 2, 100, 5, 0.0, 0.0, 2);

    pbl::model::Packet p1(pbl::model::Packet::Type::DATA, 1, 2, 1, "a", 10);
    pbl::model::Packet p2(pbl::model::Packet::Type::DATA, 1, 2, 2, "b", 10);
    pbl::model::Packet p3(pbl::model::Packet::Type::DATA, 1, 2, 3, "c", 10);

    link.enqueue(p1);
    assert(!link.queue_full());
    link.enqueue(p2);
    assert(link.queue_full());

    // Invariant: Queues never exceed their configured bound
    bool rejected = false;
    try {
        link.enqueue(p3);
    } catch (const overflow_error&) {
        rejected = true;
    }
    assert(rejected);
    assert(link.queue_size() == 2);
}

// Milestone 5: Test transmission timing calculations and fixed 1 ms ticks
void test_transmission_timing_and_ticks() {
    pbl::model::NetworkGraph graph;
    graph.add_client(1, "Client-A");
    graph.add_client(2, "Client-B");

    // Bandwidth = 100 bytes/ms, Latency = 5 ms, Capacity = 5
    graph.add_link(1, 2, 100, 5, 0.0, 0.0, 5);

    pbl::services::SimulationService sim;
    sim.set_network_graph(&graph);

    // Check transmission time formula: ceil(S / B)
    // S = 1024, B = 100 -> ceil(1024 / 100) = 11 ms
    assert(sim.calculate_transmission_time(1024, 100) == 11);
    // S = 200, B = 100 -> ceil(200 / 100) = 2 ms
    assert(sim.calculate_transmission_time(200, 100) == 2);
    // S = 100, B = 100 -> ceil(100 / 100) = 1 ms
    assert(sim.calculate_transmission_time(100, 100) == 1);
    // S = 0 -> 0 ms
    assert(sim.calculate_transmission_time(0, 100) == 0);

    // Send a 1024-byte DATA packet
    pbl::model::Packet pkt(pbl::model::Packet::Type::DATA, 1, 2, 1, "test_payload", 1024);
    assert(sim.send_packet(1, 2, pkt));
    assert(sim.get_statistics().sent_count == 1);

    // Tick 1: clock becomes 1 ms. Link dequeues packet and dispatches it into flight.
    // Transmission time = 11 ms, Latency = 5 ms -> Arrival tick = 1 + 11 + 5 = 17 ms.
    sim.tick();
    assert(sim.get_current_tick() == 1);
    assert(sim.get_in_flight_count() == 1);
    assert(sim.get_delivered_packets().empty());

    // Advance 15 ticks to tick 16: packet is still in flight (elapsed 15 ms < 16 ms total required)
    sim.advance_ticks(15);
    assert(sim.get_current_tick() == 16);
    assert(sim.get_in_flight_count() == 1);
    assert(sim.get_delivered_packets().empty());

    // Tick 17: arrival tick 17 is reached and packet is delivered!
    sim.tick();
    assert(sim.get_current_tick() == 17);
    assert(sim.get_in_flight_count() == 0);
    assert(sim.get_delivered_packets().size() == 1);
    assert(sim.get_statistics().delivered_count == 1);
    assert(sim.get_delivered_packets()[0].get_sequence_number() == 1);
}

// Milestone 5: Test serialization busy state and pipelined packet transmission
void test_link_serialization_busy_pipelining() {
    pbl::model::NetworkGraph graph;
    graph.add_client(1, "Client-A");
    graph.add_client(2, "Client-B");
    // Bandwidth = 100 bytes/ms, Latency = 3 ms
    graph.add_link(1, 2, 100, 3, 0.0, 0.0, 5);

    pbl::services::SimulationService sim;
    sim.set_network_graph(&graph);

    // Enqueue two 200-byte packets (tx_time = 2 ms each)
    pbl::model::Packet pkt1(pbl::model::Packet::Type::DATA, 1, 2, 1, "payload1", 200);
    pbl::model::Packet pkt2(pbl::model::Packet::Type::DATA, 1, 2, 2, "payload2", 200);

    sim.send_packet(1, 2, pkt1);
    sim.send_packet(1, 2, pkt2);

    pbl::model::Link* link = graph.get_link(1, 2);
    assert(link->queue_size() == 2);

    // Tick 1: clock = 1 ms. Link dispatches pkt1. Link is busy until tick 1 + 2 = 3 ms.
    // Arrival for pkt1 = 1 + 2 + 3 = 6 ms.
    sim.tick();
    assert(sim.get_current_tick() == 1);
    assert(link->queue_size() == 1); // pkt2 is waiting in queue!
    assert(sim.get_in_flight_count() == 1);

    // Tick 2: clock = 2 ms. Link is still busy serializing pkt1 (current_tick 2 < busy_until 3).
    sim.tick();
    assert(sim.get_current_tick() == 2);
    assert(link->queue_size() == 1); // pkt2 still waiting!

    // Tick 3: clock = 3 ms. Link is no longer busy (current_tick 3 >= busy_until 3).
    // Now pkt2 is dequeued and dispatched into flight! Link busy until 3 + 2 = 5 ms.
    // Arrival for pkt2 = 3 + 2 + 3 = 8 ms.
    sim.tick();
    assert(sim.get_current_tick() == 3);
    assert(link->queue_empty()); // pkt2 has been dispatched!
    assert(sim.get_in_flight_count() == 2);

    // Advance 2 ticks to tick 5: both still in flight
    sim.advance_ticks(2);
    assert(sim.get_current_tick() == 5);
    assert(sim.get_delivered_packets().empty());

    // Tick 6: pkt1 reaches arrival tick 6 and is delivered!
    sim.tick();
    assert(sim.get_current_tick() == 6);
    assert(sim.get_delivered_packets().size() == 1);
    assert(sim.get_delivered_packets()[0].get_sequence_number() == 1);
    assert(sim.get_in_flight_count() == 1); // pkt2 still in flight

    // Tick 7: pkt2 still in flight
    sim.tick();
    assert(sim.get_current_tick() == 7);
    assert(sim.get_delivered_packets().size() == 1);

    // Tick 8: pkt2 reaches arrival tick 8 and is delivered!
    sim.tick();
    assert(sim.get_current_tick() == 8);
    assert(sim.get_delivered_packets().size() == 2);
    assert(sim.get_delivered_packets()[1].get_sequence_number() == 2);
    assert(sim.get_in_flight_count() == 0);
}

// Milestone 5: Test invariant — packets affected by a failed node or link are dropped
void test_failure_drops_packets() {
    pbl::model::NetworkGraph graph;
    graph.add_client(1, "Client-A");
    graph.add_router(2, "Router-R");
    graph.add_client(3, "Client-B");

    graph.add_link(1, 2, 100, 10, 0.0, 0.0, 5);
    graph.add_link(2, 3, 100, 10, 0.0, 0.0, 5);

    pbl::services::SimulationService sim;
    sim.set_network_graph(&graph);

    // Case 1: In-flight packet dropped upon link failure
    pbl::model::Packet pkt1(pbl::model::Packet::Type::DATA, 1, 2, 10, "data", 100);
    sim.send_packet(1, 2, pkt1);
    sim.tick(); // pkt1 is dispatched into flight on link (1, 2)
    assert(sim.get_in_flight_count() == 1);

    // Fail link (1, 2) during transit
    sim.process_link_failure(1, 2);
    // Invariant: In-flight packet is immediately dropped, NOT delivered!
    assert(sim.get_in_flight_count() == 0);
    assert(sim.get_statistics().failure_drop_count == 1);
    assert(sim.get_statistics().dropped_count == 1);

    // Advance time: packet must never be delivered
    sim.advance_ticks(20);
    assert(sim.get_delivered_packets().empty());

    // Case 2: In-flight packet dropped upon node failure
    graph.recover_link(1, 2);
    pbl::model::Packet pkt2(pbl::model::Packet::Type::DATA, 1, 2, 20, "data", 100);
    sim.send_packet(1, 2, pkt2);
    sim.tick(); // pkt2 is dispatched
    assert(sim.get_in_flight_count() == 1);

    // Fail node 2
    sim.process_node_failure(2);
    assert(sim.get_in_flight_count() == 0);
    assert(sim.get_statistics().failure_drop_count == 2);

    // Case 3: Sending over a failed link or node immediately drops packet
    pbl::model::Packet pkt3(pbl::model::Packet::Type::DATA, 1, 2, 30, "data", 100);
    assert(!sim.send_packet(1, 2, pkt3));
    assert(sim.get_statistics().failure_drop_count == 3);
}

// Milestone 5: Test simulation lifecycle controls (start, pause, resume, reset)
void test_simulation_lifecycle() {
    pbl::services::SimulationService sim;
    assert(sim.get_state() == pbl::services::SimulationService::State::STOPPED);
    assert(!sim.is_running());

    sim.start();
    assert(sim.get_state() == pbl::services::SimulationService::State::RUNNING);
    assert(sim.is_running());

    sim.pause();
    assert(sim.get_state() == pbl::services::SimulationService::State::PAUSED);
    assert(!sim.is_running());

    sim.resume();
    assert(sim.get_state() == pbl::services::SimulationService::State::RUNNING);

    sim.advance_ticks(10);
    assert(sim.get_current_tick() == 10);

    sim.reset();
    assert(sim.get_state() == pbl::services::SimulationService::State::STOPPED);
    assert(sim.get_current_tick() == 0);
    assert(sim.get_statistics().sent_count == 0);
    assert(sim.get_in_flight_count() == 0);
}

// Milestone 8: Test deterministic defaults and packet loss behavior
void test_loss_and_deterministic_defaults() {
    pbl::model::NetworkGraph graph;
    graph.add_client(1, "Client1");
    graph.add_client(2, "Client2");
    // Link with 0.0 default loss and corruption
    graph.add_link(1, 2, 100, 5, 0.0, 0.0, 10);

    pbl::services::SimulationService sim;
    sim.set_network_graph(&graph);
    sim.start();

    // Invariant: Default zero probabilities remain deterministic
    pbl::model::Packet p1(pbl::model::Packet::Type::DATA, 1, 2, 1, "Payload1", 100);
    assert(sim.send_packet(1, 2, p1));
    sim.advance_ticks(10);
    assert(sim.get_delivered_packets().size() == 1);
    assert(sim.get_statistics().lost_count == 0);

    // Now test manual loss injection on the link
    sim.inject_link_loss(1, 2);
    pbl::model::Packet p2(pbl::model::Packet::Type::DATA, 1, 2, 2, "Payload2", 100);
    assert(sim.send_packet(1, 2, p2));

    // Advance ticks to process dispatch
    sim.advance_ticks(10);

    // Packet p2 must have been lost, not delivered!
    assert(sim.get_delivered_packets().size() == 1); // Still only p1 delivered
    assert(sim.get_statistics().lost_count == 1);

    // Review checkpoint: Event log must contain [PACKET_LOSS]
    bool found_loss_log = false;
    const pbl::dsa::DynamicArray<string>& logs = sim.get_event_log();
    for (int i = 0; i < logs.size(); ++i) {
        if (logs[i].find("[PACKET_LOSS]") != string::npos) {
            found_loss_log = true;
            break;
        }
    }
    assert(found_loss_log);
}

// Milestone 8: Test manual packet corruption injection
void test_corruption_injection_and_detection() {
    pbl::model::NetworkGraph graph;
    graph.add_client(1, "Client1");
    graph.add_client(2, "Client2");
    graph.add_link(1, 2, 100, 5, 0.0, 0.0, 10);

    pbl::services::SimulationService sim;
    sim.set_network_graph(&graph);
    sim.start();

    string original_payload = "OriginalPayloadData";
    int original_checksum = 0;
    for (int i = 0; i < int(original_payload.size()); ++i) {
        original_checksum ^= static_cast<unsigned char>(original_payload[i]);
    }

    // Packet created with valid payload and checksum
    pbl::model::Packet p(pbl::model::Packet::Type::DATA, 1, 2, 10, original_payload, 100, original_checksum);

    // Inject manual corruption on link
    sim.inject_link_corruption(1, 2);
    assert(sim.send_packet(1, 2, p));

    sim.advance_ticks(10);

    // Packet arrived, but its payload was corrupted
    assert(sim.get_delivered_packets().size() == 1);
    assert(sim.get_statistics().corrupted_count == 1);

    pbl::model::Packet arrived = sim.get_delivered_packets()[0];
    // Invariant: Corruption is detected before successful delivery acknowledgment
    int arrived_checksum = 0;
    string arrived_payload = arrived.get_payload();
    for (int i = 0; i < int(arrived_payload.size()); ++i) {
        arrived_checksum ^= static_cast<unsigned char>(arrived_payload[i]);
    }
    // Checksum of arrived payload does NOT match expected packet checksum
    assert(arrived_checksum != arrived.get_checksum());

    // Review checkpoint: Event log must contain [PACKET_CORRUPTION]
    bool found_corruption_log = false;
    const pbl::dsa::DynamicArray<string>& logs = sim.get_event_log();
    for (int i = 0; i < logs.size(); ++i) {
        if (logs[i].find("[PACKET_CORRUPTION]") != string::npos) {
            found_corruption_log = true;
            break;
        }
    }
    assert(found_corruption_log);
}

// Milestone 8: Test manual failure takes effect immediately
void test_manual_failure_immediate() {
    pbl::model::NetworkGraph graph;
    graph.add_client(1, "Client1");
    graph.add_router(2, "Router2");
    graph.add_link(1, 2, 100, 5, 0.0, 0.0, 10);

    pbl::services::SimulationService sim;
    sim.set_network_graph(&graph);
    sim.start();

    sim.advance_ticks(15);
    assert(graph.is_node_active(2));

    // Invariant: Manual failure takes effect immediately
    sim.fail_node_manually(2);
    assert(!graph.is_node_active(2));
    assert(sim.get_statistics().manual_failure_count == 1);

    // Review checkpoint: Event log must contain [MANUAL_FAILURE]
    bool found_manual_log = false;
    const pbl::dsa::DynamicArray<string>& logs = sim.get_event_log();
    for (int i = 0; i < logs.size(); ++i) {
        if (logs[i].find("[MANUAL_FAILURE]") != string::npos) {
            found_manual_log = true;
            break;
        }
    }
    assert(found_manual_log);
}

// Milestone 8: Test heartbeat monitoring and timeout failure detection
void test_heartbeat_timeout_failure() {
    pbl::model::NetworkGraph graph;
    graph.add_client(1, "Client1");
    graph.add_router(2, "Router2");
    graph.add_router(3, "Router3");
    graph.add_link(1, 2, 100, 5, 0.0, 0.0, 10);
    graph.add_link(2, 3, 100, 5, 0.0, 0.0, 10);

    pbl::services::SimulationService sim;
    sim.set_network_graph(&graph);
    sim.set_heartbeat_interval(100);
    sim.set_heartbeat_timeout(300);
    sim.set_heartbeat_enabled(true);
    sim.start();

    // Advance 200 ms: Heartbeats emitted at ticks 100 and 200
    sim.advance_ticks(200);
    assert(graph.is_node_active(2));
    assert(graph.is_node_active(3));
    assert(sim.get_statistics().heartbeat_timeout_count == 0);

    // Silence router 2 at tick 200 (stops emitting heartbeats)
    sim.silence_node_heartbeat(2);

    // Advance 299 ms from tick 200 (tick = 499)
    // Invariant: Heartbeat failure changes node/link availability only AFTER the documented timeout (300 ms)
    sim.advance_ticks(299);
    assert(sim.get_current_tick() == 499);
    assert(graph.is_node_active(2)); // Still active at tick 499 (299ms elapsed < 300ms)
    assert(sim.get_statistics().heartbeat_timeout_count == 0);

    // Advance 1 more tick to reach tick 500 (300ms elapsed since tick 200)
    sim.advance_ticks(1);
    assert(sim.get_current_tick() == 500);

    // Router 2 must now have failed due to heartbeat timeout!
    assert(!graph.is_node_active(2));
    assert(graph.is_node_active(3)); // Router 3 continued beating and remains active
    assert(sim.get_statistics().heartbeat_timeout_count == 1);

    // Review checkpoint: Event log must contain [HEARTBEAT_TIMEOUT]
    bool found_hb_timeout_log = false;
    const pbl::dsa::DynamicArray<string>& logs = sim.get_event_log();
    for (int i = 0; i < logs.size(); ++i) {
        if (logs[i].find("[HEARTBEAT_TIMEOUT]") != string::npos) {
            found_hb_timeout_log = true;
            break;
        }
    }
    assert(found_hb_timeout_log);
}

// Milestone 8: Review Checkpoint — verify event log clearly distinguishes all 4 events
void test_review_checkpoint_event_log_distinction() {
    pbl::model::NetworkGraph graph;
    graph.add_client(1, "Client1");
    graph.add_router(2, "Router2");
    graph.add_router(3, "Router3");
    graph.add_client(4, "Client4");
    graph.add_link(1, 2, 100, 5, 0.0, 0.0, 10);
    graph.add_link(2, 3, 100, 5, 0.0, 0.0, 10);
    graph.add_link(3, 4, 100, 5, 0.0, 0.0, 10);

    pbl::services::SimulationService sim;
    sim.set_network_graph(&graph);
    sim.set_heartbeat_interval(100);
    sim.set_heartbeat_timeout(300);
    sim.set_heartbeat_enabled(true);
    sim.start();

    // 1. Trigger [PACKET_LOSS]
    sim.inject_link_loss(1, 2);
    sim.send_packet(1, 2, pbl::model::Packet(pbl::model::Packet::Type::DATA, 1, 2, 1, "test", 10));
    sim.advance_ticks(10);

    // 2. Trigger [PACKET_CORRUPTION]
    sim.inject_link_corruption(2, 3);
    sim.send_packet(2, 3, pbl::model::Packet(pbl::model::Packet::Type::DATA, 2, 3, 2, "test", 10));
    sim.advance_ticks(10);

    // 3. Trigger [MANUAL_FAILURE]
    sim.fail_link_manually(3, 4);

    // 4. Trigger [HEARTBEAT_TIMEOUT]
    sim.silence_node_heartbeat(3);
    sim.advance_ticks(350); // Will exceed 300 ms timeout for router 3

    // Verify all 4 distinct events are present in log
    bool has_loss = false;
    bool has_corruption = false;
    bool has_manual = false;
    bool has_timeout = false;

    const pbl::dsa::DynamicArray<string>& logs = sim.get_event_log();
    for (int i = 0; i < logs.size(); ++i) {
        if (logs[i].find("[PACKET_LOSS]") != string::npos) has_loss = true;
        if (logs[i].find("[PACKET_CORRUPTION]") != string::npos) has_corruption = true;
        if (logs[i].find("[MANUAL_FAILURE]") != string::npos) has_manual = true;
        if (logs[i].find("[HEARTBEAT_TIMEOUT]") != string::npos) has_timeout = true;
    }

    assert(has_loss);
    assert(has_corruption);
    assert(has_manual);
    assert(has_timeout);
}

} // namespace

int main() {
    test_link_queue_priority_and_fifo();
    test_bounded_queue_capacity();
    test_transmission_timing_and_ticks();
    test_link_serialization_busy_pipelining();
    test_failure_drops_packets();
    test_simulation_lifecycle();

    // Milestone 8 tests
    test_loss_and_deterministic_defaults();
    test_corruption_injection_and_detection();
    test_manual_failure_immediate();
    test_heartbeat_timeout_failure();
    test_review_checkpoint_event_log_distinction();

    cout << "All Milestone 5 and Milestone 8 simulation, loss, corruption, and failure detection tests passed successfully!" << endl;
    return 0;
}
