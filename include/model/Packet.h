#pragma once

#include <bits/stdc++.h>

using namespace std;

namespace pbl {
namespace model {

// Milestone 2 & Milestone 6: explicit packet metadata and payload checksum integrity
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
    Packet(Type type, int source_id, int destination_id, int sequence_number,
           string payload, int payload_size, int checksum);

    Type get_type() const;
    int get_source_id() const;
    int get_destination_id() const;
    int get_sequence_number() const;
    string get_payload() const;
    int get_payload_size() const;
    int get_checksum() const;

    void set_payload(string payload);
    void set_payload_size(int payload_size);
    void set_checksum(int checksum);

private:
    Type type_;
    int source_id_;
    int destination_id_;
    int sequence_number_;
    string payload_;
    int payload_size_;
    int checksum_;
};

}
}
