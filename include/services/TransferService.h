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

    // Milestone 7: Core logic — ACKs, timeout, retransmission, and retry limits
    enum class ChunkState {
        UNSENT,
        IN_FLIGHT,
        TIMED_OUT,
        ACKNOWLEDGED,
        FAILED
    };

    enum class TransferStatus {
        IDLE,
        IN_PROGRESS,
        COMPLETED,
        FAILED
    };

    // Tracking metadata for each individual chunk at the sender
    struct ChunkProgress {
        int sequence_number;
        ChunkState state;
        int attempts;
        int last_sent_tick;
        bool acknowledged;

        ChunkProgress() {
            sequence_number = -1;
            state = ChunkState::UNSENT;
            attempts = 0;
            last_sent_tick = -1;
            acknowledged = false;
        }

        ChunkProgress(int seq) {
            sequence_number = seq;
            state = ChunkState::UNSENT;
            attempts = 0;
            last_sent_tick = -1;
            acknowledged = false;
        }
    };

    // Statistics tracking for file transfer progress and failure evidence
    struct TransferStatistics {
        int sent_count;
        int acknowledged_count;
        int retransmitted_count;
        int timeout_count;
        int failed_count;

        TransferStatistics() {
            sent_count = 0;
            acknowledged_count = 0;
            retransmitted_count = 0;
            timeout_count = 0;
            failed_count = 0;
        }
    };

    static const int DEFAULT_ACK_TIMEOUT = 100; // 100 ms per SPEC.md
    static const int DEFAULT_MAX_ATTEMPTS = 3;  // 3 attempts max per SPEC.md

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

    // Milestone 7: Configuration
    void set_ack_timeout(int timeout_ms);
    int get_ack_timeout() const;
    void set_max_attempts(int max_attempts);
    int get_max_attempts() const;

    // Milestone 7: Transfer progress and state queries
    TransferStatus get_transfer_status() const;
    bool is_transfer_completed() const;
    bool is_transfer_failed() const;
    bool is_chunk_acknowledged(int sequence_number) const;
    int get_chunk_attempts(int sequence_number) const;
    ChunkState get_chunk_state(int sequence_number) const;
    const ChunkProgress& get_chunk_progress(int sequence_number) const;
    const TransferStatistics& get_statistics() const;
    void reset_statistics();

    // Milestone 7: Invariant: Acknowledged chunks are not retransmitted unnecessarily after a route failure
    dsa::DynamicArray<int> get_unacknowledged_chunks() const;

    // Milestone 7: Sender packet dispatch with attempt tracking
    model::Packet send_chunk(int sequence_number, int source_id, int destination_id, int current_tick);

    // Milestone 7: Invariant: Only a valid ACK for the matching chunk can mark that chunk delivered
    // Invariant: A lost, corrupted, or missing DATA packet remains unacknowledged
    bool process_ack_packet(const model::Packet& packet);

    // Milestone 7: Invariant: Timeout checking and retry limits
    // Returns sequence numbers of chunks eligible for retransmission at current_tick.
    // Invariant: The transfer fails after the final allowed attempt (3 attempts) and preserves evidence in statistics.
    dsa::DynamicArray<int> check_timeouts(int current_tick);

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

    // Milestone 7 state
    int ack_timeout_ms_;
    int max_attempts_;
    TransferStatus transfer_status_;
    dsa::DynamicArray<ChunkProgress> chunk_progress_;
    TransferStatistics statistics_;
};

}
}
