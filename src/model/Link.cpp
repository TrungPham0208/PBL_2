#include "model/Link.h"

using namespace std;

namespace pbl {
namespace model {

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
    validate_probability(loss_probability);
    validate_probability(corruption_probability);
    packet_queue_ = dsa::Queue<Packet>(queue_capacity);
    first_node_id_ = first_node_id;
    second_node_id_ = second_node_id;
    bandwidth_ = bandwidth;
    latency_ = latency;
    loss_probability_ = loss_probability;
    corruption_probability_ = corruption_probability;
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

void Link::set_active(bool active) { active_ = active; }
void Link::fail() { active_ = false; }
void Link::recover() { active_ = true; }

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

void Link::enqueue(const Packet& packet) { packet_queue_.push(packet); }
Packet Link::dequeue() { return packet_queue_.pop(); }
const Packet& Link::front() const { return packet_queue_.front(); }
bool Link::queue_empty() const { return packet_queue_.empty(); }
bool Link::queue_full() const { return packet_queue_.full(); }
int Link::queue_size() const { return packet_queue_.size(); }
int Link::queue_capacity() const { return packet_queue_.capacity(); }

void Link::validate_probability(double probability) const {
    if (probability < 0.0 || probability > 1.0) {
        throw invalid_argument("probability must be between zero and one");
    }
}

}
}
