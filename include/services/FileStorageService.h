#pragma once

#include <bits/stdc++.h>

#include "model/Client.h"
#include "model/Link.h"
#include "model/NetworkGraph.h"
#include "model/Router.h"
#include "pbl/dsa/DynamicArray.h"
#include "services/SimulationService.h"
#include "services/TransferService.h"

using namespace std;

namespace pbl {
namespace services {

// Milestone 10: Core logic — JSON configuration and experiment persistence
// FileStorageService provides local, file-based JSON persistence for network topology,
// experiment configuration, and simulation results.
// No external database, credentials, or paid third-party services are used.
// Invalid or incomplete JSON files are strictly rejected with descriptive runtime errors.
class FileStorageService {
public:
    // Representation of a node configuration in JSON
    struct NodeConfig {
        int id;
        string name;
        string type; // "Client" or "Router"

        NodeConfig() {
            id = -1;
            name = "";
            type = "Client";
        }

        NodeConfig(int node_id, string node_name, string node_type) {
            id = node_id;
            name = node_name;
            type = node_type;
        }
    };

    // Representation of an undirected link configuration in JSON
    struct LinkConfig {
        int node_a;
        int node_b;
        int bandwidth;
        int latency;
        double loss_probability;
        double corruption_probability;
        int queue_capacity;

        LinkConfig() {
            node_a = -1;
            node_b = -1;
            bandwidth = 100;
            latency = 10;
            loss_probability = 0.0;
            corruption_probability = 0.0;
            queue_capacity = 10;
        }

        LinkConfig(int a, int b, int bw, int lat, double loss, double corr, int q_cap) {
            node_a = a;
            node_b = b;
            bandwidth = bw;
            latency = lat;
            loss_probability = loss;
            corruption_probability = corr;
            queue_capacity = q_cap;
        }
    };

    // Complete simulation and topology configuration matching config.json schema
    struct SimulationConfig {
        int random_seed;
        int chunk_size;
        int heartbeat_interval;
        int failure_timeout;
        int source_client_id;
        int destination_client_id;
        dsa::DynamicArray<NodeConfig> nodes;
        dsa::DynamicArray<LinkConfig> links;

        SimulationConfig() {
            random_seed = 12345;
            chunk_size = 1024;
            heartbeat_interval = 100;
            failure_timeout = 300;
            source_client_id = -1;
            destination_client_id = -1;
        }
    };

    // Experiment result representation for persistence in data/experiments/
    struct ExperimentResult {
        string experiment_id;
        string outcome; // "SUCCESS" or "FAILURE"
        int total_ticks;
        SimulationConfig config;
        SimulationService::Statistics simulation_stats;
        TransferService::TransferStatistics transfer_stats;
        dsa::DynamicArray<string> route_changes;
        dsa::DynamicArray<string> packet_events;

        ExperimentResult() {
            experiment_id = "";
            outcome = "UNKNOWN";
            total_ticks = 0;
        }
    };

    // Lightweight internal JSON representation
    enum class JsonType {
        JSON_NULL,
        JSON_BOOL,
        JSON_NUMBER,
        JSON_STRING,
        JSON_ARRAY,
        JSON_OBJECT
    };

    struct JsonValue {
        JsonType type;
        bool bool_value;
        double number_value;
        string string_value;
        dsa::DynamicArray<JsonValue> array_value;
        dsa::DynamicArray<pair<string, JsonValue>> object_value;

        JsonValue() {
            type = JsonType::JSON_NULL;
            bool_value = false;
            number_value = 0.0;
            string_value = "";
        }

        bool has_key(const string& key) const;
        const JsonValue& get(const string& key) const;
        int as_int() const;
        double as_double() const;
        string as_string() const;
        bool as_bool() const;
    };

    FileStorageService();
    // FileStorageService does not own dynamically allocated heap memory,
    // so no custom destructor is required.

    // JSON String Parsing and Serialization
    // Invariant: Invalid or incomplete JSON is rejected clearly rather than silently producing unsafe defaults.
    JsonValue parse_json(const string& json_str) const;
    string serialize_json(const JsonValue& value, int indent_level = 0) const;

    // File-based configuration operations
    void save_config(const string& file_path, const SimulationConfig& config) const;
    SimulationConfig load_config(const string& file_path) const;

    // Topology extraction and application helpers
    void apply_config_to_graph(const SimulationConfig& config, model::NetworkGraph& graph) const;
    SimulationConfig extract_config_from_graph(const model::NetworkGraph& graph,
                                              int random_seed = 12345,
                                              int chunk_size = 1024,
                                              int source_id = -1,
                                              int destination_id = -1) const;

    // Experiment result persistence
    // Invariant: Saved results describe what happened without changing simulation behavior.
    void save_experiment_result(const string& file_path, const ExperimentResult& result) const;
    ExperimentResult load_experiment_result(const string& file_path) const;

private:
    // Internal recursive descent JSON parsing helpers
    JsonValue parse_value(const string& s, int& pos) const;
    JsonValue parse_object(const string& s, int& pos) const;
    JsonValue parse_array(const string& s, int& pos) const;
    string parse_string(const string& s, int& pos) const;
    JsonValue parse_number(const string& s, int& pos) const;
    void skip_whitespace(const string& s, int& pos) const;

    // Helper to format string with JSON escaping
    string escape_string(const string& s) const;

    // Convert between domain structures and JsonValue
    JsonValue config_to_json(const SimulationConfig& config) const;
    SimulationConfig json_to_config(const JsonValue& json) const;
    JsonValue experiment_to_json(const ExperimentResult& result) const;
    ExperimentResult json_to_experiment(const JsonValue& json) const;
};

}
}
