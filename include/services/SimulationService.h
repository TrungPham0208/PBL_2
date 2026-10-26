#pragma once

#include <bits/stdc++.h>

#include "model/Link.h"
#include "model/NetworkGraph.h"
#include "model/Packet.h"
#include "pbl/dsa/DynamicArray.h"

using namespace std;

namespace pbl {
namespace services {

// Milestone 5: Core logic — Queues and fixed-tick simulation
// SimulationService implements the 1 ms discrete step clock,
// priority-based link queue scheduling, bandwidth serialization delay,
// transmission latency, deterministic packet dropping upon node/link failure,
// and simulation control states (start, pause, resume, reset, tick).
class SimulationService {
public:
    enum class State {
        STOPPED,
        RUNNING,
        PAUSED
    };

    // Representation of a packet traveling across a physical link
    struct InFlightPacket {
        model::Packet packet;
        int from_node_id;
        int to_node_id;
        int dispatch_tick;
        int arrival_tick;
        int transmission_time;
        int latency;
        dsa::DynamicArray<int> route; // Optional multi-hop route
        int current_hop_index;

        InFlightPacket() {
            from_node_id = -1;
            to_node_id = -1;
            dispatch_tick = 0;
            arrival_tick = 0;
            transmission_time = 0;
            latency = 0;
            current_hop_index = 0;
        }

        InFlightPacket(const model::Packet& pkt, int from_id, int to_id,
                       int disp_tick, int arr_tick, int tx_time, int lat) {
            packet = pkt;
            from_node_id = from_id;
            to_node_id = to_id;
            dispatch_tick = disp_tick;
            arrival_tick = arr_tick;
            transmission_time = tx_time;
            latency = lat;
            current_hop_index = 0;
        }
    };

    // Track packet counts and failure events
    struct Statistics {
        int sent_count;
        int delivered_count;
        int dropped_count;
        int queue_overflow_count;
        int failure_drop_count;

        Statistics() {
            sent_count = 0;
            delivered_count = 0;
            dropped_count = 0;
            queue_overflow_count = 0;
            failure_drop_count = 0;
        }
    };

    SimulationService();
    // SimulationService does not manage dynamically allocated heap memory,
    // so no custom destructor is needed.

    void set_network_graph(model::NetworkGraph* graph);
    model::NetworkGraph* get_network_graph() const;

    // Simulation lifecycle controls
    void start();
    void pause();
    void resume();
    void reset();
    State get_state() const;
    bool is_running() const;

    // Fixed 1 ms tick progression
    int get_current_tick() const;
    void tick();
    void advance_ticks(int count);

    // Bandwidth and latency timing formulas:
    // transmission_time = ceiling(packet_size / bandwidth)
    // arrival_time = dispatch_tick + transmission_time + latency
    int calculate_transmission_time(int packet_size_bytes, int bandwidth_bytes_per_ms) const;

    // Packet sending operations
    bool send_packet(int from_node_id, int to_node_id, const model::Packet& packet);
    bool send_packet_along_route(const dsa::DynamicArray<int>& route, const model::Packet& packet);

    // Inspection queries
    const dsa::DynamicArray<InFlightPacket>& get_in_flight_packets() const;
    int get_in_flight_count() const;
    const dsa::DynamicArray<model::Packet>& get_delivered_packets() const;
    const Statistics& get_statistics() const;
    const dsa::DynamicArray<string>& get_event_log() const;

    // Failure event processing
    void process_node_failure(int node_id);
    void process_link_failure(int first_node_id, int second_node_id);

private:
    model::NetworkGraph* graph_;
    State state_;
    int current_tick_;
    Statistics statistics_;
    dsa::DynamicArray<InFlightPacket> in_flight_packets_;
    dsa::DynamicArray<model::Packet> delivered_packets_;
    dsa::DynamicArray<string> event_log_;

    void log_event(string message);
    void drop_in_flight_at(int index, string reason);
};

}
}
