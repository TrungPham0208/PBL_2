#include "model/Packet.h"

using namespace std;

namespace pbl {
namespace model {

// Milestone 2 & Milestone 6: Packet implementation
Packet::Packet() {
    type_ = Type::DATA;
    source_id_ = -1;
    destination_id_ = -1;
    sequence_number_ = -1;
    payload_ = "";
    payload_size_ = 0;
    checksum_ = 0;
}

Packet::Packet(Type type, int source_id, int destination_id, int sequence_number,
               string payload, int payload_size) {
    if (source_id < 0 || destination_id < 0) {
        throw invalid_argument("packet node ids must not be negative");
    }
    if (sequence_number < 0) {
        throw invalid_argument("packet sequence number must not be negative");
    }
    if (payload_size < 0) {
        throw invalid_argument("packet size must not be negative");
    }
    type_ = type;
    source_id_ = source_id;
    destination_id_ = destination_id;
    sequence_number_ = sequence_number;
    payload_ = payload;
    payload_size_ = payload_size;
    checksum_ = 0;
}

Packet::Packet(Type type, int source_id, int destination_id, int sequence_number,
               string payload, int payload_size, int checksum) {
    if (source_id < 0 || destination_id < 0) {
        throw invalid_argument("packet node ids must not be negative");
    }
    if (sequence_number < 0) {
        throw invalid_argument("packet sequence number must not be negative");
    }
    if (payload_size < 0) {
        throw invalid_argument("packet size must not be negative");
    }
    type_ = type;
    source_id_ = source_id;
    destination_id_ = destination_id;
    sequence_number_ = sequence_number;
    payload_ = payload;
    payload_size_ = payload_size;
    checksum_ = checksum;
}

Packet::Type Packet::get_type() const {
    return type_;
}

int Packet::get_source_id() const {
    return source_id_;
}

int Packet::get_destination_id() const {
    return destination_id_;
}

int Packet::get_sequence_number() const {
    return sequence_number_;
}

string Packet::get_payload() const {
    return payload_;
}

int Packet::get_payload_size() const {
    return payload_size_;
}

int Packet::get_checksum() const {
    return checksum_;
}

void Packet::set_payload(string payload) {
    payload_ = payload;
}

void Packet::set_payload_size(int payload_size) {
    if (payload_size < 0) {
        throw invalid_argument("packet size must not be negative");
    }
    payload_size_ = payload_size;
}

void Packet::set_checksum(int checksum) {
    checksum_ = checksum;
}

}
}
