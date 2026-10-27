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

} // namespace

int main() {
    test_chunking_and_sequence_tracking();
    test_xor_checksum_integrity_and_corruption_detection();
    test_out_of_order_arrival_and_ordered_reconstruction();
    test_checksum_mismatch_rejects_data();
    test_sender_state();
    test_duplicate_chunks_idempotency();
    cout << "All Milestone 6 chunking, checksums, and reconstruction tests passed successfully!" << endl;
    return 0;
}
