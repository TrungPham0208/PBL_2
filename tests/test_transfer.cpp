#include <bits/stdc++.h>

#include "model/Packet.h"
#include "services/TransferService.h"

using namespace std;
using namespace pbl;

namespace {

// Milestone 6: Test file chunking, sequence numbering, and size calculations
void test_chunking_and_sequence_tracking() {
    pbl::services::TransferService transfer;

    string content = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"; // 36 bytes
    int chunk_size = 10;

    dsa::DynamicArray<pbl::services::TransferService::Chunk> chunks =
        transfer.split_into_chunks(content, chunk_size);

    // 36 bytes with 10 bytes/chunk -> 4 chunks (10, 10, 10, 6)
    assert(chunks.size() == 4);

    assert(chunks[0].sequence_number == 0);
    assert(chunks[0].data == "ABCDEFGHIJ");
    assert(chunks[0].size == 10);

    assert(chunks[1].sequence_number == 1);
    assert(chunks[1].data == "KLMNOPQRST");
    assert(chunks[1].size == 10);

    assert(chunks[2].sequence_number == 2);
    assert(chunks[2].data == "UVWXYZ0123");
    assert(chunks[2].size == 10);

    assert(chunks[3].sequence_number == 3);
    assert(chunks[3].data == "456789");
    assert(chunks[3].size == 6);

    // Empty content produces 0 chunks
    dsa::DynamicArray<pbl::services::TransferService::Chunk> empty_chunks =
        transfer.split_into_chunks("", 10);
    assert(empty_chunks.empty());

    // Invalid non-positive chunk size throws invalid_argument
    bool rejected = false;
    try {
        transfer.split_into_chunks(content, 0);
    } catch (const invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
}

// Milestone 6: Test educational XOR checksum calculation and corruption detection
void test_xor_checksum_integrity_and_corruption_detection() {
    pbl::services::TransferService transfer;

    string original = "PacketPayloadData123";
    int checksum = transfer.calculate_xor_checksum(original);

    // Verifying intact data succeeds
    assert(transfer.verify_checksum(original, checksum));

    // Corruption: single character modification (bit flip / substitution)
    string corrupted = "QacketPayloadData123";
    assert(!transfer.verify_checksum(corrupted, checksum));

    // Corruption: payload truncation by 1 byte
    string truncated = "PacketPayloadData12";
    assert(!transfer.verify_checksum(truncated, checksum));

    // Invariant check: XOR is an educational check, not cryptographic protection.
    // Confirm documented behavior for empty string
    assert(transfer.calculate_xor_checksum("") == 0);
}

// Milestone 6: Test out-of-order chunk arrival and ordered reconstruction
void test_out_of_order_arrival_and_ordered_reconstruction() {
    pbl::services::TransferService transfer;

    string original_file = "The quick brown fox jumps over the lazy dog."; // 44 characters
    int chunk_size = 10; // Chunks: 0..4 (5 chunks)

    dsa::DynamicArray<pbl::services::TransferService::Chunk> chunks =
        transfer.split_into_chunks(original_file, chunk_size);
    assert(chunks.size() == 5);

    transfer.init_receiver(5);
    assert(!transfer.is_reconstruction_ready());
    assert(transfer.get_received_chunk_count() == 0);

    // Create packets
    model::Packet pkt0 = transfer.create_data_packet(1, 2, chunks[0]);
    model::Packet pkt1 = transfer.create_data_packet(1, 2, chunks[1]);
    model::Packet pkt2 = transfer.create_data_packet(1, 2, chunks[2]);
    model::Packet pkt3 = transfer.create_data_packet(1, 2, chunks[3]);
    model::Packet pkt4 = transfer.create_data_packet(1, 2, chunks[4]);

    // Deliver strictly out of order: 3 -> 1 -> 4 -> 0
    assert(transfer.receive_chunk_packet(pkt3));
    assert(transfer.receive_chunk_packet(pkt1));
    assert(transfer.receive_chunk_packet(pkt4));
    assert(transfer.receive_chunk_packet(pkt0));

    assert(transfer.get_received_chunk_count() == 4);
    assert(!transfer.is_reconstruction_ready());

    // Missing chunk check: sequence 2 is missing
    dsa::DynamicArray<int> missing = transfer.get_missing_chunk_sequences();
    assert(missing.size() == 1);
    assert(missing[0] == 2);

    // Attempting reconstruction before all chunks are present must fail
    bool reconstruction_failed = false;
    try {
        transfer.reconstruct_file();
    } catch (const runtime_error&) {
        reconstruction_failed = true;
    }
    assert(reconstruction_failed);

    // Now deliver the missing chunk 2
    assert(transfer.receive_chunk_packet(pkt2));
    assert(transfer.get_received_chunk_count() == 5);
    assert(transfer.is_reconstruction_ready());
    assert(transfer.get_missing_chunk_sequences().empty());

    // Invariant: Reconstruction reassembles all chunks in exact numerical sequence [0, 1, 2, 3, 4]
    string reconstructed = transfer.reconstruct_file();
    assert(reconstructed == original_file);
}

// Milestone 6: Test that checksum mismatch rejects DATA and cannot be accepted
void test_checksum_mismatch_rejects_data() {
    pbl::services::TransferService transfer;

    pbl::services::TransferService::Chunk chunk(0, "ValidData", 9, 0);
    chunk.checksum = transfer.calculate_xor_checksum(chunk.data);

    model::Packet packet = transfer.create_data_packet(1, 2, chunk);

    transfer.init_receiver(1);

    // Simulating packet payload corruption in transit
    packet.set_payload("CorruptData"); // Checksum in packet still belongs to "ValidData"

    // Invariant: A checksum mismatch rejects DATA and cannot produce a successful ACK.
    bool accepted = transfer.receive_chunk_packet(packet);
    assert(!accepted);
    assert(!transfer.has_chunk(0));
    assert(transfer.get_received_chunk_count() == 0);
    assert(!transfer.is_reconstruction_ready());
}

// Milestone 6: Test sender state initialization and inspection
void test_sender_state() {
    pbl::services::TransferService transfer;

    string content = "HelloNetworkSimulator";
    transfer.init_sender(content, 5);

    assert(transfer.get_sender_chunk_count() == 5);
    assert(transfer.get_sender_chunk(0).data == "Hello");
    assert(transfer.get_sender_chunk(1).data == "Netwo");
    assert(transfer.get_sender_chunk(4).data == "r");

    // ACK packet builder
    model::Packet ack = transfer.create_ack_packet(2, 1, 3);
    assert(ack.get_type() == model::Packet::Type::ACK);
    assert(ack.get_sequence_number() == 3);
    assert(ack.get_source_id() == 2);
    assert(ack.get_destination_id() == 1);
}

// Milestone 6: Test duplicate chunk arrival idempotency
void test_duplicate_chunks_idempotency() {
    pbl::services::TransferService transfer;
    transfer.init_receiver(2);

    pbl::services::TransferService::Chunk chunk0(0, "Part0", 5, 0);
    chunk0.checksum = transfer.calculate_xor_checksum(chunk0.data);
    model::Packet pkt0 = transfer.create_data_packet(1, 2, chunk0);

    // Receive chunk 0 first time
    assert(transfer.receive_chunk_packet(pkt0));
    assert(transfer.get_received_chunk_count() == 1);

    // Receive chunk 0 duplicate
    assert(transfer.receive_chunk_packet(pkt0));
    // Size must remain 1 without duplicates
    assert(transfer.get_received_chunk_count() == 1);
}

// Milestone 7: Test individual ACK tracking, duplicate handling, and invariant checks
void test_individual_ack_tracking() {
    pbl::services::TransferService transfer;
    transfer.init_sender("HelloACKTracker", 5); // 15 characters -> 3 chunks (5, 5, 5)
    assert(transfer.get_sender_chunk_count() == 3);
    assert(transfer.get_transfer_status() == pbl::services::TransferService::TransferStatus::IDLE);

    // Initial check: no chunk acknowledged
    assert(!transfer.is_chunk_acknowledged(0));
    assert(!transfer.is_chunk_acknowledged(1));
    assert(!transfer.is_chunk_acknowledged(2));
    assert(transfer.get_chunk_attempts(0) == 0);

    // Dispatch chunk 0 at tick 10
    model::Packet pkt0 = transfer.send_chunk(0, 1, 2, 10);
    assert(pkt0.get_type() == model::Packet::Type::DATA);
    assert(pkt0.get_sequence_number() == 0);
    assert(transfer.get_chunk_attempts(0) == 1);
    assert(transfer.get_chunk_state(0) == pbl::services::TransferService::ChunkState::IN_FLIGHT);
    assert(transfer.get_transfer_status() == pbl::services::TransferService::TransferStatus::IN_PROGRESS);

    // Invariant: Non-ACK packets (e.g. DATA or ROUTE_PROBE) cannot mark chunks delivered
    model::Packet fake_data(model::Packet::Type::DATA, 2, 1, 0, "test", 4, 0);
    assert(!transfer.process_ack_packet(fake_data));
    assert(!transfer.is_chunk_acknowledged(0));

    // Invariant: Out-of-range ACK sequence numbers are rejected
    model::Packet out_of_bounds_ack = transfer.create_ack_packet(2, 1, 99);
    assert(!transfer.process_ack_packet(out_of_bounds_ack));

    // Valid ACK for chunk 0 marks it delivered
    model::Packet ack0 = transfer.create_ack_packet(2, 1, 0);
    assert(transfer.process_ack_packet(ack0));
    assert(transfer.is_chunk_acknowledged(0));
    assert(transfer.get_chunk_state(0) == pbl::services::TransferService::ChunkState::ACKNOWLEDGED);
    assert(!transfer.is_transfer_completed()); // 1/3 acknowledged

    // Invariant: Duplicate ACK is ignored idempotently without inflating counts
    assert(!transfer.process_ack_packet(ack0));
    assert(transfer.get_statistics().acknowledged_count == 1);

    // Invariant: Trying to send an already acknowledged chunk is rejected
    bool resend_rejected = false;
    try {
        transfer.send_chunk(0, 1, 2, 20);
    } catch (const runtime_error&) {
        resend_rejected = true;
    }
    assert(resend_rejected);
}

// Milestone 7: Test that lost or corrupted DATA packet remains unacknowledged
void test_unacknowledged_on_loss_or_corruption() {
    pbl::services::TransferService transfer;
    transfer.init_sender("IntegrityTestData", 10);
    transfer.init_receiver(transfer.get_sender_chunk_count());

    model::Packet pkt = transfer.send_chunk(0, 1, 2, 0);

    // Simulate corruption in transit
    pkt.set_payload("CorruptData!");

    // Receiver rejects corrupted packet
    bool accepted = transfer.receive_chunk_packet(pkt);
    assert(!accepted);

    // Invariant: A lost, corrupted, or missing DATA packet remains unacknowledged
    assert(!transfer.is_chunk_acknowledged(0));
    assert(transfer.get_statistics().acknowledged_count == 0);
}

// Milestone 7: Test timeout detection and retransmission walkthrough (Scenario: one timeout/retry)
void test_timeout_and_retransmission_walkthrough() {
    pbl::services::TransferService transfer;
    transfer.init_sender("TestDataForRetransmission", 10); // 3 chunks
    transfer.init_receiver(3);

    // Initial configuration: 100 ms timeout, 3 max attempts
    assert(transfer.get_ack_timeout() == 100);
    assert(transfer.get_max_attempts() == 3);

    // Send chunk 0 at tick 0 (Attempt 1)
    model::Packet pkt0 = transfer.send_chunk(0, 1, 2, 0);
    assert(transfer.get_chunk_attempts(0) == 1);
    assert(transfer.get_statistics().sent_count == 1);
    assert(transfer.get_statistics().retransmitted_count == 0);

    // At tick 50: no timeout yet (< 100 ms)
    dsa::DynamicArray<int> timeouts_at_50 = transfer.check_timeouts(50);
    assert(timeouts_at_50.empty());

    // At tick 100: timeout occurs for chunk 0! (100 - 0 >= 100)
    dsa::DynamicArray<int> timeouts_at_100 = transfer.check_timeouts(100);
    assert(timeouts_at_100.size() == 1);
    assert(timeouts_at_100[0] == 0);
    assert(transfer.get_chunk_state(0) == pbl::services::TransferService::ChunkState::TIMED_OUT);
    assert(transfer.get_statistics().timeout_count == 1);

    // Retransmit chunk 0 at tick 100 (Attempt 2)
    model::Packet retry_pkt0 = transfer.send_chunk(0, 1, 2, 100);
    assert(transfer.get_chunk_attempts(0) == 2);
    assert(transfer.get_statistics().sent_count == 2);
    assert(transfer.get_statistics().retransmitted_count == 1);

    // Receiver accepts retransmitted chunk 0
    assert(transfer.receive_chunk_packet(retry_pkt0));

    // Receiver generates ACK, sender receives ACK at tick 120
    model::Packet ack0 = transfer.create_ack_packet(2, 1, 0);
    assert(transfer.process_ack_packet(ack0));
    assert(transfer.is_chunk_acknowledged(0));
    assert(transfer.get_statistics().acknowledged_count == 1);
}

// Milestone 7: Test retry limit reached and transfer failure (Scenario: retry-limit failure)
void test_retry_limit_failure_walkthrough() {
    pbl::services::TransferService transfer;
    transfer.init_sender("FailureData", 10); // 2 chunks

    // Attempt 1 at tick 0
    transfer.send_chunk(0, 1, 2, 0);
    assert(transfer.get_chunk_attempts(0) == 1);

    // Tick 100: timeout 1 -> retransmit
    dsa::DynamicArray<int> to1 = transfer.check_timeouts(100);
    assert(to1.size() == 1 && to1[0] == 0);
    // Attempt 2 at tick 100
    transfer.send_chunk(0, 1, 2, 100);
    assert(transfer.get_chunk_attempts(0) == 2);

    // Tick 200: timeout 2 -> retransmit
    dsa::DynamicArray<int> to2 = transfer.check_timeouts(200);
    assert(to2.size() == 1 && to2[0] == 0);
    // Attempt 3 at tick 200 (maximum allowed attempts)
    transfer.send_chunk(0, 1, 2, 200);
    assert(transfer.get_chunk_attempts(0) == 3);

    // Tick 300: timeout 3 (attempt 3 has timed out: 300 - 200 >= 100)
    dsa::DynamicArray<int> to3 = transfer.check_timeouts(300);

    // Invariant: The transfer fails after the final allowed attempt (3 attempts) and preserves evidence in statistics.
    assert(to3.empty()); // No more retransmissions allowed!
    assert(transfer.is_transfer_failed());
    assert(transfer.get_chunk_state(0) == pbl::services::TransferService::ChunkState::FAILED);

    const pbl::services::TransferService::TransferStatistics& stats = transfer.get_statistics();
    assert(stats.sent_count == 3);
    assert(stats.retransmitted_count == 2);
    assert(stats.timeout_count == 3);
    assert(stats.failed_count == 1);
    assert(stats.acknowledged_count == 0);
}

// Milestone 7: Test that acknowledged chunks are not retransmitted unnecessarily after route failure
void test_unacknowledged_chunks_after_route_failure() {
    pbl::services::TransferService transfer;
    transfer.init_sender("ABCDEFGHIJKLMNO", 5); // 3 chunks: seq 0, 1, 2
    assert(transfer.get_sender_chunk_count() == 3);

    transfer.send_chunk(0, 1, 2, 0);
    transfer.send_chunk(1, 1, 2, 0);
    transfer.send_chunk(2, 1, 2, 0);

    // Chunks 0 and 2 receive ACKs successfully
    transfer.process_ack_packet(transfer.create_ack_packet(2, 1, 0));
    transfer.process_ack_packet(transfer.create_ack_packet(2, 1, 2));

    // Chunk 1 was in transit when the route failed and its ACK was lost.
    // Invariant: Acknowledged chunks are not retransmitted unnecessarily after a route failure.
    dsa::DynamicArray<int> unacked = transfer.get_unacknowledged_chunks();
    assert(unacked.size() == 1);
    assert(unacked[0] == 1);

    // Verify chunk 0 and 2 remain acknowledged
    assert(transfer.is_chunk_acknowledged(0));
    assert(!transfer.is_chunk_acknowledged(1));
    assert(transfer.is_chunk_acknowledged(2));

    // Transfer can be resumed for chunk 1 over a replacement route
    transfer.send_chunk(1, 1, 2, 100);
    transfer.process_ack_packet(transfer.create_ack_packet(2, 1, 1));
    assert(transfer.is_transfer_completed());
}

} // namespace

int main() {
    test_chunking_and_sequence_tracking();
    test_xor_checksum_integrity_and_corruption_detection();
    test_out_of_order_arrival_and_ordered_reconstruction();
    test_checksum_mismatch_rejects_data();
    test_sender_state();
    test_duplicate_chunks_idempotency();

    // Milestone 7 tests
    test_individual_ack_tracking();
    test_unacknowledged_on_loss_or_corruption();
    test_timeout_and_retransmission_walkthrough();
    test_retry_limit_failure_walkthrough();
    test_unacknowledged_chunks_after_route_failure();

    cout << "All Milestone 6 and Milestone 7 reliable transfer tests passed successfully!" << endl;
    return 0;
}
