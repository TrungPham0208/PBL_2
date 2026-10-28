#include <bits/stdc++.h>

#include "model/Client.h"
#include "model/Link.h"
#include "model/NetworkGraph.h"
#include "model/Router.h"
#include "services/FileStorageService.h"

using namespace std;

// Milestone 10: Unit and regression tests for JSON configuration and experiment persistence.
// Tests verify:
// 1. JSON recursive descent parser & serializer fidelity (types, nesting, formatting).
// 2. Strict rejection of invalid / malformed / incomplete JSON structures.
// 3. Topology configuration persistence (save/load) and bidirectional graph translation.
// 4. Experiment result persistence (statistics, route changes, event log).
// 5. Detection and rejection of invalid topologies (missing nodes, dangling links).

static int tests_run = 0;
static int tests_passed = 0;

#define TEST_ASSERT(cond, msg) \
    do { \
        tests_run++; \
        if (!(cond)) { \
            cerr << "[FAIL] Line " << __LINE__ << ": " << msg << endl; \
        } else { \
            tests_passed++; \
        } \
    } while (0)

void test_json_primitives_and_nesting() {
    pbl::services::FileStorageService storage;

    string sample_json = "{\n"
                         "  \"name\": \"simulator_test\",\n"
                         "  \"version\": 2,\n"
                         "  \"loss_rate\": 0.05,\n"
                         "  \"is_active\": true,\n"
                         "  \"is_corrupted\": false,\n"
                         "  \"empty_val\": null,\n"
                         "  \"tags\": [\"pbl\", \"network\", 42],\n"
                         "  \"nested\": {\n"
                         "    \"inner_key\": \"nested_val\"\n"
                         "  }\n"
                         "}";

    pbl::services::FileStorageService::JsonValue parsed = storage.parse_json(sample_json);
    TEST_ASSERT(parsed.type == pbl::services::FileStorageService::JsonType::JSON_OBJECT, "Parsed root is object");
    TEST_ASSERT(parsed.has_key("name"), "Root has 'name' key");
    TEST_ASSERT(parsed.get("name").as_string() == "simulator_test", "Name matches");
    TEST_ASSERT(parsed.get("version").as_int() == 2, "Version matches integer 2");
    TEST_ASSERT(abs(parsed.get("loss_rate").as_double() - 0.05) < 1e-6, "Loss rate matches 0.05");
    TEST_ASSERT(parsed.get("is_active").as_bool() == true, "is_active is true");
    TEST_ASSERT(parsed.get("is_corrupted").as_bool() == false, "is_corrupted is false");
    TEST_ASSERT(parsed.get("empty_val").type == pbl::services::FileStorageService::JsonType::JSON_NULL, "empty_val is null");

    const pbl::services::FileStorageService::JsonValue& tags = parsed.get("tags");
    TEST_ASSERT(tags.type == pbl::services::FileStorageService::JsonType::JSON_ARRAY, "tags is array");
    TEST_ASSERT(tags.array_value.size() == 3, "tags has 3 items");
    TEST_ASSERT(tags.array_value[0].as_string() == "pbl", "tags[0] is 'pbl'");
    TEST_ASSERT(tags.array_value[1].as_string() == "network", "tags[1] is 'network'");
    TEST_ASSERT(tags.array_value[2].as_int() == 42, "tags[2] is 42");

    const pbl::services::FileStorageService::JsonValue& nested = parsed.get("nested");
    TEST_ASSERT(nested.type == pbl::services::FileStorageService::JsonType::JSON_OBJECT, "nested is object");
    TEST_ASSERT(nested.get("inner_key").as_string() == "nested_val", "inner_key matches nested_val");

    // Serialization roundtrip
    string serialized = storage.serialize_json(parsed);
    pbl::services::FileStorageService::JsonValue roundtrip = storage.parse_json(serialized);
    TEST_ASSERT(roundtrip.get("name").as_string() == "simulator_test", "Roundtrip preserved string");
    TEST_ASSERT(roundtrip.get("version").as_int() == 2, "Roundtrip preserved number");
    TEST_ASSERT(roundtrip.get("tags").array_value.size() == 3, "Roundtrip preserved array size");
}

void test_json_malformed_rejection() {
    pbl::services::FileStorageService storage;

    // Test incomplete object (missing closing brace)
    bool threw1 = false;
    try {
        storage.parse_json("{\"key\": \"value\"");
    } catch (const runtime_error& e) {
        threw1 = true;
    }
    TEST_ASSERT(threw1, "Malformed unclosed object rejected");

    // Test unquoted key in object
    bool threw2 = false;
    try {
        storage.parse_json("{invalid_key: 123}");
    } catch (const runtime_error& e) {
        threw2 = true;
    }
    TEST_ASSERT(threw2, "Unquoted key in object rejected");

    // Test unclosed string
    bool threw3 = false;
    try {
        storage.parse_json("{\"key\": \"unclosed string}");
    } catch (const runtime_error& e) {
        threw3 = true;
    }
    TEST_ASSERT(threw3, "Unclosed string literal rejected");

    // Test trailing unexpected characters
    bool threw4 = false;
    try {
        storage.parse_json("{\"key\": 10} extra_garbage");
    } catch (const runtime_error& e) {
        threw4 = true;
    }
    TEST_ASSERT(threw4, "Trailing unexpected tokens rejected");

    // Test missing colon
    bool threw5 = false;
    try {
        storage.parse_json("{\"key\" \"val\"}");
    } catch (const runtime_error& e) {
        threw5 = true;
    }
    TEST_ASSERT(threw5, "Missing colon between key and value rejected");
}

