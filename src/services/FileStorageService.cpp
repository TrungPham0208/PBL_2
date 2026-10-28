#include <bits/stdc++.h>

#include "services/FileStorageService.h"

using namespace std;

namespace pbl {
namespace services {

// Milestone 10: Core logic — JSON configuration and experiment persistence
// Normalizing constructor
FileStorageService::FileStorageService() {
}

// JsonValue helper implementations
bool FileStorageService::JsonValue::has_key(const string& key) const {
    if (type != JsonType::JSON_OBJECT) {
        return false;
    }
    for (int i = 0; i < object_value.size(); ++i) {
        if (object_value[i].first == key) {
            return true;
        }
    }
    return false;
}

const FileStorageService::JsonValue& FileStorageService::JsonValue::get(const string& key) const {
    if (type != JsonType::JSON_OBJECT) {
        throw runtime_error("JSON value is not an object");
    }
    for (int i = 0; i < object_value.size(); ++i) {
        if (object_value[i].first == key) {
            return object_value[i].second;
        }
    }
    throw runtime_error("key not found in JSON object: " + key);
}

int FileStorageService::JsonValue::as_int() const {
    if (type != JsonType::JSON_NUMBER) {
        throw runtime_error("expected JSON number for integer conversion");
    }
    return int(number_value);
}

double FileStorageService::JsonValue::as_double() const {
    if (type != JsonType::JSON_NUMBER) {
        throw runtime_error("expected JSON number for double conversion");
    }
    return number_value;
}

string FileStorageService::JsonValue::as_string() const {
    if (type != JsonType::JSON_STRING) {
        throw runtime_error("expected JSON string");
    }
    return string_value;
}

bool FileStorageService::JsonValue::as_bool() const {
    if (type != JsonType::JSON_BOOL) {
        throw runtime_error("expected JSON boolean");
    }
    return bool_value;
}

void FileStorageService::skip_whitespace(const string& s, int& pos) const {
    int len = int(s.size());
    while (pos < len && (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\r' || s[pos] == '\n')) {
        pos++;
    }
}

string FileStorageService::escape_string(const string& s) const {
    string out = "";
    int len = int(s.size());
    for (int i = 0; i < len; ++i) {
        char c = s[i];
        if (c == '"') {
            out += "\\\"";
        } else if (c == '\\') {
            out += "\\\\";
        } else if (c == '\n') {
            out += "\\n";
        } else if (c == '\r') {
            out += "\\r";
        } else if (c == '\t') {
            out += "\\t";
        } else {
            out += c;
        }
    }
    return out;
}

string FileStorageService::parse_string(const string& s, int& pos) const {
    int len = int(s.size());
    if (pos >= len || s[pos] != '"') {
        throw runtime_error("expected '\"' to begin JSON string");
    }
    pos++; // consume opening quote
    string result = "";
    while (pos < len) {
        char c = s[pos++];
        if (c == '"') {
            return result; // closing quote
        }
        if (c == '\\') {
            if (pos >= len) {
                throw runtime_error("unexpected end of escaped JSON string");
            }
            char esc = s[pos++];
            if (esc == '"') result += '"';
            else if (esc == '\\') result += '\\';
            else if (esc == '/') result += '/';
            else if (esc == 'n') result += '\n';
            else if (esc == 'r') result += '\r';
            else if (esc == 't') result += '\t';
            else result += esc;
        } else {
            result += c;
        }
    }
    throw runtime_error("unterminated JSON string");
}

FileStorageService::JsonValue FileStorageService::parse_number(const string& s, int& pos) const {
    int start = pos;
    int len = int(s.size());
    if (pos < len && (s[pos] == '-' || s[pos] == '+')) {
        pos++;
    }
    while (pos < len && (s[pos] >= '0' && s[pos] <= '9')) {
        pos++;
    }
    if (pos < len && s[pos] == '.') {
        pos++;
        while (pos < len && (s[pos] >= '0' && s[pos] <= '9')) {
            pos++;
        }
    }
    if (pos < len && (s[pos] == 'e' || s[pos] == 'E')) {
        pos++;
        if (pos < len && (s[pos] == '-' || s[pos] == '+')) {
            pos++;
        }
        while (pos < len && (s[pos] >= '0' && s[pos] <= '9')) {
            pos++;
        }
    }

    if (start == pos) {
        throw runtime_error("invalid number format in JSON");
    }

    string num_str = s.substr(start, pos - start);
    JsonValue val;
    val.type = JsonType::JSON_NUMBER;
    try {
        val.number_value = stod(num_str);
    } catch (...) {
        throw runtime_error("failed to parse number in JSON: " + num_str);
    }
    return val;
}

FileStorageService::JsonValue FileStorageService::parse_array(const string& s, int& pos) const {
    int len = int(s.size());
    if (pos >= len || s[pos] != '[') {
        throw runtime_error("expected '[' for JSON array");
    }
    pos++; // consume '['
    skip_whitespace(s, pos);

    JsonValue val;
    val.type = JsonType::JSON_ARRAY;

    if (pos < len && s[pos] == ']') {
        pos++; // empty array
        return val;
    }

    while (pos < len) {
        val.array_value.push_back(parse_value(s, pos));
        skip_whitespace(s, pos);
        if (pos >= len) {
            throw runtime_error("unterminated JSON array");
        }
        if (s[pos] == ',') {
            pos++; // consume comma
            skip_whitespace(s, pos);
            // Invariant: Trailing comma in array is invalid JSON
            if (pos < len && s[pos] == ']') {
                throw runtime_error("trailing comma in JSON array");
            }
        } else if (s[pos] == ']') {
            pos++; // consume ']'
            return val;
        } else {
            throw runtime_error("expected ',' or ']' in JSON array");
        }
    }

    throw runtime_error("unterminated JSON array");
}

FileStorageService::JsonValue FileStorageService::parse_object(const string& s, int& pos) const {
    int len = int(s.size());
    if (pos >= len || s[pos] != '{') {
        throw runtime_error("expected '{' for JSON object");
    }
    pos++; // consume '{'
    skip_whitespace(s, pos);

    JsonValue val;
    val.type = JsonType::JSON_OBJECT;

    if (pos < len && s[pos] == '}') {
        pos++; // empty object
        return val;
    }

    while (pos < len) {
        skip_whitespace(s, pos);
        if (pos >= len || s[pos] != '"') {
            throw runtime_error("expected string key in JSON object");
        }
        string key = parse_string(s, pos);
        skip_whitespace(s, pos);

        if (pos >= len || s[pos] != ':') {
            throw runtime_error("expected ':' after key in JSON object");
        }
        pos++; // consume ':'
        skip_whitespace(s, pos);

        JsonValue child = parse_value(s, pos);
        val.object_value.push_back(make_pair(key, child));

        skip_whitespace(s, pos);
        if (pos >= len) {
            throw runtime_error("unterminated JSON object");
        }
        if (s[pos] == ',') {
            pos++; // consume comma
            skip_whitespace(s, pos);
            // Invariant: Trailing comma in object is invalid JSON
            if (pos < len && s[pos] == '}') {
                throw runtime_error("trailing comma in JSON object");
            }
        } else if (s[pos] == '}') {
            pos++; // consume '}'
            return val;
        } else {
            throw runtime_error("expected ',' or '}' in JSON object");
        }
    }

    throw runtime_error("unterminated JSON object");
}

FileStorageService::JsonValue FileStorageService::parse_value(const string& s, int& pos) const {
    skip_whitespace(s, pos);
    int len = int(s.size());
    if (pos >= len) {
        throw runtime_error("unexpected end of JSON input");
    }

    char c = s[pos];
    if (c == '{') {
        return parse_object(s, pos);
    }
    if (c == '[') {
        return parse_array(s, pos);
    }
    if (c == '"') {
        JsonValue val;
        val.type = JsonType::JSON_STRING;
        val.string_value = parse_string(s, pos);
        return val;
    }
    if (c == 't' || c == 'T') {
        if (pos + 4 <= len && s.substr(pos, 4) == "true") {
            pos += 4;
            JsonValue val;
            val.type = JsonType::JSON_BOOL;
            val.bool_value = true;
            return val;
        }
        throw runtime_error("invalid token in JSON");
    }
    if (c == 'f' || c == 'F') {
        if (pos + 5 <= len && s.substr(pos, 5) == "false") {
            pos += 5;
            JsonValue val;
            val.type = JsonType::JSON_BOOL;
            val.bool_value = false;
            return val;
        }
        throw runtime_error("invalid token in JSON");
    }
    if (c == 'n' || c == 'N') {
        if (pos + 4 <= len && s.substr(pos, 4) == "null") {
            pos += 4;
            JsonValue val;
            val.type = JsonType::JSON_NULL;
            return val;
        }
        throw runtime_error("invalid token in JSON");
    }
    if (c == '-' || (c >= '0' && c <= '9')) {
        return parse_number(s, pos);
    }

    throw runtime_error(string("unexpected character in JSON input: '") + c + "'");
}

FileStorageService::JsonValue FileStorageService::parse_json(const string& json_str) const {
    int pos = 0;
    JsonValue root = parse_value(json_str, pos);
    skip_whitespace(json_str, pos);
    if (pos < int(json_str.size())) {
        throw runtime_error("extra characters found after JSON root value");
    }
    return root;
}

string FileStorageService::serialize_json(const JsonValue& value, int indent_level) const {
    string indent = string(indent_level * 2, ' ');
    string next_indent = string((indent_level + 1) * 2, ' ');

    if (value.type == JsonType::JSON_NULL) {
        return "null";
    }
    if (value.type == JsonType::JSON_BOOL) {
        return value.bool_value ? "true" : "false";
    }
    if (value.type == JsonType::JSON_NUMBER) {
        // Output clean integers or floating point numbers
        if (value.number_value == floor(value.number_value)) {
            return to_string(static_cast<long long>(value.number_value));
        }
        ostringstream ss;
        ss << setprecision(6) << value.number_value;
        return ss.str();
    }
    if (value.type == JsonType::JSON_STRING) {
        return "\"" + escape_string(value.string_value) + "\"";
    }
    if (value.type == JsonType::JSON_ARRAY) {
        if (value.array_value.empty()) {
            return "[]";
        }
        string out = "[\n";
        for (int i = 0; i < value.array_value.size(); ++i) {
            out += next_indent + serialize_json(value.array_value[i], indent_level + 1);
            if (i + 1 < value.array_value.size()) {
                out += ",";
            }
            out += "\n";
        }
        out += indent + "]";
        return out;
    }
    if (value.type == JsonType::JSON_OBJECT) {
        if (value.object_value.empty()) {
            return "{}";
        }
        string out = "{\n";
        for (int i = 0; i < value.object_value.size(); ++i) {
            out += next_indent + "\"" + escape_string(value.object_value[i].first) + "\": " +
                   serialize_json(value.object_value[i].second, indent_level + 1);
            if (i + 1 < value.object_value.size()) {
                out += ",";
            }
            out += "\n";
        }
        out += indent + "}";
        return out;
    }
    return "null";
}

FileStorageService::JsonValue FileStorageService::config_to_json(const SimulationConfig& config) const {
    JsonValue root;
    root.type = JsonType::JSON_OBJECT;

    JsonValue val_seed; val_seed.type = JsonType::JSON_NUMBER; val_seed.number_value = config.random_seed;
    root.object_value.push_back(make_pair("random_seed", val_seed));

    JsonValue val_chunk; val_chunk.type = JsonType::JSON_NUMBER; val_chunk.number_value = config.chunk_size;
    root.object_value.push_back(make_pair("chunk_size", val_chunk));

    JsonValue val_hb; val_hb.type = JsonType::JSON_NUMBER; val_hb.number_value = config.heartbeat_interval;
    root.object_value.push_back(make_pair("heartbeat_interval", val_hb));

    JsonValue val_to; val_to.type = JsonType::JSON_NUMBER; val_to.number_value = config.failure_timeout;
    root.object_value.push_back(make_pair("failure_timeout", val_to));

    JsonValue val_src; val_src.type = JsonType::JSON_NUMBER; val_src.number_value = config.source_client_id;
    root.object_value.push_back(make_pair("source_client_id", val_src));

    JsonValue val_dst; val_dst.type = JsonType::JSON_NUMBER; val_dst.number_value = config.destination_client_id;
    root.object_value.push_back(make_pair("destination_client_id", val_dst));

    // Nodes array
    JsonValue nodes_arr;
    nodes_arr.type = JsonType::JSON_ARRAY;
    for (int i = 0; i < config.nodes.size(); ++i) {
        JsonValue node_obj;
        node_obj.type = JsonType::JSON_OBJECT;

        JsonValue nid; nid.type = JsonType::JSON_NUMBER; nid.number_value = config.nodes[i].id;
        JsonValue nname; nname.type = JsonType::JSON_STRING; nname.string_value = config.nodes[i].name;
        JsonValue ntype; ntype.type = JsonType::JSON_STRING; ntype.string_value = config.nodes[i].type;

        node_obj.object_value.push_back(make_pair("id", nid));
        node_obj.object_value.push_back(make_pair("name", nname));
        node_obj.object_value.push_back(make_pair("type", ntype));

        nodes_arr.array_value.push_back(node_obj);
    }
    root.object_value.push_back(make_pair("nodes", nodes_arr));

    // Links array
    JsonValue links_arr;
    links_arr.type = JsonType::JSON_ARRAY;
    for (int i = 0; i < config.links.size(); ++i) {
        JsonValue link_obj;
        link_obj.type = JsonType::JSON_OBJECT;

        JsonValue na; na.type = JsonType::JSON_NUMBER; na.number_value = config.links[i].node_a;
        JsonValue nb; nb.type = JsonType::JSON_NUMBER; nb.number_value = config.links[i].node_b;
        JsonValue bw; bw.type = JsonType::JSON_NUMBER; bw.number_value = config.links[i].bandwidth;
        JsonValue lat; lat.type = JsonType::JSON_NUMBER; lat.number_value = config.links[i].latency;
        JsonValue loss; loss.type = JsonType::JSON_NUMBER; loss.number_value = config.links[i].loss_probability;
        JsonValue corr; corr.type = JsonType::JSON_NUMBER; corr.number_value = config.links[i].corruption_probability;
        JsonValue cap; cap.type = JsonType::JSON_NUMBER; cap.number_value = config.links[i].queue_capacity;

        link_obj.object_value.push_back(make_pair("node_a", na));
        link_obj.object_value.push_back(make_pair("node_b", nb));
        link_obj.object_value.push_back(make_pair("bandwidth", bw));
        link_obj.object_value.push_back(make_pair("latency", lat));
        link_obj.object_value.push_back(make_pair("loss_probability", loss));
        link_obj.object_value.push_back(make_pair("corruption_probability", corr));
        link_obj.object_value.push_back(make_pair("queue_capacity", cap));

        links_arr.array_value.push_back(link_obj);
    }
    root.object_value.push_back(make_pair("links", links_arr));

    return root;
}

FileStorageService::SimulationConfig FileStorageService::json_to_config(const JsonValue& json) const {
    // Invariant: Invalid or incomplete JSON is rejected clearly rather than silently producing unsafe defaults.
    if (!json.has_key("nodes") || !json.has_key("links")) {
        throw runtime_error("incomplete JSON configuration: missing required 'nodes' or 'links' field");
    }

    SimulationConfig config;
    if (json.has_key("random_seed")) config.random_seed = json.get("random_seed").as_int();
    if (json.has_key("chunk_size")) config.chunk_size = json.get("chunk_size").as_int();
    if (json.has_key("heartbeat_interval")) config.heartbeat_interval = json.get("heartbeat_interval").as_int();
    if (json.has_key("failure_timeout")) config.failure_timeout = json.get("failure_timeout").as_int();
    if (json.has_key("source_client_id")) config.source_client_id = json.get("source_client_id").as_int();
    if (json.has_key("destination_client_id")) config.destination_client_id = json.get("destination_client_id").as_int();

    // Parse nodes
    const JsonValue& nodes_val = json.get("nodes");
    if (nodes_val.type != JsonType::JSON_ARRAY) {
        throw runtime_error("invalid JSON configuration: 'nodes' must be an array");
    }
    for (int i = 0; i < nodes_val.array_value.size(); ++i) {
        const JsonValue& n = nodes_val.array_value[i];
        if (!n.has_key("id") || !n.has_key("name") || !n.has_key("type")) {
            throw runtime_error("incomplete node record in JSON configuration");
        }
        NodeConfig nc(n.get("id").as_int(), n.get("name").as_string(), n.get("type").as_string());
        config.nodes.push_back(nc);
    }

    // Parse links
    const JsonValue& links_val = json.get("links");
    if (links_val.type != JsonType::JSON_ARRAY) {
        throw runtime_error("invalid JSON configuration: 'links' must be an array");
    }
    for (int i = 0; i < links_val.array_value.size(); ++i) {
        const JsonValue& l = links_val.array_value[i];
        if (!l.has_key("node_a") || !l.has_key("node_b") || !l.has_key("bandwidth") || !l.has_key("latency")) {
            throw runtime_error("incomplete link record in JSON configuration");
        }
        int na = l.get("node_a").as_int();
        int nb = l.get("node_b").as_int();
        int bw = l.get("bandwidth").as_int();
        int lat = l.get("latency").as_int();
        double loss = l.has_key("loss_probability") ? l.get("loss_probability").as_double() : 0.0;
        double corr = l.has_key("corruption_probability") ? l.get("corruption_probability").as_double() : 0.0;
        int cap = l.has_key("queue_capacity") ? l.get("queue_capacity").as_int() : 10;

        // Validation
        if (bw <= 0 || lat < 0 || cap <= 0 || loss < 0.0 || loss > 1.0 || corr < 0.0 || corr > 1.0) {
            throw runtime_error("invalid link parameter values in JSON configuration");
        }

        LinkConfig lc(na, nb, bw, lat, loss, corr, cap);
        config.links.push_back(lc);
    }

    return config;
}

FileStorageService::JsonValue FileStorageService::experiment_to_json(const ExperimentResult& result) const {
    JsonValue root;
    root.type = JsonType::JSON_OBJECT;

    JsonValue exp_id; exp_id.type = JsonType::JSON_STRING; exp_id.string_value = result.experiment_id;
    root.object_value.push_back(make_pair("experiment_id", exp_id));

    JsonValue outcome; outcome.type = JsonType::JSON_STRING; outcome.string_value = result.outcome;
    root.object_value.push_back(make_pair("outcome", outcome));

    JsonValue ticks; ticks.type = JsonType::JSON_NUMBER; ticks.number_value = result.total_ticks;
    root.object_value.push_back(make_pair("total_ticks", ticks));

    // Configuration sub-object
    root.object_value.push_back(make_pair("configuration", config_to_json(result.config)));

    // Statistics sub-object
    JsonValue stats_obj;
    stats_obj.type = JsonType::JSON_OBJECT;
    auto add_num = [&](const string& name, double val) {
        JsonValue v; v.type = JsonType::JSON_NUMBER; v.number_value = val;
        stats_obj.object_value.push_back(make_pair(name, v));
    };

    add_num("sent_count", result.simulation_stats.sent_count);
    add_num("delivered_count", result.simulation_stats.delivered_count);
    add_num("dropped_count", result.simulation_stats.dropped_count);
    add_num("queue_overflow_count", result.simulation_stats.queue_overflow_count);
    add_num("failure_drop_count", result.simulation_stats.failure_drop_count);
    add_num("lost_count", result.simulation_stats.lost_count);
    add_num("corrupted_count", result.simulation_stats.corrupted_count);
    add_num("manual_failure_count", result.simulation_stats.manual_failure_count);
    add_num("heartbeat_timeout_count", result.simulation_stats.heartbeat_timeout_count);
    add_num("transfer_acknowledged_count", result.transfer_stats.acknowledged_count);
    add_num("transfer_retransmitted_count", result.transfer_stats.retransmitted_count);
    add_num("transfer_timeout_count", result.transfer_stats.timeout_count);
    add_num("transfer_failed_count", result.transfer_stats.failed_count);

    root.object_value.push_back(make_pair("statistics", stats_obj));

    // Route changes array
    JsonValue routes_arr;
    routes_arr.type = JsonType::JSON_ARRAY;
    for (int i = 0; i < result.route_changes.size(); ++i) {
        JsonValue r; r.type = JsonType::JSON_STRING; r.string_value = result.route_changes[i];
        routes_arr.array_value.push_back(r);
    }
    root.object_value.push_back(make_pair("route_changes", routes_arr));

    // Packet events array
    JsonValue events_arr;
    events_arr.type = JsonType::JSON_ARRAY;
    for (int i = 0; i < result.packet_events.size(); ++i) {
        JsonValue e; e.type = JsonType::JSON_STRING; e.string_value = result.packet_events[i];
        events_arr.array_value.push_back(e);
    }
    root.object_value.push_back(make_pair("packet_events", events_arr));

    return root;
}

FileStorageService::ExperimentResult FileStorageService::json_to_experiment(const JsonValue& json) const {
    if (!json.has_key("experiment_id") || !json.has_key("outcome") || !json.has_key("statistics")) {
        throw runtime_error("incomplete experiment JSON: missing required fields");
    }

    ExperimentResult result;
    result.experiment_id = json.get("experiment_id").as_string();
    result.outcome = json.get("outcome").as_string();
    if (json.has_key("total_ticks")) {
        result.total_ticks = json.get("total_ticks").as_int();
    }

    if (json.has_key("configuration")) {
        result.config = json_to_config(json.get("configuration"));
    }

    // Statistics
    const JsonValue& st = json.get("statistics");
    if (st.has_key("sent_count")) result.simulation_stats.sent_count = st.get("sent_count").as_int();
    if (st.has_key("delivered_count")) result.simulation_stats.delivered_count = st.get("delivered_count").as_int();
    if (st.has_key("dropped_count")) result.simulation_stats.dropped_count = st.get("dropped_count").as_int();
    if (st.has_key("queue_overflow_count")) result.simulation_stats.queue_overflow_count = st.get("queue_overflow_count").as_int();
    if (st.has_key("failure_drop_count")) result.simulation_stats.failure_drop_count = st.get("failure_drop_count").as_int();
    if (st.has_key("lost_count")) result.simulation_stats.lost_count = st.get("lost_count").as_int();
    if (st.has_key("corrupted_count")) result.simulation_stats.corrupted_count = st.get("corrupted_count").as_int();
    if (st.has_key("manual_failure_count")) result.simulation_stats.manual_failure_count = st.get("manual_failure_count").as_int();
    if (st.has_key("heartbeat_timeout_count")) result.simulation_stats.heartbeat_timeout_count = st.get("heartbeat_timeout_count").as_int();

    if (st.has_key("transfer_acknowledged_count")) result.transfer_stats.acknowledged_count = st.get("transfer_acknowledged_count").as_int();
    if (st.has_key("transfer_retransmitted_count")) result.transfer_stats.retransmitted_count = st.get("transfer_retransmitted_count").as_int();
    if (st.has_key("transfer_timeout_count")) result.transfer_stats.timeout_count = st.get("transfer_timeout_count").as_int();
    if (st.has_key("transfer_failed_count")) result.transfer_stats.failed_count = st.get("transfer_failed_count").as_int();

    if (json.has_key("route_changes")) {
        const JsonValue& rc = json.get("route_changes");
        if (rc.type == JsonType::JSON_ARRAY) {
            for (int i = 0; i < rc.array_value.size(); ++i) {
                result.route_changes.push_back(rc.array_value[i].as_string());
            }
        }
    }

    if (json.has_key("packet_events")) {
        const JsonValue& pe = json.get("packet_events");
        if (pe.type == JsonType::JSON_ARRAY) {
            for (int i = 0; i < pe.array_value.size(); ++i) {
                result.packet_events.push_back(pe.array_value[i].as_string());
            }
        }
    }

    return result;
}

void FileStorageService::save_config(const string& file_path, const SimulationConfig& config) const {
    filesystem::path p(file_path);
    if (p.has_parent_path()) {
        filesystem::create_directories(p.parent_path());
    }

    JsonValue root = config_to_json(config);
    string json_str = serialize_json(root, 0);

    ofstream fout(file_path);
    if (!fout.is_open()) {
        throw runtime_error("cannot open file for writing: " + file_path);
    }
    fout << json_str << "\n";
    fout.close();
}

FileStorageService::SimulationConfig FileStorageService::load_config(const string& file_path) const {
    ifstream fin(file_path);
    if (!fin.is_open()) {
        throw runtime_error("cannot open file for reading: " + file_path);
    }
    stringstream ss;
    ss << fin.rdbuf();
    fin.close();

    string content = ss.str();
    if (content.empty()) {
        throw runtime_error("empty configuration file: " + file_path);
    }

    JsonValue root = parse_json(content);
    return json_to_config(root);
}

void FileStorageService::apply_config_to_graph(const SimulationConfig& config, model::NetworkGraph& graph) const {
    graph.clear();

    // 1. Add nodes
    for (int i = 0; i < config.nodes.size(); ++i) {
        const NodeConfig& nc = config.nodes[i];
        if (nc.type == "Router") {
            graph.add_router(nc.id, nc.name);
        } else {
            graph.add_client(nc.id, nc.name);
        }
    }

    // 2. Add links
    for (int i = 0; i < config.links.size(); ++i) {
        const LinkConfig& lc = config.links[i];
        if (!graph.has_node(lc.node_a) || !graph.has_node(lc.node_b)) {
            throw runtime_error("invalid configuration: link refers to unknown node");
        }
        graph.add_link(lc.node_a, lc.node_b, lc.bandwidth, lc.latency,
                       lc.loss_probability, lc.corruption_probability,
                       lc.queue_capacity);
    }
}

FileStorageService::SimulationConfig
FileStorageService::extract_config_from_graph(const model::NetworkGraph& graph,
                                              int random_seed,
                                              int chunk_size,
                                              int source_id,
                                              int destination_id) const {
    SimulationConfig config;
    config.random_seed = random_seed;
    config.chunk_size = chunk_size;
    config.source_client_id = source_id;
    config.destination_client_id = destination_id;

    const dsa::DynamicArray<int>& node_ids = graph.get_node_ids();
    for (int i = 0; i < node_ids.size(); ++i) {
        int id = node_ids[i];
        const model::Node* n = graph.get_node(id);
        if (n != nullptr) {
            config.nodes.push_back(NodeConfig(id, n->get_name(), n->get_type()));
        }
    }

    const dsa::DynamicArray<model::Link*>& links = graph.get_all_links();
    for (int i = 0; i < links.size(); ++i) {
        const model::Link* l = links[i];
        if (l != nullptr) {
            config.links.push_back(LinkConfig(l->get_first_node_id(),
                                              l->get_second_node_id(),
                                              l->get_bandwidth(),
                                              l->get_latency(),
                                              l->get_loss_probability(),
                                              l->get_corruption_probability(),
                                              l->queue_capacity()));
        }
    }

    return config;
}

void FileStorageService::save_experiment_result(const string& file_path, const ExperimentResult& result) const {
    filesystem::path p(file_path);
    if (p.has_parent_path()) {
        filesystem::create_directories(p.parent_path());
    }

    JsonValue root = experiment_to_json(result);
    string json_str = serialize_json(root, 0);

    ofstream fout(file_path);
    if (!fout.is_open()) {
        throw runtime_error("cannot open file for writing: " + file_path);
    }
    fout << json_str << "\n";
    fout.close();
}

FileStorageService::ExperimentResult FileStorageService::load_experiment_result(const string& file_path) const {
    ifstream fin(file_path);
    if (!fin.is_open()) {
        throw runtime_error("cannot open file for reading: " + file_path);
    }
    stringstream ss;
    ss << fin.rdbuf();
    fin.close();

    string content = ss.str();
    if (content.empty()) {
        throw runtime_error("empty experiment file: " + file_path);
    }

    JsonValue root = parse_json(content);
    return json_to_experiment(root);
}

}
}
