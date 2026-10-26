#pragma once

#include <bits/stdc++.h>

#include "model/Packet.h"
#include "pbl/dsa/PriorityQueue.h"

using namespace std;

namespace pbl {
namespace model {

// Milestone 2 & Milestone 5: Link owns the shared state for both directions.
// Milestone 5: Bounded packet queue with priority scheduling (ACK & ROUTE_PROBE > HEARTBEAT & ROUTE_UPDATE > DATA)
// and FIFO ordering for equal-priority packets.
class Link {
public:
    Link(int first_node_id, int second_node_id, int bandwidth,
         int latency, double loss_probability, double corruption_probability,
         int queue_capacity);

    int get_first_node_id() const;
    int get_second_node_id() const;
    bool connects(int first_node_id, int second_node_id) const;
    bool is_active() const;
    bool is_failed() const;

    int get_bandwidth() const;
    int get_latency() const;
    double get_loss_probability() const;
    double get_corruption_probability() const;

    void set_active(bool active);
    void fail();
    void recover();
    void set_bandwidth(int bandwidth);
    void set_latency(int latency);
    void set_loss_probability(double probability);
    void set_corruption_probability(double probability);

    // Queue management with priority scheduling
    void enqueue(const Packet& packet);
    Packet dequeue();
    const Packet& front() const;
    bool queue_empty() const;
    bool queue_full() const;
    int queue_size() const;
    int queue_capacity() const;
    void clear_queue();

    // Priority mapping helper:
    // ACK and ROUTE_PROBE (3) > HEARTBEAT and ROUTE_UPDATE (2) > DATA (1)
    int get_packet_priority(const Packet& packet) const;

    // Transmission busy-state tracking for fixed-tick simulation
    int get_busy_until_tick() const;
    void set_busy_until_tick(int tick);
    bool is_busy(int current_tick) const;

private:
    int first_node_id_;
    int second_node_id_;
    int bandwidth_;
    int latency_;
    double loss_probability_;
    double corruption_probability_;
    bool active_;
    int queue_capacity_;
    int busy_until_tick_;
    dsa::PriorityQueue<Packet> packet_queue_;

    void validate_probability(double probability) const;
};

}
}
