#include <bits/stdc++.h>

#include "model/Link.h"

using namespace std;

namespace pbl {
namespace model {

// Milestone 2 & Milestone 5: Link implementation
Link::Link(int first_node_id, int second_node_id, int bandwidth,
           int latency, double loss_probability, double corruption_probability,
           int queue_capacity) {
    if (first_node_id < 0 || second_node_id < 0) {
        throw invalid_argument("link node ids must not be negative");
    }
    if (first_node_id == second_node_id) {
        throw invalid_argument("link endpoints must be different");
    }
    if (bandwidth <= 0) {
        throw invalid_argument("link bandwidth must be positive");
    }
    if (latency < 0) {
        throw invalid_argument("link latency must not be negative");
    }
    if (queue_capacity <= 0) {
        throw invalid_argument("queue capacity must be positive");
    }
    validate_probability(loss_probability);
    validate_probability(corruption_probability);

    first_node_id_ = first_node_id;
    second_node_id_ = second_node_id;
    bandwidth_ = bandwidth;
    latency_ = latency;
    loss_probability_ = loss_probability;
    corruption_probability_ = corruption_probability;
    queue_capacity_ = queue_capacity;
    busy_until_tick_ = 0;
    active_ = true;
}

int Link::get_first_node_id() const { return first_node_id_; }
int Link::get_second_node_id() const { return second_node_id_; }

bool Link::connects(int first_node_id, int second_node_id) const {
    return (first_node_id_ == first_node_id && second_node_id_ == second_node_id)
        || (first_node_id_ == second_node_id && second_node_id_ == first_node_id);
}

bool Link::is_active() const { return active_; }
bool Link::is_failed() const { return !active_; }
int Link::get_bandwidth() const { return bandwidth_; }
int Link::get_latency() const { return latency_; }
double Link::get_loss_probability() const { return loss_probability_; }
double Link::get_corruption_probability() const { return corruption_probability_; }

void Link::set_active(bool active) {
    active_ = active;
    if (!active_) {
        // Invariant: Packets affected by a failed node or link are dropped rather than moved invisibly.
        clear_queue();
    }
}

void Link::fail() {
    set_active(false);
}

void Link::recover() {
    set_active(true);
}

void Link::set_bandwidth(int bandwidth) {
    if (bandwidth <= 0) throw invalid_argument("link bandwidth must be positive");
    bandwidth_ = bandwidth;
}

void Link::set_latency(int latency) {
    if (latency < 0) throw invalid_argument("link latency must not be negative");
    latency_ = latency;
}

void Link::set_loss_probability(double probability) {
    validate_probability(probability);
    loss_probability_ = probability;
}

void Link::set_corruption_probability(double probability) {
    validate_probability(probability);
    corruption_probability_ = probability;
}

// Milestone 5: Priority scheduling
// ACK and ROUTE_PROBE (3) > HEARTBEAT and ROUTE_UPDATE (2) > DATA (1)
int Link::get_packet_priority(const Packet& packet) const {
    Packet::Type type = packet.get_type();
    if (type == Packet::Type::ACK || type == Packet::Type::ROUTE_PROBE) {
        return 3;
    }
    if (type == Packet::Type::HEARTBEAT || type == Packet::Type::ROUTE_UPDATE) {
        return 2;
    }
    return 1; // DATA
}

void Link::enqueue(const Packet& packet) {
    if (queue_full()) {
        throw overflow_error("link queue is full");
    }
    int priority = get_packet_priority(packet);
    packet_queue_.push(packet, priority);
}

Packet Link::dequeue() {
    if (queue_empty()) {
        throw out_of_range("cannot remove from an empty queue");
    }
    return packet_queue_.pop();
}

const Packet& Link::front() const {
    if (queue_empty()) {
        throw out_of_range("empty link queue has no front");
    }
    return packet_queue_.top();
}

bool Link::queue_empty() const { return packet_queue_.empty(); }
bool Link::queue_full() const { return packet_queue_.size() >= queue_capacity_; }
int Link::queue_size() const { return packet_queue_.size(); }
int Link::queue_capacity() const { return queue_capacity_; }

void Link::clear_queue() {
    packet_queue_.clear();
}

int Link::get_busy_until_tick() const { return busy_until_tick_; }
void Link::set_busy_until_tick(int tick) { busy_until_tick_ = tick; }
bool Link::is_busy(int current_tick) const { return current_tick < busy_until_tick_; }

void Link::validate_probability(double probability) const {
    if (probability < 0.0 || probability > 1.0) {
        throw invalid_argument("probability must be between zero and one");
    }
}

}
}
