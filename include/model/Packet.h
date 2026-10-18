#pragma once

#include <bits/stdc++.h>

using namespace std;

namespace pbl {
namespace model {

// Milestone 2: explicit packet metadata shared by future routing and transfer code.
class Packet {
public:
    enum class Type {
        DATA,
        ACK,
        ROUTE_PROBE,
        HEARTBEAT,
        ROUTE_UPDATE
    };

    Packet();
    Packet(Type type, int source_id, int destination_id, int sequence_number,
           string payload, int payload_size);

    Type get_type() const;
    int get_source_id() const;
    int get_destination_id() const;
    int get_sequence_number() const;
    string get_payload() const;
    int get_payload_size() const;

    void set_payload(string payload);
    void set_payload_size(int payload_size);

private:
    Type type_;
    int source_id_;
    int destination_id_;
    int sequence_number_;
    string payload_;
    int payload_size_;
};

}
}
