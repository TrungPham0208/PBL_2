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

} // namespace

int main() {
    test_link_queue_priority_and_fifo();
    test_bounded_queue_capacity();
    test_transmission_timing_and_ticks();
    test_link_serialization_busy_pipelining();
    test_failure_drops_packets();
    test_simulation_lifecycle();
    cout << "All Milestone 5 queues and fixed-tick simulation tests passed successfully!" << endl;
    return 0;
}