void test_config_save_load_and_graph_application() {
    pbl::services::FileStorageService storage;
    string temp_config_file = "test_temp_config.json";

    pbl::services::FileStorageService::SimulationConfig config;
    config.random_seed = 998877;
    config.chunk_size = 2048;
    config.heartbeat_interval = 50;
    config.failure_timeout = 150;
    config.source_client_id = 1;
    config.destination_client_id = 4;

    config.nodes.push_back(pbl::services::FileStorageService::NodeConfig(1, "ClientA", "Client"));
    config.nodes.push_back(pbl::services::FileStorageService::NodeConfig(2, "Router1", "Router"));
    config.nodes.push_back(pbl::services::FileStorageService::NodeConfig(3, "Router2", "Router"));
    config.nodes.push_back(pbl::services::FileStorageService::NodeConfig(4, "ClientB", "Client"));

    config.links.push_back(pbl::services::FileStorageService::LinkConfig(1, 2, 100, 5, 0.0, 0.0, 10));
    config.links.push_back(pbl::services::FileStorageService::LinkConfig(2, 3, 50, 15, 0.02, 0.01, 20));
    config.links.push_back(pbl::services::FileStorageService::LinkConfig(3, 4, 100, 5, 0.0, 0.0, 10));

    // Save config
    storage.save_config(temp_config_file, config);

    // Load config back
    pbl::services::FileStorageService::SimulationConfig loaded = storage.load_config(temp_config_file);
    TEST_ASSERT(loaded.random_seed == 998877, "Loaded seed matches");
    TEST_ASSERT(loaded.chunk_size == 2048, "Loaded chunk size matches");
    TEST_ASSERT(loaded.heartbeat_interval == 50, "Loaded heartbeat interval matches");
    TEST_ASSERT(loaded.failure_timeout == 150, "Loaded failure timeout matches");
    TEST_ASSERT(loaded.source_client_id == 1, "Loaded source ID matches");
    TEST_ASSERT(loaded.destination_client_id == 4, "Loaded dest ID matches");
    TEST_ASSERT(loaded.nodes.size() == 4, "Loaded node count matches");
    TEST_ASSERT(loaded.links.size() == 3, "Loaded link count matches");

    // Apply config to NetworkGraph
    pbl::model::NetworkGraph graph;
    storage.apply_config_to_graph(loaded, graph);

    TEST_ASSERT(graph.get_node(1) != nullptr, "Node 1 exists in graph");
    TEST_ASSERT(graph.get_node(2) != nullptr, "Node 2 exists in graph");
    TEST_ASSERT(graph.get_node(3) != nullptr, "Node 3 exists in graph");
    TEST_ASSERT(graph.get_node(4) != nullptr, "Node 4 exists in graph");
    TEST_ASSERT(graph.has_link(1, 2), "Link 1-2 exists in graph");
    TEST_ASSERT(graph.has_link(2, 3), "Link 2-3 exists in graph");
    TEST_ASSERT(graph.has_link(3, 4), "Link 3-4 exists in graph");

    pbl::model::Link* l23 = graph.get_link(2, 3);
    TEST_ASSERT(l23 != nullptr, "Link 2-3 retrievable");
    TEST_ASSERT(l23->get_bandwidth() == 50, "Link 2-3 bandwidth matches");
    TEST_ASSERT(l23->get_latency() == 15, "Link 2-3 latency matches");
    TEST_ASSERT(abs(l23->get_loss_probability() - 0.02) < 1e-6, "Link 2-3 loss prob matches");
    TEST_ASSERT(abs(l23->get_corruption_probability() - 0.01) < 1e-6, "Link 2-3 corruption prob matches");

    // Extract config from graph and check consistency
    pbl::services::FileStorageService::SimulationConfig extracted =
        storage.extract_config_from_graph(graph, 998877, 2048, 1, 4);
    TEST_ASSERT(extracted.nodes.size() == 4, "Extracted nodes count is 4");
    TEST_ASSERT(extracted.links.size() == 3, "Extracted links count is 3");
    TEST_ASSERT(extracted.source_client_id == 1, "Extracted source ID matches");
    TEST_ASSERT(extracted.destination_client_id == 4, "Extracted dest ID matches");

    // Clean up temporary file
    remove(temp_config_file.c_str());
}

