#include <bits/stdc++.h>

#include "model/Client.h"
#include "model/Link.h"
#include "model/Packet.h"
#include "model/Router.h"

using namespace std;

namespace {

void test_nodes() {
    pbl::model::Client client(1, "client-a");
    pbl::model::Router router(2, "router-a");
    assert(client.get_id() == 1);
    assert(client.get_name() == "client-a");
    assert(client.get_type() == "Client");
    assert(router.get_type() == "Router");
    assert(client.is_active());
    client.fail();
    assert(client.is_failed());
    client.recover();
    assert(client.is_active());
}

void test_packet_metadata() {
    pbl::model::Packet packet(pbl::model::Packet::Type::DATA, 1, 2, 7, "abc", 3);
    assert(packet.get_type() == pbl::model::Packet::Type::DATA);
    assert(packet.get_source_id() == 1);
    assert(packet.get_destination_id() == 2);
    assert(packet.get_sequence_number() == 7);
    assert(packet.get_payload() == "abc");
    assert(packet.get_payload_size() == 3);
    packet.set_payload("abcd");
    packet.set_payload_size(4);
    assert(packet.get_payload() == "abcd");
    assert(packet.get_payload_size() == 4);
}

void test_link_configuration_and_shared_state() {
    pbl::model::Link link(1, 2, 100, 5, 0.1, 0.2, 2);
    assert(link.connects(1, 2));
    assert(link.connects(2, 1));
    assert(link.get_bandwidth() == 100);
    assert(link.get_latency() == 5);
    assert(link.get_loss_probability() == 0.1);
    assert(link.get_corruption_probability() == 0.2);
    assert(link.is_active());
    link.fail();
    assert(link.is_failed());
    link.recover();
    assert(link.is_active());

    pbl::model::Packet first(pbl::model::Packet::Type::ACK, 2, 1, 0, "", 0);
    pbl::model::Packet second(pbl::model::Packet::Type::DATA, 1, 2, 1, "x", 1);
    link.enqueue(first);
    link.enqueue(second);
    assert(link.queue_full());
    assert(link.dequeue().get_type() == pbl::model::Packet::Type::ACK);
    assert(link.dequeue().get_type() == pbl::model::Packet::Type::DATA);
    assert(link.queue_empty());
}

void test_invalid_model_values() {
    bool rejected = false;
    try {
        pbl::model::Link invalid(1, 2, 100, 5, 1.1, 0.0, 2);
    } catch (const invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
}

}

int main() {
    test_nodes();
    test_packet_metadata();
    test_link_configuration_and_shared_state();
    test_invalid_model_values();
    return 0;
}
