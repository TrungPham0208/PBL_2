#pragma once

#include <bits/stdc++.h>

#include "model/Packet.h"
#include "pbl/dsa/Queue.h"

using namespace std;

namespace pbl {
namespace model {

// Milestone 2: one Link object owns the shared state for both directions.
// Its queue is bounded and uses the custom FIFO structure from Milestone 1.
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

    void enqueue(const Packet& packet);
    Packet dequeue();
    const Packet& front() const;
    bool queue_empty() const;
    bool queue_full() const;
    int queue_size() const;
    int queue_capacity() const;

private:
    int first_node_id_;
    int second_node_id_;
    int bandwidth_;
    int latency_;
    double loss_probability_;
    double corruption_probability_;
    bool active_;
    dsa::Queue<Packet> packet_queue_;

    void validate_probability(double probability) const;
};

}
}