void test_experiment_result_persistence() {
    pbl::services::FileStorageService storage;
    string temp_exp_file = "test_temp_experiment.json";

    pbl::services::FileStorageService::ExperimentResult result;
    result.experiment_id = "EXP-20261028-001";
    result.outcome = "SUCCESS";
    result.total_ticks = 425;

    result.config.random_seed = 42;
    result.config.chunk_size = 1024;
    result.config.source_client_id = 1;
    result.config.destination_client_id = 3;
    result.config.nodes.push_back(pbl::services::FileStorageService::NodeConfig(1, "Client1", "Client"));
    result.config.nodes.push_back(pbl::services::FileStorageService::NodeConfig(2, "Router1", "Router"));
    result.config.nodes.push_back(pbl::services::FileStorageService::NodeConfig(3, "Client2", "Client"));
    result.config.links.push_back(pbl::services::FileStorageService::LinkConfig(1, 2, 100, 10, 0.0, 0.0, 10));
    result.config.links.push_back(pbl::services::FileStorageService::LinkConfig(2, 3, 100, 10, 0.0, 0.0, 10));

    result.simulation_stats.sent_count = 10;
    result.simulation_stats.delivered_count = 10;
    result.simulation_stats.dropped_count = 0;
    result.simulation_stats.corrupted_count = 0;
    result.simulation_stats.heartbeat_timeout_count = 1;

    result.transfer_stats.sent_count = 5;
    result.transfer_stats.acknowledged_count = 5;
    result.transfer_stats.retransmitted_count = 0;
    result.transfer_stats.timeout_count = 0;
    result.transfer_stats.failed_count = 0;

    result.route_changes.push_back("Tick 50: Route updated via Router1");
    result.packet_events.push_back("Tick 0: Data Packet 0 injected");
    result.packet_events.push_back("Tick 20: ACK 0 received");

    // Save experiment result
    storage.save_experiment_result(temp_exp_file, result);

    // Load experiment result
    pbl::services::FileStorageService::ExperimentResult loaded = storage.load_experiment_result(temp_exp_file);

    TEST_ASSERT(loaded.experiment_id == "EXP-20261028-001", "Loaded experiment ID matches");
    TEST_ASSERT(loaded.outcome == "SUCCESS", "Loaded outcome matches");
    TEST_ASSERT(loaded.total_ticks == 425, "Loaded total ticks matches");
    TEST_ASSERT(loaded.simulation_stats.sent_count == 10, "Sim stats sent matches");
    TEST_ASSERT(loaded.simulation_stats.delivered_count == 10, "Sim stats delivered matches");
    TEST_ASSERT(loaded.simulation_stats.heartbeat_timeout_count == 1, "Sim stats heartbeat timeout matches");
    TEST_ASSERT(loaded.transfer_stats.acknowledged_count == 5, "Transfer stats acknowledged count matches");
    TEST_ASSERT(loaded.transfer_stats.retransmitted_count == 0, "Transfer stats retransmitted matches");
    TEST_ASSERT(loaded.route_changes.size() == 1, "Route changes count matches");
    TEST_ASSERT(loaded.route_changes[0] == "Tick 50: Route updated via Router1", "Route change log matches");
    TEST_ASSERT(loaded.packet_events.size() == 2, "Packet events count matches");
    TEST_ASSERT(loaded.packet_events[1] == "Tick 20: ACK 0 received", "Packet event log matches");
    TEST_ASSERT(loaded.config.nodes.size() == 3, "Config nodes preserved in experiment result");
    TEST_ASSERT(loaded.config.links.size() == 2, "Config links preserved in experiment result");

    // Clean up temporary file
    remove(temp_exp_file.c_str());
}

void test_invalid_topology_rejection() {
    pbl::services::FileStorageService storage;

    // Config references link with non-existent node ID 99
    pbl::services::FileStorageService::SimulationConfig bad_config;
    bad_config.nodes.push_back(pbl::services::FileStorageService::NodeConfig(1, "Client1", "Client"));
    bad_config.nodes.push_back(pbl::services::FileStorageService::NodeConfig(2, "Client2", "Client"));
    bad_config.links.push_back(pbl::services::FileStorageService::LinkConfig(1, 99, 100, 10, 0.0, 0.0, 10));

    pbl::model::NetworkGraph graph;
    bool threw = false;
    try {
        storage.apply_config_to_graph(bad_config, graph);
    } catch (const runtime_error& e) {
        threw = true;
    }
    TEST_ASSERT(threw, "Dangling link referencing missing node ID 99 rejected");

    // Loading non-existent file
    bool threw_file = false;
    try {
        storage.load_config("non_existent_file_definitely_not_here_12345.json");
    } catch (const runtime_error& e) {
        threw_file = true;
    }
    TEST_ASSERT(threw_file, "Loading non-existent file throws descriptive runtime_error");
}

int main() {
    cout << "=== Running PBL2 Milestone 10 Storage Tests ===" << endl;

    test_json_primitives_and_nesting();
    test_json_malformed_rejection();
    test_config_save_load_and_graph_application();
    test_experiment_result_persistence();
    test_invalid_topology_rejection();

    cout << "Tests run: " << tests_run << ", Passed: " << tests_passed << endl;
    if (tests_run == tests_passed) {
        cout << "[ALL STORAGE TESTS PASSED]" << endl;
        return 0;
    } else {
        cout << "[STORAGE TESTS FAILED]" << endl;
        return 1;
    }
}
