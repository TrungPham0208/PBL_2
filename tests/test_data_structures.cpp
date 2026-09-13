#include <bits/stdc++.h>

#include "pbl/dsa/AdjacencyList.h"
#include "pbl/dsa/DynamicArray.h"
#include "pbl/dsa/HashMap.h"
#include "pbl/dsa/PriorityQueue.h"
#include "pbl/dsa/Queue.h"

using namespace std;

namespace {

void test_dynamic_array() {
    pbl::dsa::DynamicArray<int> values;
    values.push_back(4);
    values.push_back(8);
    assert(values.size() == 2);
    assert(values.at(1) == 8);
    assert(values[0] == 4);
    values[0] = 40;
    assert(values.at(0) == 40);
    values.pop_back();
    assert(values.size() == 1);
    bool rejected = false;
    try {
        values.at(1);
    } catch (const out_of_range&) {
        rejected = true;
    }
    assert(rejected);
}

void test_queue() {
    pbl::dsa::Queue<int> queue(2);
    queue.push(10);
    queue.push(20);
    assert(queue.front() == 10);
    assert(queue.pop() == 10);
    queue.push(30);
    assert(queue.pop() == 20);
    assert(queue.pop() == 30);
    bool rejected = false;
    try {
        queue.pop();
    } catch (const out_of_range&) {
        rejected = true;
    }
    assert(rejected);
}

void test_priority_queue() {
    pbl::dsa::PriorityQueue<string> queue;
    queue.push("data", 1);
    queue.push("ack-1", 3);
    queue.push("ack-2", 3);
    queue.push("probe", 2);
    assert(queue.pop() == "ack-1");
    assert(queue.pop() == "ack-2");
    assert(queue.pop() == "probe");
    assert(queue.pop() == "data");
}

void test_hash_map() {
    pbl::dsa::HashMap<string, int> map(2);
    map.put("alpha", 1);
    map.put("alpha", 2);
    map.put("beta", 3);
    assert(map.size() == 2);
    assert(map.at("alpha") == 2);
    assert(map.contains("beta"));
    assert(map.remove("alpha"));
    assert(!map.contains("alpha"));
    assert(!map.remove("missing"));
}

void test_adjacency_list() {
    pbl::dsa::AdjacencyList<int, int> graph;
    graph.add_node(1);
    graph.add_undirected_edge(1, 2, 50);
    graph.add_undirected_edge(2, 3, 75);
    assert(graph.node_count() == 3);
    assert(graph.has_edge(1, 2));
    assert(graph.has_edge(2, 1));
    assert(graph.neighbors(2).size() == 2);
    assert(graph.remove_undirected_edge(1, 2));
    assert(!graph.has_edge(1, 2));
    assert(!graph.has_edge(2, 1));
    assert(graph.remove_node(3));
    assert(!graph.has_node(3));
}

}  // namespace

int main() {
    test_dynamic_array();
    test_queue();
    test_priority_queue();
    test_hash_map();
    test_adjacency_list();
    return 0;
}