#include <bits/stdc++.h>

#include "services/SimulationService.h"

using namespace std;

namespace pbl {
namespace services {

// Milestone 5: Core logic — Queues and fixed-tick simulation
// Constructor initializes simulator fields in body to keep syntax beginner-friendly.
SimulationService::SimulationService() {
    graph_ = nullptr;
    state_ = State::STOPPED;
    current_tick_ = 0;
}

void SimulationService::set_network_graph(model::NetworkGraph* graph) {
    graph_ = graph;
}

model::NetworkGraph* SimulationService::get_network_graph() const {
    return graph_;
}

void SimulationService::start() {
    state_ = State::RUNNING;
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

    if (graph_ != nullptr) {
        const dsa::DynamicArray<model::Link*>& links = graph_->get_all_links();
        for (int i = 0; i < links.size(); ++i) {
            links[i]->clear_queue();
            links[i]->set_busy_until_tick(0);
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

            // Packet successfully reached its target for this hop
            delivered_packets_.push_back(arrived.packet);
            statistics_.delivered_count++;
            log_event("[DELIVERED] Tick " + to_string(current_tick_) + ": Packet (seq=" +
                      to_string(arrived.packet.get_sequence_number()) + ") arrived at node " +
                      to_string(arrived.to_node_id));
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

        // If endpoints are not directly equal to link endpoints (e.g. multi-hop forwarding),
        // determine the immediate hop direction based on link connects
        if (!link->connects(from_id, to_id)) {
            from_id = link->get_first_node_id();
            to_id = link->get_second_node_id();
        }

        int packet_size = packet.get_payload_size();
        int bandwidth = link->get_bandwidth();
        int latency = link->get_latency();

        int tx_time = calculate_transmission_time(packet_size, bandwidth);
        // Link stays busy serializing this packet on the wire
        link->set_busy_until_tick(current_tick_ + tx_time);

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

}
}
