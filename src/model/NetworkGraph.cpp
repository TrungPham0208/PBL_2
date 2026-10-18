#include "model/NetworkGraph.h"

using namespace std;

namespace pbl {
namespace model {

// Milestone 3: Core logic — Network graph representation and topology validation.
NetworkGraph::NetworkGraph() {
    // Normalizing constructor: initialize member state inside constructor body.
    // Memory ownership rule: NetworkGraph owns all Node* and Link* instances added to it.
}

NetworkGraph::~NetworkGraph() {
    // When the network graph is destroyed, clean up all heap-allocated nodes and links.
    clear();
}

void NetworkGraph::clear() {
    // Free all dynamically allocated links
    for (int i = 0; i < links_.size(); ++i) {
        delete links_[i];
    }
    links_.clear();

    // Free all dynamically allocated nodes
    for (int i = 0; i < node_ids_.size(); ++i) {
        int id = node_ids_[i];
        if (nodes_.contains(id)) {
            Node* node_ptr = nodes_.at(id);
            delete node_ptr;
        }
    }
    nodes_.clear();
    node_ids_.clear();

    // Reset adjacency list by replacing with a fresh instance
    adjacency_list_ = dsa::AdjacencyList<int, Link*>();
}

void NetworkGraph::add_node(Node* node) {
    if (node == nullptr) {
        throw invalid_argument("cannot add null node to graph");
    }
    int id = node->get_id();
    if (nodes_.contains(id)) {
        throw invalid_argument("node with duplicate id already exists in graph");
    }
    nodes_.put(id, node);
    node_ids_.push_back(id);
    adjacency_list_.add_node(id);
}

void NetworkGraph::add_client(int id, string name) {
    add_node(new Client(id, name));
}

void NetworkGraph::add_router(int id, string name) {
    add_node(new Router(id, name));
}

bool NetworkGraph::remove_node(int id) {
    if (!nodes_.contains(id)) {
        return false;
    }

    // First collect and remove all links connected to this node to prevent dangling pointers
    dsa::DynamicArray<pair<int, int>> connected_links;
    if (adjacency_list_.has_node(id)) {
        const dsa::DynamicArray<dsa::AdjacencyList<int, Link*>::Edge>& edges = adjacency_list_.neighbors(id);
        for (int i = 0; i < edges.size(); ++i) {
            connected_links.push_back({id, edges[i].destination});
        }
    }

    for (int i = 0; i < connected_links.size(); ++i) {
        remove_link(connected_links[i].first, connected_links[i].second);
    }

    // Remove from adjacency list
    adjacency_list_.remove_node(id);

    // Remove from node IDs array
    for (int i = 0; i < node_ids_.size(); ++i) {
        if (node_ids_[i] == id) {
            node_ids_[i] = node_ids_[node_ids_.size() - 1];
            node_ids_.pop_back();
            break;
        }
    }

    // Delete the heap-allocated node object and remove from HashMap
    Node* node_ptr = nodes_.at(id);
    nodes_.remove(id);
    delete node_ptr;

    return true;
}

bool NetworkGraph::has_node(int id) const {
    return nodes_.contains(id);
}

Node* NetworkGraph::get_node(int id) {
    if (!nodes_.contains(id)) {
        return nullptr;
    }
    return nodes_.at(id);
}

const Node* NetworkGraph::get_node(int id) const {
    if (!nodes_.contains(id)) {
        return nullptr;
    }
    return nodes_.at(id);
}

int NetworkGraph::node_count() const {
    return node_ids_.size();
}

const dsa::DynamicArray<int>& NetworkGraph::get_node_ids() const {
    return node_ids_;
}

dsa::DynamicArray<int> NetworkGraph::get_client_ids() const {
    dsa::DynamicArray<int> clients;
    for (int i = 0; i < node_ids_.size(); ++i) {
        int id = node_ids_[i];
        if (nodes_.contains(id)) {
            const Node* node = nodes_.at(id);
            if (node->get_type() == "Client") {
                clients.push_back(id);
            }
        }
    }
    return clients;
}

dsa::DynamicArray<int> NetworkGraph::get_router_ids() const {
    dsa::DynamicArray<int> routers;
    for (int i = 0; i < node_ids_.size(); ++i) {
        int id = node_ids_[i];
        if (nodes_.contains(id)) {
            const Node* node = nodes_.at(id);
            if (node->get_type() == "Router") {
                routers.push_back(id);
            }
        }
    }
    return routers;
}

bool NetworkGraph::is_node_active(int id) const {
    const Node* node = get_node(id);
    if (node == nullptr) {
        return false;
    }
    return node->is_active();
}

void NetworkGraph::set_node_active(int id, bool active) {
    Node* node = get_node(id);
    if (node == nullptr) {
        throw out_of_range("node not found in graph");
    }
    node->set_active(active);
}

void NetworkGraph::fail_node(int id) {
    set_node_active(id, false);
}

void NetworkGraph::recover_node(int id) {
    set_node_active(id, true);
}

void NetworkGraph::add_link(Link* link) {
    if (link == nullptr) {
        throw invalid_argument("cannot add null link to graph");
    }
    int first = link->get_first_node_id();
    int second = link->get_second_node_id();

    // Invariant: no dangling links
    if (!has_node(first) || !has_node(second)) {
        throw invalid_argument("both endpoint nodes must exist in graph before adding link");
    }

    // Invariant: no duplicate links between same endpoints
    if (has_link(first, second)) {
        throw invalid_argument("link between these nodes already exists");
    }

    links_.push_back(link);
    // AdjacencyList adds undirected edge consistently from both endpoints
    adjacency_list_.add_undirected_edge(first, second, link);
}

void NetworkGraph::add_link(int first_node_id, int second_node_id, int bandwidth,
                           int latency, double loss_probability,
                           double corruption_probability, int queue_capacity) {
    Link* link = new Link(first_node_id, second_node_id, bandwidth, latency,
                          loss_probability, corruption_probability, queue_capacity);
    add_link(link);
}

bool NetworkGraph::remove_link(int first_node_id, int second_node_id) {
    if (!has_link(first_node_id, second_node_id)) {
        return false;
    }

    Link* target = nullptr;
    for (int i = 0; i < links_.size(); ++i) {
        if (links_[i]->connects(first_node_id, second_node_id)) {
            target = links_[i];
            links_[i] = links_[links_.size() - 1];
            links_.pop_back();
            break;
        }
    }

    adjacency_list_.remove_undirected_edge(first_node_id, second_node_id);
    delete target;
    return true;
}

bool NetworkGraph::has_link(int first_node_id, int second_node_id) const {
    if (!has_node(first_node_id) || !has_node(second_node_id)) {
        return false;
    }
    return adjacency_list_.has_edge(first_node_id, second_node_id);
}

Link* NetworkGraph::get_link(int first_node_id, int second_node_id) {
    if (!has_link(first_node_id, second_node_id)) {
        return nullptr;
    }
    const dsa::DynamicArray<dsa::AdjacencyList<int, Link*>::Edge>& edges = adjacency_list_.neighbors(first_node_id);
    for (int i = 0; i < edges.size(); ++i) {
        if (edges[i].destination == second_node_id) {
            return edges[i].value;
        }
    }
    return nullptr;
}

const Link* NetworkGraph::get_link(int first_node_id, int second_node_id) const {
    if (!has_link(first_node_id, second_node_id)) {
        return nullptr;
    }
    const dsa::DynamicArray<dsa::AdjacencyList<int, Link*>::Edge>& edges = adjacency_list_.neighbors(first_node_id);
    for (int i = 0; i < edges.size(); ++i) {
        if (edges[i].destination == second_node_id) {
            return edges[i].value;
        }
    }
    return nullptr;
}

int NetworkGraph::link_count() const {
    return links_.size();
}

const dsa::DynamicArray<Link*>& NetworkGraph::get_all_links() const {
    return links_;
}

bool NetworkGraph::is_link_active(int first_node_id, int second_node_id) const {
    const Link* link = get_link(first_node_id, second_node_id);
    if (link == nullptr) {
        return false;
    }
    return link->is_active();
}

void NetworkGraph::set_link_active(int first_node_id, int second_node_id, bool active) {
    Link* link = get_link(first_node_id, second_node_id);
    if (link == nullptr) {
        throw out_of_range("link not found in graph");
    }
    link->set_active(active);
}

void NetworkGraph::fail_link(int first_node_id, int second_node_id) {
    set_link_active(first_node_id, second_node_id, false);
}

void NetworkGraph::recover_link(int first_node_id, int second_node_id) {
    set_link_active(first_node_id, second_node_id, true);
}

dsa::DynamicArray<int> NetworkGraph::get_neighbors(int node_id) const {
    dsa::DynamicArray<int> neighbor_ids;
    if (!adjacency_list_.has_node(node_id)) {
        return neighbor_ids;
    }
    const dsa::DynamicArray<dsa::AdjacencyList<int, Link*>::Edge>& edges = adjacency_list_.neighbors(node_id);
    for (int i = 0; i < edges.size(); ++i) {
        neighbor_ids.push_back(edges[i].destination);
    }
    return neighbor_ids;
}

dsa::DynamicArray<int> NetworkGraph::get_active_neighbors(int node_id) const {
    dsa::DynamicArray<int> active_neighbors;
    if (!has_node(node_id) || !is_node_active(node_id)) {
        return active_neighbors;
    }
    const dsa::DynamicArray<dsa::AdjacencyList<int, Link*>::Edge>& edges = adjacency_list_.neighbors(node_id);
    for (int i = 0; i < edges.size(); ++i) {
        int neighbor_id = edges[i].destination;
        Link* link = edges[i].value;
        // Invariant: failed nodes and links cannot be treated as usable routing connections
        if (link->is_active() && is_node_active(neighbor_id)) {
            active_neighbors.push_back(neighbor_id);
        }
    }
    return active_neighbors;
}

const dsa::AdjacencyList<int, Link*>& NetworkGraph::get_adjacency_list() const {
    return adjacency_list_;
}

dsa::DynamicArray<int> NetworkGraph::find_path_bfs(int source_id, int destination_id) const {
    dsa::DynamicArray<int> path;
    if (!has_node(source_id) || !has_node(destination_id)) {
        return path;
    }
    if (!is_node_active(source_id) || !is_node_active(destination_id)) {
        return path;
    }
    if (source_id == destination_id) {
        path.push_back(source_id);
        return path;
    }

    int capacity = node_count() + 10;
    dsa::Queue<int> queue(capacity);
    dsa::HashMap<int, int> parent_map;
    dsa::HashMap<int, bool> visited;

    queue.push(source_id);
    visited.put(source_id, true);

    bool reached = false;
    while (!queue.empty()) {
        int current = queue.pop();
        if (current == destination_id) {
            reached = true;
            break;
        }

        dsa::DynamicArray<int> neighbors = get_active_neighbors(current);
        for (int i = 0; i < neighbors.size(); ++i) {
            int next = neighbors[i];
            if (!visited.contains(next)) {
                visited.put(next, true);
                parent_map.put(next, current);
                queue.push(next);
            }
        }
    }

    if (!reached) {
        return path;
    }

    // Reconstruct path from destination to source
    dsa::DynamicArray<int> reversed_path;
    int curr = destination_id;
    while (curr != source_id) {
        reversed_path.push_back(curr);
        curr = parent_map.at(curr);
    }
    reversed_path.push_back(source_id);

    // Reverse into final path
    for (int i = reversed_path.size() - 1; i >= 0; --i) {
        path.push_back(reversed_path[i]);
    }
    return path;
}

bool NetworkGraph::is_reachable(int source_id, int destination_id) const {
    dsa::DynamicArray<int> path = find_path_bfs(source_id, destination_id);
    return !path.empty();
}

bool NetworkGraph::is_reachable_excluding_edge(int source_id, int destination_id,
                                              int excluded_first, int excluded_second) const {
    if (!has_node(source_id) || !has_node(destination_id)) {
        return false;
    }
    if (!is_node_active(source_id) || !is_node_active(destination_id)) {
        return false;
    }
    if (source_id == destination_id) {
        return true;
    }

    int capacity = node_count() + 10;
    dsa::Queue<int> queue(capacity);
    dsa::HashMap<int, bool> visited;

    queue.push(source_id);
    visited.put(source_id, true);

    while (!queue.empty()) {
        int current = queue.pop();
        if (current == destination_id) {
            return true;
        }

        dsa::DynamicArray<int> neighbors = get_active_neighbors(current);
        for (int i = 0; i < neighbors.size(); ++i) {
            int next = neighbors[i];
            // Skip the excluded edge
            if ((current == excluded_first && next == excluded_second) ||
                (current == excluded_second && next == excluded_first)) {
                continue;
            }

            if (!visited.contains(next)) {
                visited.put(next, true);
                queue.push(next);
            }
        }
    }
    return false;
}

bool NetworkGraph::has_alternative_route(int source_id, int destination_id) const {
    dsa::DynamicArray<int> primary_path = find_path_bfs(source_id, destination_id);
    if (primary_path.size() < 2) {
        return false;
    }

    // Check every edge on the primary path. If removing any edge disconnects the pair,
    // that edge is a bridge and no alternative route exists for that failure.
    for (int i = 0; i < primary_path.size() - 1; ++i) {
        int u = primary_path[i];
        int v = primary_path[i + 1];
        if (!is_reachable_excluding_edge(source_id, destination_id, u, v)) {
            return false;
        }
    }
    return true;
}

NetworkGraph::ValidationResult NetworkGraph::validate() const {
    ValidationResult result;
    result.is_valid = true;

    // Check 1: Isolated nodes (nodes with 0 neighbors)
    dsa::DynamicArray<int> isolated = get_isolated_nodes();
    for (int i = 0; i < isolated.size(); ++i) {
        result.isolated_nodes.push_back(isolated[i]);
        Node* node = nodes_.at(isolated[i]);
        result.warnings.push_back("Node " + to_string(isolated[i]) + " (" + node->get_name() + ") is isolated.");
        result.is_valid = false;
    }

    // Check 2: Client reachability and alternative routes
    dsa::DynamicArray<int> clients = get_client_ids();
    if (clients.size() < 2) {
        result.warnings.push_back("Graph contains fewer than two clients for file transfer.");
        result.is_valid = false;
        return result;
    }

    // Pairwise checks for all client pairs
    for (int i = 0; i < clients.size(); ++i) {
        for (int j = i + 1; j < clients.size(); ++j) {
            int c1 = clients[i];
            int c2 = clients[j];

            if (!is_reachable(c1, c2)) {
                result.unreachable_client_pairs.push_back({c1, c2});
                result.warnings.push_back("Client " + to_string(c1) + " cannot reach client " + to_string(c2) + ".");
                result.is_valid = false;
            } else if (!has_alternative_route(c1, c2)) {
                result.missing_alternative_routes.push_back({c1, c2});
                result.warnings.push_back("Missing alternative route between client " + to_string(c1) +
                                          " and client " + to_string(c2) + " (single point of failure).");
                result.is_valid = false;
            }
        }
    }

    return result;
}

bool NetworkGraph::is_valid() const {
    ValidationResult report = validate();
    return report.is_valid;
}

dsa::DynamicArray<int> NetworkGraph::get_isolated_nodes() const {
    dsa::DynamicArray<int> isolated;
    for (int i = 0; i < node_ids_.size(); ++i) {
        int id = node_ids_[i];
        if (get_neighbors(id).size() == 0) {
            isolated.push_back(id);
        }
    }
    return isolated;
}

dsa::DynamicArray<pair<int, int>> NetworkGraph::get_unreachable_client_pairs() const {
    dsa::DynamicArray<pair<int, int>> unreachable;
    dsa::DynamicArray<int> clients = get_client_ids();
    for (int i = 0; i < clients.size(); ++i) {
        for (int j = i + 1; j < clients.size(); ++j) {
            if (!is_reachable(clients[i], clients[j])) {
                unreachable.push_back({clients[i], clients[j]});
            }
        }
    }
    return unreachable;
}

dsa::DynamicArray<pair<int, int>> NetworkGraph::get_missing_alternative_routes() const {
    dsa::DynamicArray<pair<int, int>> missing;
    dsa::DynamicArray<int> clients = get_client_ids();
    for (int i = 0; i < clients.size(); ++i) {
        for (int j = i + 1; j < clients.size(); ++j) {
            if (is_reachable(clients[i], clients[j]) && !has_alternative_route(clients[i], clients[j])) {
                missing.push_back({clients[i], clients[j]});
            }
        }
    }
    return missing;
}

void NetworkGraph::generate_random_topology(int client_count, int router_count,
                                           double connection_density, int seed) {
    if (client_count < 0 || router_count < 0) {
        throw invalid_argument("node counts must not be negative");
    }
    int total_nodes = client_count + router_count;
    if (total_nodes < 2) {
        throw invalid_argument("graph must have at least two nodes");
    }
    if (connection_density < 0.0 || connection_density > 1.0) {
        throw invalid_argument("connection density must be between 0.0 and 1.0");
    }

    clear();

    // Create clients
    for (int i = 0; i < client_count; ++i) {
        add_client(i, "Client_" + to_string(i));
    }

    // Create routers
    for (int i = 0; i < router_count; ++i) {
        int id = client_count + i;
        add_router(id, "Router_" + to_string(id));
    }

    mt19937 rng(seed);

    // Build a connected backbone chain between consecutive nodes if density > 0
    if (connection_density > 0.0) {
        for (int i = 0; i < total_nodes - 1; ++i) {
            add_link(i, i + 1, 100, 5, 0.0, 0.0, 10);
        }
    }

    // Add extra random links according to connection density
    uniform_real_distribution<double> dist(0.0, 1.0);
    for (int i = 0; i < total_nodes; ++i) {
        for (int j = i + 1; j < total_nodes; ++j) {
            if (!has_link(i, j)) {
                if (dist(rng) < connection_density) {
                    add_link(i, j, 100, 5, 0.0, 0.0, 10);
                }
            }
        }
    }
}

}
}
