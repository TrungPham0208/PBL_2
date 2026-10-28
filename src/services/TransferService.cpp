#include <bits/stdc++.h>

#include "services/TransferService.h"

using namespace std;

namespace pbl {
namespace services {

// Milestone 6 & 7: Core logic — Chunking, checksums, reconstruction, and reliable transfer
// Normalizing constructor: initialize member fields inside constructor body.
TransferService::TransferService() {
    total_expected_chunks_ = 0;
    ack_timeout_ms_ = DEFAULT_ACK_TIMEOUT;
    max_attempts_ = DEFAULT_MAX_ATTEMPTS;
    transfer_status_ = TransferStatus::IDLE;
    statistics_ = TransferStatistics();
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
    chunk_progress_.clear();
    for (int i = 0; i < sender_chunks_.size(); ++i) {
        chunk_progress_.push_back(ChunkProgress(i));
    }
    if (sender_chunks_.empty()) {
        transfer_status_ = TransferStatus::COMPLETED;
    } else {
        transfer_status_ = TransferStatus::IDLE;
    }
    statistics_ = TransferStatistics();
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
    chunk_progress_.clear();
    transfer_status_ = TransferStatus::IDLE;
    statistics_ = TransferStatistics();
}

// Milestone 7: Configuration
void TransferService::set_ack_timeout(int timeout_ms) {
    if (timeout_ms <= 0) {
        throw invalid_argument("ack timeout must be positive");
    }
    ack_timeout_ms_ = timeout_ms;
}

int TransferService::get_ack_timeout() const {
    return ack_timeout_ms_;
}

void TransferService::set_max_attempts(int max_attempts) {
    if (max_attempts <= 0) {
        throw invalid_argument("max attempts must be positive");
    }
    max_attempts_ = max_attempts;
}

int TransferService::get_max_attempts() const {
    return max_attempts_;
}

// Milestone 7: Transfer progress and state queries
TransferService::TransferStatus TransferService::get_transfer_status() const {
    return transfer_status_;
}

bool TransferService::is_transfer_completed() const {
    return transfer_status_ == TransferStatus::COMPLETED;
}

bool TransferService::is_transfer_failed() const {
    return transfer_status_ == TransferStatus::FAILED;
}

bool TransferService::is_chunk_acknowledged(int sequence_number) const {
    if (sequence_number < 0 || sequence_number >= chunk_progress_.size()) {
        return false;
    }
    return chunk_progress_[sequence_number].acknowledged;
}

int TransferService::get_chunk_attempts(int sequence_number) const {
    if (sequence_number < 0 || sequence_number >= chunk_progress_.size()) {
        return 0;
    }
    return chunk_progress_[sequence_number].attempts;
}

TransferService::ChunkState TransferService::get_chunk_state(int sequence_number) const {
    if (sequence_number < 0 || sequence_number >= chunk_progress_.size()) {
        return ChunkState::UNSENT;
    }
    return chunk_progress_[sequence_number].state;
}

const TransferService::ChunkProgress& TransferService::get_chunk_progress(int sequence_number) const {
    if (sequence_number < 0 || sequence_number >= chunk_progress_.size()) {
        throw out_of_range("sequence number out of range");
    }
    return chunk_progress_[sequence_number];
}

const TransferService::TransferStatistics& TransferService::get_statistics() const {
    return statistics_;
}

void TransferService::reset_statistics() {
    statistics_ = TransferStatistics();
}

// Milestone 7: Invariant: Acknowledged chunks are not retransmitted unnecessarily after a route failure
// Returns only the sequence numbers of chunks that have not yet been successfully acknowledged.
dsa::DynamicArray<int> TransferService::get_unacknowledged_chunks() const {
    dsa::DynamicArray<int> unacknowledged;
    for (int i = 0; i < chunk_progress_.size(); ++i) {
        if (!chunk_progress_[i].acknowledged) {
            unacknowledged.push_back(i);
        }
    }
    return unacknowledged;
}

// Milestone 7: Sender packet dispatch with attempt tracking
// Invariant: Acknowledged chunks are not retransmitted unnecessarily.
model::Packet TransferService::send_chunk(int sequence_number, int source_id, int destination_id, int current_tick) {
    if (sequence_number < 0 || sequence_number >= sender_chunks_.size()) {
        throw out_of_range("chunk sequence number out of range");
    }

    if (chunk_progress_[sequence_number].acknowledged) {
        throw runtime_error("cannot send chunk: chunk is already acknowledged");
    }

    chunk_progress_[sequence_number].attempts++;
    chunk_progress_[sequence_number].last_sent_tick = current_tick;
    chunk_progress_[sequence_number].state = ChunkState::IN_FLIGHT;

    statistics_.sent_count++;
    if (chunk_progress_[sequence_number].attempts > 1) {
        statistics_.retransmitted_count++;
    }

    if (transfer_status_ == TransferStatus::IDLE) {
        transfer_status_ = TransferStatus::IN_PROGRESS;
    }

    return create_data_packet(source_id, destination_id, sender_chunks_[sequence_number]);
}

// Milestone 7: Invariant: Only a valid ACK for the matching chunk can mark that chunk delivered.
// Invariant: A lost, corrupted, or missing DATA packet remains unacknowledged.
bool TransferService::process_ack_packet(const model::Packet& packet) {
    if (packet.get_type() != model::Packet::Type::ACK) {
        return false;
    }

    int seq = packet.get_sequence_number();
    if (seq < 0 || seq >= chunk_progress_.size()) {
        return false;
    }

    // Duplicate ACK check: if already acknowledged, ignore duplicate ACK idempotently
    if (chunk_progress_[seq].acknowledged) {
        return false;
    }

    chunk_progress_[seq].acknowledged = true;
    chunk_progress_[seq].state = ChunkState::ACKNOWLEDGED;
    statistics_.acknowledged_count++;

    if (statistics_.acknowledged_count == sender_chunks_.size()) {
        transfer_status_ = TransferStatus::COMPLETED;
    }

    return true;
}

// Milestone 7: Invariant: Timeout checking and retry limits
// Returns sequence numbers of chunks eligible for retransmission at current_tick.
// Invariant: The transfer fails after the final allowed attempt (3 attempts) and preserves evidence in statistics.
dsa::DynamicArray<int> TransferService::check_timeouts(int current_tick) {
    dsa::DynamicArray<int> retransmit_list;

    for (int seq = 0; seq < chunk_progress_.size(); ++seq) {
        if (chunk_progress_[seq].acknowledged) {
            continue;
        }

        if (chunk_progress_[seq].attempts == 0) {
            continue;
        }

        if (chunk_progress_[seq].state != ChunkState::IN_FLIGHT) {
            continue;
        }

        if (current_tick - chunk_progress_[seq].last_sent_tick >= ack_timeout_ms_) {
            statistics_.timeout_count++;

            if (chunk_progress_[seq].attempts < max_attempts_) {
                chunk_progress_[seq].state = ChunkState::TIMED_OUT;
                retransmit_list.push_back(seq);
            } else {
                // Maximum attempts reached without receiving an ACK
                chunk_progress_[seq].state = ChunkState::FAILED;
                transfer_status_ = TransferStatus::FAILED;
                statistics_.failed_count++;
            }
        }
    }

    return retransmit_list;
}

}
}
