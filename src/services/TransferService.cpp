#include <bits/stdc++.h>

#include "services/TransferService.h"

using namespace std;

namespace pbl {
namespace services {

// Milestone 6: Core logic — Chunking, checksums, and reconstruction
// Normalizing constructor: initialize member fields inside constructor body.
TransferService::TransferService() {
    total_expected_chunks_ = 0;
}

// Educational XOR checksum calculation over the byte payload.
// Security Constraint: XOR is an educational checksum, NOT cryptographic protection.
// CRC-32/IEEE is documented as a stronger non-cryptographic alternative, but is not implemented.
int TransferService::calculate_xor_checksum(const string& data) const {
    int checksum = 0;
    int len = int(data.size());
    for (int i = 0; i < len; ++i) {
        checksum ^= static_cast<unsigned char>(data[i]);
    }
    return checksum;
}

bool TransferService::verify_checksum(const string& data, int expected_checksum) const {
    return calculate_xor_checksum(data) == expected_checksum;
}

// Splits the original file content into discrete chunks with stable sequence numbers and checksums.
dsa::DynamicArray<TransferService::Chunk>
TransferService::split_into_chunks(const string& file_content, int chunk_size) const {
    if (chunk_size <= 0) {
        throw invalid_argument("chunk size must be positive");
    }

    dsa::DynamicArray<Chunk> chunks;
    if (file_content.empty()) {
        return chunks;
    }

    int content_len = int(file_content.size());
    int seq = 0;

    for (int offset = 0; offset < content_len; offset += chunk_size) {
        int length = chunk_size;
        if (offset + length > content_len) {
            length = content_len - offset;
        }

        string chunk_data = file_content.substr(offset, length);
        int checksum = calculate_xor_checksum(chunk_data);

        chunks.push_back(Chunk(seq, chunk_data, length, checksum));
        seq++;
    }

    return chunks;
}

// Helper to wrap a chunk into a network DATA packet
model::Packet TransferService::create_data_packet(int source_id, int destination_id, const Chunk& chunk) const {
    return model::Packet(model::Packet::Type::DATA, source_id, destination_id,
                         chunk.sequence_number, chunk.data, chunk.size, chunk.checksum);
}

// Helper to create an individual ACK packet confirming receipt of a chunk sequence
model::Packet TransferService::create_ack_packet(int source_id, int destination_id, int sequence_number) const {
    return model::Packet(model::Packet::Type::ACK, source_id, destination_id,
                         sequence_number, "", 0, 0);
}

// Initializes the sender with file content and precomputed chunks
void TransferService::init_sender(const string& file_content, int chunk_size) {
    sender_chunks_ = split_into_chunks(file_content, chunk_size);
}

int TransferService::get_sender_chunk_count() const {
    return sender_chunks_.size();
}

const TransferService::Chunk& TransferService::get_sender_chunk(int sequence_number) const {
    if (sequence_number < 0 || sequence_number >= sender_chunks_.size()) {
        throw out_of_range("sender chunk sequence number out of range");
    }
    return sender_chunks_[sequence_number];
}

const dsa::DynamicArray<TransferService::Chunk>& TransferService::get_all_sender_chunks() const {
    return sender_chunks_;
}

// Initializes receiver state expecting total_chunks discrete chunks
void TransferService::init_receiver(int total_chunks) {
    if (total_chunks < 0) {
        throw invalid_argument("total chunks cannot be negative");
    }
    total_expected_chunks_ = total_chunks;
    received_chunks_.clear();
    received_sequences_.clear();
}

// Receives and validates an incoming packet.
// Invariant: A checksum mismatch rejects DATA and cannot produce a successful ACK.
// Invariant: Chunks can arrive out of order and are placed into ordered storage by sequence number.
bool TransferService::receive_chunk_packet(const model::Packet& packet) {
    if (packet.get_type() != model::Packet::Type::DATA) {
        return false;
    }

    // Integrity check: verify XOR checksum against payload
    if (!verify_checksum(packet.get_payload(), packet.get_checksum())) {
        // Checksum mismatch: reject corrupted DATA
        return false;
    }

    int seq = packet.get_sequence_number();
    if (seq < 0) {
        return false;
    }
    if (total_expected_chunks_ > 0 && seq >= total_expected_chunks_) {
        return false;
    }

    // Place into ordered chunk storage
    if (!received_chunks_.contains(seq)) {
        Chunk chunk(seq, packet.get_payload(), packet.get_payload_size(), packet.get_checksum());
        received_chunks_.put(seq, chunk);
        received_sequences_.push_back(seq);
    }

    return true;
}

bool TransferService::has_chunk(int sequence_number) const {
    return received_chunks_.contains(sequence_number);
}

const TransferService::Chunk& TransferService::get_received_chunk(int sequence_number) const {
    if (!received_chunks_.contains(sequence_number)) {
        throw out_of_range("chunk has not been received");
    }
    return received_chunks_.at(sequence_number);
}

int TransferService::get_received_chunk_count() const {
    return received_chunks_.size();
}

int TransferService::get_total_expected_chunks() const {
    return total_expected_chunks_;
}

// Returns all missing sequence numbers in [0, total_expected_chunks_ - 1]
dsa::DynamicArray<int> TransferService::get_missing_chunk_sequences() const {
    dsa::DynamicArray<int> missing;
    for (int seq = 0; seq < total_expected_chunks_; ++seq) {
        if (!received_chunks_.contains(seq)) {
            missing.push_back(seq);
        }
    }
    return missing;
}

// Checks if all required chunks from 0 to total_expected_chunks - 1 are present
bool TransferService::is_reconstruction_ready() const {
    if (total_expected_chunks_ == 0) {
        return true;
    }
    if (received_chunks_.size() != total_expected_chunks_) {
        return false;
    }
    for (int seq = 0; seq < total_expected_chunks_; ++seq) {
        if (!received_chunks_.contains(seq)) {
            return false;
        }
    }
    return true;
}

// Invariant: Reconstruction returns the original bytes only when all required chunks are present and valid.
// Reassembles chunks in strictly ascending sequence order regardless of arrival order.
string TransferService::reconstruct_file() const {
    if (!is_reconstruction_ready()) {
        throw runtime_error("cannot reconstruct file: missing chunks");
    }

    string reconstructed = "";
    for (int seq = 0; seq < total_expected_chunks_; ++seq) {
        reconstructed += received_chunks_.at(seq).data;
    }
    return reconstructed;
}

void TransferService::reset_receiver() {
    total_expected_chunks_ = 0;
    received_chunks_.clear();
    received_sequences_.clear();
}

void TransferService::reset_sender() {
    sender_chunks_.clear();
}

}
}
