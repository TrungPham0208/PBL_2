#pragma once

#include <bits/stdc++.h>

#include "model/Packet.h"
#include "pbl/dsa/DynamicArray.h"
#include "pbl/dsa/HashMap.h"

using namespace std;

namespace pbl {
namespace services {

// Milestone 6: Core logic — Chunking, checksums, and reconstruction
// TransferService manages splitting files into discrete chunks, sequence numbering,
// educational XOR integrity checksums, out-of-order chunk storage, and file reconstruction.
class TransferService {
public:
    // Representation of a discrete file chunk with stable sequence metadata
    struct Chunk {
        int sequence_number;
        string data;
        int size;
        int checksum;

        Chunk() {
            sequence_number = -1;
            data = "";
            size = 0;
            checksum = 0;
        }

        Chunk(int seq, string d, int sz, int chk) {
            sequence_number = seq;
            data = d;
            size = sz;
            checksum = chk;
        }
    };

    TransferService();
    // TransferService does not own dynamically allocated heap memory,
    // so no custom destructor is required.

    // Educational XOR checksum calculation over byte payload.
    // Invariant / Security note:
    // XOR is strictly an educational integrity check for detecting random corruption.
    // It is NOT a cryptographic hash and provides NO cryptographic protection.
    // (CRC-32/IEEE is documented in SPEC.md as a stronger non-cryptographic alternative,
    // but is intentionally not implemented to maintain pedagogical focus on DSA/OOP).
    int calculate_xor_checksum(const string& data) const;
    bool verify_checksum(const string& data, int expected_checksum) const;

    // File chunking
    dsa::DynamicArray<Chunk> split_into_chunks(const string& file_content, int chunk_size = 1024) const;

    // Packet creation helpers
    model::Packet create_data_packet(int source_id, int destination_id, const Chunk& chunk) const;
    model::Packet create_ack_packet(int source_id, int destination_id, int sequence_number) const;

    // Sender state management
    void init_sender(const string& file_content, int chunk_size = 1024);
    int get_sender_chunk_count() const;
    const Chunk& get_sender_chunk(int sequence_number) const;
    const dsa::DynamicArray<Chunk>& get_all_sender_chunks() const;

    // Receiver state management and ordered chunk storage
    void init_receiver(int total_chunks);
    bool receive_chunk_packet(const model::Packet& packet);
    bool has_chunk(int sequence_number) const;
    const Chunk& get_received_chunk(int sequence_number) const;
    int get_received_chunk_count() const;
    int get_total_expected_chunks() const;

    // Missing chunks and reconstruction
    dsa::DynamicArray<int> get_missing_chunk_sequences() const;
    bool is_reconstruction_ready() const;

    // Invariant: Reconstruction returns the original bytes only when all required chunks are present and valid.
    string reconstruct_file() const;

    // State reset
    void reset_receiver();
    void reset_sender();

private:
    dsa::DynamicArray<Chunk> sender_chunks_;
    int total_expected_chunks_;
    dsa::HashMap<int, Chunk> received_chunks_;
    dsa::DynamicArray<int> received_sequences_;
};

}
}
