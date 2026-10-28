#include <bits/stdc++.h>

#include "services/RoutingService.h"
#include "services/SimulationService.h"

using namespace std;

namespace pbl {
namespace services {

// Milestone 5, 8 & 9: Core logic — Queues, fixed-tick simulation, loss, failure detection, and rerouting
// Constructor initializes simulator fields in body to keep syntax beginner-friendly.
SimulationService::SimulationService() {
    graph_ = nullptr;
    routing_service_ = nullptr;
    state_ = State::STOPPED;
    current_tick_ = 0;
    heartbeat_enabled_ = false;
    heartbeat_interval_ = DEFAULT_HEARTBEAT_INTERVAL;
    heartbeat_timeout_ = DEFAULT_FAILURE_TIMEOUT;
    rng_.seed(12345);
}

void SimulationService::set_network_graph(model::NetworkGraph* graph) {
    graph_ = graph;
}

model::NetworkGraph* SimulationService::get_network_graph() const {
    return graph_;
}

// Milestone 9: Routing service attachment
void SimulationService::set_routing_service(RoutingService* routing) {
    routing_service_ = routing;
}

RoutingService* SimulationService::get_routing_service() const {
    return routing_service_;
}

void SimulationService::start() {
    state_ = State::RUNNING;
    if (graph_ != nullptr) {
        dsa::DynamicArray<int> router_ids = graph_->get_router_ids();
        for (int i = 0; i < router_ids.size(); ++i) {
            int id = router_ids[i];
            if (!last_heartbeat_tick_.contains(id)) {
                last_heartbeat_tick_.put(id, current_tick_);
            }
        }
    }
    log_event("[SIMULATION] Started at tick " + to_string(current_tick_));
}

void SimulationService::pause() {
    if (state_ == State::RUNNING) {
        state_ = State::PAUSED;
        log_event("[SIMULATION] Paused at tick " + to_string(current_tick_));
    }
}

void SimulationService::resume() {
    if (state_ == State::PAUSED) {
        state_ = State::RUNNING;
        log_event("[SIMULATION] Resumed at tick " + to_string(current_tick_));
    }
}

void SimulationService::reset() {
    state_ = State::STOPPED;
    current_tick_ = 0;
    statistics_ = Statistics();
    in_flight_packets_.clear();
    delivered_packets_.clear();
    event_log_.clear();
    last_heartbeat_tick_.clear();
    silenced_heartbeats_.clear();
    rng_.seed(12345);

    if (graph_ != nullptr) {
        const dsa::DynamicArray<model::Link*>& links = graph_->get_all_links();
        for (int i = 0; i < links.size(); ++i) {
            links[i]->clear_queue();
            links[i]->set_busy_until_tick(0);
            links[i]->clear_injections();
        }
    }
    log_event("[SIMULATION] Reset to tick 0");
}

SimulationService::State SimulationService::get_state() const {
    return state_;
}

bool SimulationService::is_running() const {
    return state_ == State::RUNNING;
}

int SimulationService::get_current_tick() const {
    return current_tick_;
}

// Bandwidth serialization delay formula:
// transmission_time = ceiling(packet_size / bandwidth)
int SimulationService::calculate_transmission_time(int packet_size_bytes, int bandwidth_bytes_per_ms) const {
    if (bandwidth_bytes_per_ms <= 0) {
        bandwidth_bytes_per_ms = 1;
    }
    if (packet_size_bytes <= 0) {
        return 0;
    }
    // Integer ceiling arithmetic: (S + B - 1) / B
    return (packet_size_bytes + bandwidth_bytes_per_ms - 1) / bandwidth_bytes_per_ms;
}

// Sends a packet directly across a single physical link
bool SimulationService::send_packet(int from_node_id, int to_node_id, const model::Packet& packet) {
    if (graph_ == nullptr) {
        return false;
    }

    // Invariant: Failed nodes cannot send or receive packets
    if (!graph_->has_node(from_node_id) || !graph_->is_node_active(from_node_id) ||
        !graph_->has_node(to_node_id) || !graph_->is_node_active(to_node_id)) {
        statistics_.dropped_count++;
        statistics_.failure_drop_count++;
        log_event("[DROP] Packet dropped: inactive endpoint node (" +
                  to_string(from_node_id) + " -> " + to_string(to_node_id) + ")");
        return false;
    }

    // Invariant: Failed links cannot accept packets
    if (!graph_->has_link(from_node_id, to_node_id) || !graph_->is_link_active(from_node_id, to_node_id)) {
        statistics_.dropped_count++;
        statistics_.failure_drop_count++;
        log_event("[DROP] Packet dropped: inactive or missing link (" +
                  to_string(from_node_id) + " -> " + to_string(to_node_id) + ")");
        return false;
    }

    model::Link* link = graph_->get_link(from_node_id, to_node_id);
    if (link == nullptr) {
        return false;
    }

    // Invariant: Queues never exceed their configured bound
    if (link->queue_full()) {
        statistics_.dropped_count++;
        statistics_.queue_overflow_count++;
        log_event("[DROP] Packet dropped: link queue overflow on (" +
                  to_string(from_node_id) + " -> " + to_string(to_node_id) + ")");
        return false;
    }

    // Enqueue with priority scheduling
    link->enqueue(packet);
    statistics_.sent_count++;
    log_event("[ENQUEUE] Packet (seq=" + to_string(packet.get_sequence_number()) +
              ") queued on link (" + to_string(from_node_id) + " -> " + to_string(to_node_id) + ")");
    return true;
}

// Sends a packet along an explicit multi-hop route
bool SimulationService::send_packet_along_route(const dsa::DynamicArray<int>& route, const model::Packet& packet) {
    if (route.size() < 2) {
        return false;
    }

    int first_hop = route[0];
    int second_hop = route[1];

    return send_packet(first_hop, second_hop, packet);
}

// How one tick changes queue and packet state deterministically:
// 1. Advance the simulation clock by 1 ms.
// 2. Failure check: Purge any in-flight packets traveling over failed links or toward failed nodes.
// 3. In-flight arrival: Packets whose arrival_tick <= current_tick_ reach their target node.
// 4. Link queue dispatch: Each active, non-busy link dequeues its highest-priority packet,
//    computes serialization transmission time and latency, and launches it into flight.
void SimulationService::tick() {
    if (graph_ == nullptr) {
        current_tick_++;
        return;
    }

    // 1. Advance the simulation clock by 1 ms
    current_tick_++;

    // Milestone 8: Process heartbeats and failure timeouts
    process_heartbeats();

    // 2. Check for failure-affected in-flight packets and drop them
    for (int i = 0; i < in_flight_packets_.size(); ++i) {
        int u = in_flight_packets_[i].from_node_id;
        int v = in_flight_packets_[i].to_node_id;

        bool endpoint_failed = !graph_->is_node_active(u) || !graph_->is_node_active(v);
        bool link_failed = !graph_->is_link_active(u, v);

        if (endpoint_failed || link_failed) {
            drop_in_flight_at(i, "node or link failure during transit");
            --i; // Re-evaluate index after removal
        }
    }

    // 3. Process in-flight packet arrivals at the current tick
    for (int i = 0; i < in_flight_packets_.size(); ++i) {
        if (current_tick_ >= in_flight_packets_[i].arrival_tick) {
            InFlightPacket arrived = in_flight_packets_[i];

            // Remove from in_flight_packets_
            in_flight_packets_[i] = in_flight_packets_[in_flight_packets_.size() - 1];
            in_flight_packets_.pop_back();
            --i;

            // Invariant: check if destination node is active upon arrival
            if (!graph_->has_node(arrived.to_node_id) || !graph_->is_node_active(arrived.to_node_id)) {
                statistics_.dropped_count++;
                statistics_.failure_drop_count++;
                log_event("[DROP] Packet dropped upon arrival: destination node " +
                          to_string(arrived.to_node_id) + " is inactive");
                continue;
            }

            // Check whether packet reached its final destination or requires forwarding
            if (arrived.to_node_id == arrived.packet.get_destination_id() || routing_service_ == nullptr) {
                // Packet successfully reached its target for this hop / final destination
                delivered_packets_.push_back(arrived.packet);
                statistics_.delivered_count++;
                log_event("[DELIVERED] Tick " + to_string(current_tick_) + ": Packet (seq=" +
                          to_string(arrived.packet.get_sequence_number()) + ") arrived at node " +
                          to_string(arrived.to_node_id));
            } else {
                // Milestone 9: Packet reached an intermediate router node: forward along current best route
                bool forwarded = false;
                RoutingService::Route next_route = routing_service_->find_route(*graph_, arrived.to_node_id, arrived.packet.get_destination_id());
                if (next_route.found && next_route.path.size() >= 2) {
                    int next_hop = next_route.path[1];
                    forwarded = send_packet(arrived.to_node_id, next_hop, arrived.packet);
                    if (forwarded) {
                        log_event("[FORWARD] Tick " + to_string(current_tick_) + ": Router " +
                                  to_string(arrived.to_node_id) + " forwarded packet (seq=" +
                                  to_string(arrived.packet.get_sequence_number()) + ") to next hop " +
                                  to_string(next_hop));
                    }
                }

                if (!forwarded) {
                    statistics_.dropped_count++;
                    statistics_.failure_drop_count++;
                    log_event("[DROP] Packet (seq=" + to_string(arrived.packet.get_sequence_number()) +
                              ") dropped at intermediate router " + to_string(arrived.to_node_id) +
                              ": no usable route to destination " +
                              to_string(arrived.packet.get_destination_id()));
                }
            }
        }
    }

    // 4. Link queue dispatch for the current tick
    const dsa::DynamicArray<model::Link*>& links = graph_->get_all_links();
    for (int i = 0; i < links.size(); ++i) {
        model::Link* link = links[i];
        if (!link->is_active()) {
            continue;
        }

        // If link is currently busy serializing a previous packet, it cannot dispatch yet
        if (link->is_busy(current_tick_)) {
            continue;
        }

        if (link->queue_empty()) {
            continue;
        }

        // Dequeue packet (PriorityQueue extracts highest priority first, FIFO for ties)
        model::Packet packet = link->dequeue();

        int from_id = packet.get_source_id();
        int to_id = packet.get_destination_id();

        // Determine hop direction along undirected link
        if (link->get_second_node_id() == packet.get_destination_id()) {
            from_id = link->get_first_node_id();
            to_id = link->get_second_node_id();
        } else if (link->get_first_node_id() == packet.get_destination_id()) {
            from_id = link->get_second_node_id();
            to_id = link->get_first_node_id();
        } else if (routing_service_ != nullptr) {
            RoutingService::Route r_from_first = routing_service_->find_route(*graph_, link->get_first_node_id(), packet.get_destination_id());
            if (r_from_first.found && r_from_first.get_next_hop() == link->get_second_node_id()) {
                from_id = link->get_first_node_id();
                to_id = link->get_second_node_id();
            } else {
                from_id = link->get_second_node_id();
                to_id = link->get_first_node_id();
            }
        } else {
            from_id = link->get_first_node_id();
            to_id = link->get_second_node_id();
        }

        int packet_size = packet.get_payload_size();
        int bandwidth = link->get_bandwidth();
        int latency = link->get_latency();

        int tx_time = calculate_transmission_time(packet_size, bandwidth);
        // Link stays busy serializing this packet on the wire
        link->set_busy_until_tick(current_tick_ + tx_time);

        // Milestone 8: Check for packet loss (manual injection or configured random probability)
        bool is_lost = false;
        if (link->should_inject_loss()) {
            is_lost = true;
        } else if (link->get_loss_probability() > 0.0) {
            double prob = generate_random_probability();
            if (prob < link->get_loss_probability()) {
                is_lost = true;
            }
        }

        if (is_lost) {
            statistics_.dropped_count++;
            statistics_.lost_count++;
            log_event("[PACKET_LOSS] Tick " + to_string(current_tick_) + ": Packet (seq=" +
                      to_string(packet.get_sequence_number()) + ") lost on link (" +
                      to_string(from_id) + " -> " + to_string(to_id) + ")");
            continue; // Packet dropped, does NOT enter in-flight transit
        }

        // Milestone 8: Check for packet corruption (manual injection or configured random probability)
        bool is_corrupted = false;
        if (link->should_inject_corruption()) {
            is_corrupted = true;
        } else if (link->get_corruption_probability() > 0.0) {
            double prob = generate_random_probability();
            if (prob < link->get_corruption_probability()) {
                is_corrupted = true;
            }
        }

        if (is_corrupted) {
            // Corrupt packet payload (flip bits of first byte)
            string corrupted_payload = packet.get_payload();
            if (!corrupted_payload.empty()) {
                corrupted_payload[0] ^= 0xFF;
            } else {
                corrupted_payload = "CORRUPTED";
            }
            packet.set_payload(corrupted_payload);
            statistics_.corrupted_count++;
            log_event("[PACKET_CORRUPTION] Tick " + to_string(current_tick_) + ": Packet (seq=" +
                      to_string(packet.get_sequence_number()) + ") corrupted on link (" +
                      to_string(from_id) + " -> " + to_string(to_id) + ")");
        }

        int arrival_tick = current_tick_ + tx_time + latency;

        InFlightPacket in_flight(packet, from_id, to_id, current_tick_, arrival_tick, tx_time, latency);
        in_flight_packets_.push_back(in_flight);

        log_event("[DISPATCH] Tick " + to_string(current_tick_) + ": Packet (seq=" +
                  to_string(packet.get_sequence_number()) + ") dispatched on link (" +
                  to_string(from_id) + " -> " + to_string(to_id) + "), tx_time=" +
                  to_string(tx_time) + "ms, lat=" + to_string(latency) +
                  "ms, arrival_tick=" + to_string(arrival_tick));
    }
}

void SimulationService::advance_ticks(int count) {
    for (int i = 0; i < count; ++i) {
        tick();
    }
}

const dsa::DynamicArray<SimulationService::InFlightPacket>& SimulationService::get_in_flight_packets() const {
    return in_flight_packets_;
}

int SimulationService::get_in_flight_count() const {
    return in_flight_packets_.size();
}

const dsa::DynamicArray<model::Packet>& SimulationService::get_delivered_packets() const {
    return delivered_packets_;
}

const SimulationService::Statistics& SimulationService::get_statistics() const {
    return statistics_;
}

const dsa::DynamicArray<string>& SimulationService::get_event_log() const {
    return event_log_;
}

// Invariant: Packets affected by a failed node or link are dropped rather than moved invisibly.
void SimulationService::process_node_failure(int node_id) {
    if (graph_ != nullptr) {
        graph_->fail_node(node_id);
    }

    // Purge in-flight packets connected to this node
    for (int i = 0; i < in_flight_packets_.size(); ++i) {
        if (in_flight_packets_[i].from_node_id == node_id || in_flight_packets_[i].to_node_id == node_id) {
            drop_in_flight_at(i, "node " + to_string(node_id) + " failed");
            --i;
        }
    }

    // Purge queued packets on links connected to this node
    if (graph_ != nullptr) {
        const dsa::DynamicArray<model::Link*>& links = graph_->get_all_links();
        for (int i = 0; i < links.size(); ++i) {
            if (links[i]->get_first_node_id() == node_id || links[i]->get_second_node_id() == node_id) {
                int dropped_count = links[i]->queue_size();
                links[i]->clear_queue();
                statistics_.dropped_count += dropped_count;
                statistics_.failure_drop_count += dropped_count;
            }
        }
    }

    log_event("[FAILURE] Node " + to_string(node_id) + " failed. Affected packets purged.");
}

void SimulationService::process_link_failure(int first_node_id, int second_node_id) {
    if (graph_ != nullptr) {
        graph_->fail_link(first_node_id, second_node_id);
    }

    // Purge in-flight packets traveling over this link
    for (int i = 0; i < in_flight_packets_.size(); ++i) {
        int u = in_flight_packets_[i].from_node_id;
        int v = in_flight_packets_[i].to_node_id;
        if ((u == first_node_id && v == second_node_id) || (u == second_node_id && v == first_node_id)) {
            drop_in_flight_at(i, "link (" + to_string(first_node_id) + " - " + to_string(second_node_id) + ") failed");
            --i;
        }
    }

    // Purge queue of this link
    if (graph_ != nullptr) {
        model::Link* link = graph_->get_link(first_node_id, second_node_id);
        if (link != nullptr) {
            int dropped_count = link->queue_size();
            link->clear_queue();
            statistics_.dropped_count += dropped_count;
            statistics_.failure_drop_count += dropped_count;
        }
    }

    log_event("[FAILURE] Link (" + to_string(first_node_id) + " - " +
              to_string(second_node_id) + ") failed. Affected packets purged.");
}

void SimulationService::drop_in_flight_at(int index, string reason) {
    if (index >= 0 && index < in_flight_packets_.size()) {
        model::Packet pkt = in_flight_packets_[index].packet;
        in_flight_packets_[index] = in_flight_packets_[in_flight_packets_.size() - 1];
        in_flight_packets_.pop_back();

        statistics_.dropped_count++;
        statistics_.failure_drop_count++;
        log_event("[DROP] In-flight packet (seq=" + to_string(pkt.get_sequence_number()) +
                  ") dropped: " + reason);
    }
}

void SimulationService::log_event(string message) {
    event_log_.push_back(message);
}

// Milestone 8: Random seed for deterministic simulation experiments
void SimulationService::set_random_seed(unsigned int seed) {
    rng_.seed(seed);
}

double SimulationService::generate_random_probability() {
    uniform_real_distribution<double> dist(0.0, 1.0);
    return dist(rng_);
}

// Milestone 8: Manual packet loss and corruption injection
void SimulationService::inject_link_loss(int from_node_id, int to_node_id) {
    if (graph_ != nullptr) {
        model::Link* link = graph_->get_link(from_node_id, to_node_id);
        if (link != nullptr) {
            link->inject_loss_next();
        }
    }
}

void SimulationService::inject_link_corruption(int from_node_id, int to_node_id) {
    if (graph_ != nullptr) {
        model::Link* link = graph_->get_link(from_node_id, to_node_id);
        if (link != nullptr) {
            link->inject_corruption_next();
        }
    }
}

// Milestone 8: Manual node and link failures (takes effect immediately)
void SimulationService::fail_node_manually(int node_id) {
    log_event("[MANUAL_FAILURE] Tick " + to_string(current_tick_) +
              ": Node " + to_string(node_id) + " failed manually");
    statistics_.manual_failure_count++;
    process_node_failure(node_id);
}

void SimulationService::fail_link_manually(int first_node_id, int second_node_id) {
    log_event("[MANUAL_FAILURE] Tick " + to_string(current_tick_) +
              ": Link (" + to_string(first_node_id) + " - " + to_string(second_node_id) +
              ") failed manually");
    statistics_.manual_failure_count++;
    process_link_failure(first_node_id, second_node_id);
}

// Milestone 8: Heartbeat failure detection
void SimulationService::set_heartbeat_enabled(bool enabled) {
    heartbeat_enabled_ = enabled;
    if (enabled && graph_ != nullptr) {
        dsa::DynamicArray<int> router_ids = graph_->get_router_ids();
        for (int i = 0; i < router_ids.size(); ++i) {
            int id = router_ids[i];
            if (!last_heartbeat_tick_.contains(id)) {
                last_heartbeat_tick_.put(id, current_tick_);
            }
        }
    }
}

bool SimulationService::is_heartbeat_enabled() const {
    return heartbeat_enabled_;
}

void SimulationService::set_heartbeat_interval(int interval_ms) {
    if (interval_ms <= 0) {
        throw invalid_argument("heartbeat interval must be positive");
    }
    heartbeat_interval_ = interval_ms;
}

int SimulationService::get_heartbeat_interval() const {
    return heartbeat_interval_;
}

void SimulationService::set_heartbeat_timeout(int timeout_ms) {
    if (timeout_ms <= 0) {
        throw invalid_argument("heartbeat timeout must be positive");
    }
    heartbeat_timeout_ = timeout_ms;
}

int SimulationService::get_heartbeat_timeout() const {
    return heartbeat_timeout_;
}

void SimulationService::record_heartbeat(int node_id, int tick) {
    last_heartbeat_tick_.put(node_id, tick);
}

int SimulationService::get_last_heartbeat_tick(int node_id) const {
    if (last_heartbeat_tick_.contains(node_id)) {
        return last_heartbeat_tick_.at(node_id);
    }
    return 0;
}

void SimulationService::silence_node_heartbeat(int node_id) {
    silenced_heartbeats_.put(node_id, true);
}

bool SimulationService::is_node_heartbeat_silenced(int node_id) const {
    if (silenced_heartbeats_.contains(node_id)) {
        return silenced_heartbeats_.at(node_id);
    }
    return false;
}

void SimulationService::resume_node_heartbeat(int node_id) {
    if (silenced_heartbeats_.contains(node_id)) {
        silenced_heartbeats_.put(node_id, false);
    }
    record_heartbeat(node_id, current_tick_);
}

// Internal heartbeat step called during tick()
// Invariant: Heartbeat failure changes node/link availability only after the documented timeout,
// unless a manual failure is requested.
void SimulationService::process_heartbeats() {
    if (!heartbeat_enabled_ || graph_ == nullptr) {
        return;
    }

    dsa::DynamicArray<int> router_ids = graph_->get_router_ids();

    // 1. Periodic heartbeat emission
    if (heartbeat_interval_ > 0 && (current_tick_ % heartbeat_interval_ == 0)) {
        for (int i = 0; i < router_ids.size(); ++i) {
            int node_id = router_ids[i];
            if (graph_->is_node_active(node_id)) {
                if (!is_node_heartbeat_silenced(node_id)) {
                    record_heartbeat(node_id, current_tick_);
                    log_event("[HEARTBEAT] Tick " + to_string(current_tick_) +
                              ": Router " + to_string(node_id) + " emitted heartbeat");
                }
            }
        }
    }

    // 2. Heartbeat timeout detection
    for (int i = 0; i < router_ids.size(); ++i) {
        int node_id = router_ids[i];
        if (graph_->is_node_active(node_id)) {
            int last_tick = get_last_heartbeat_tick(node_id);
            if (current_tick_ - last_tick >= heartbeat_timeout_) {
                // Heartbeat failure detected after documented timeout
                log_event("[HEARTBEAT_TIMEOUT] Tick " + to_string(current_tick_) +
                          ": Router " + to_string(node_id) + " timed out after " +
                          to_string(heartbeat_timeout_) + "ms without heartbeat");
                statistics_.heartbeat_timeout_count++;
                process_node_failure(node_id);
            }
        }
    }
}

}
}
