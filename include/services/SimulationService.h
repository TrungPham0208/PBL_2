#pragma once

#include <bits/stdc++.h>

#include "model/Link.h"
#include "model/NetworkGraph.h"
#include "model/Packet.h"
#include "pbl/dsa/DynamicArray.h"
#include "pbl/dsa/HashMap.h"

using namespace std;

namespace pbl {
namespace services {

class RoutingService;

// Milestone 5, 8 & 9: Fixed-tick simulation, link queues, failure detection, and automatic rerouting
// SimulationService implements the 1 ms discrete step clock,
// priority-based link queue scheduling, bandwidth serialization delay,
// transmission latency, deterministic packet dropping upon node/link failure,
// deterministic manual loss/corruption injection, configured random loss/corruption,
// manual failure handling, and heartbeat-based router failure detection.
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
        int lost_count;
        int corrupted_count;
        int manual_failure_count;
        int heartbeat_timeout_count;

        Statistics() {
            sent_count = 0;
            delivered_count = 0;
            dropped_count = 0;
            queue_overflow_count = 0;
            failure_drop_count = 0;
            lost_count = 0;
            corrupted_count = 0;
            manual_failure_count = 0;
            heartbeat_timeout_count = 0;
        }
    };

    static const int DEFAULT_HEARTBEAT_INTERVAL = 100; // 100 ms per SPEC.md
    static const int DEFAULT_FAILURE_TIMEOUT = 300;    // 300 ms per SPEC.md

    SimulationService();
    // SimulationService does not manage dynamically allocated heap memory,
    // so no custom destructor is needed.

    void set_network_graph(model::NetworkGraph* graph);
    model::NetworkGraph* get_network_graph() const;

    // Milestone 9: Routing service attachment for automatic hop-by-hop forwarding and rerouting
    void set_routing_service(RoutingService* routing);
    RoutingService* get_routing_service() const;

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

    // Milestone 8: Random seed for deterministic simulation experiments
    void set_random_seed(unsigned int seed);
    double generate_random_probability();

    // Milestone 8: Manual packet loss and corruption injection
    void inject_link_loss(int from_node_id, int to_node_id);
    void inject_link_corruption(int from_node_id, int to_node_id);

    // Milestone 8: Manual node and link failures (takes effect immediately)
    void fail_node_manually(int node_id);
    void fail_link_manually(int first_node_id, int second_node_id);

    // Milestone 8: Heartbeat failure detection
    void set_heartbeat_enabled(bool enabled);
    bool is_heartbeat_enabled() const;
    void set_heartbeat_interval(int interval_ms);
    int get_heartbeat_interval() const;
    void set_heartbeat_timeout(int timeout_ms);
    int get_heartbeat_timeout() const;

    void record_heartbeat(int node_id, int tick);
    int get_last_heartbeat_tick(int node_id) const;
    void silence_node_heartbeat(int node_id);
    bool is_node_heartbeat_silenced(int node_id) const;
    void resume_node_heartbeat(int node_id);

    // Internal heartbeat step called during tick()
    void process_heartbeats();

private:
    model::NetworkGraph* graph_;
    RoutingService* routing_service_;
    State state_;
    int current_tick_;
    Statistics statistics_;
    dsa::DynamicArray<InFlightPacket> in_flight_packets_;
    dsa::DynamicArray<model::Packet> delivered_packets_;
    dsa::DynamicArray<string> event_log_;

    // Milestone 8 state
    mt19937 rng_;
    bool heartbeat_enabled_;
    int heartbeat_interval_;
    int heartbeat_timeout_;
    dsa::HashMap<int, int> last_heartbeat_tick_;
    dsa::HashMap<int, bool> silenced_heartbeats_;

    void log_event(string message);
    void drop_in_flight_at(int index, string reason);
};

}
}
