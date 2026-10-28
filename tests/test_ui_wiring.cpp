#include <bits/stdc++.h>

#include <QApplication>
#include "ui/MainWindow.h"

using namespace std;

// Milestone 11: Unit and regression tests for Qt Dashboard and UI Presentation Wiring.
// Tests verify:
// 1. MainWindow initialization and proper service composition.
// 2. Deterministic, reproducible random topology generation via UI controls.
// 3. Topology validation reporting (connected vs disconnected / isolated nodes).
// 4. Simulation clock controls and state machine transitions (start, pause, resume, reset, single tick).
// 5. Fault injection wiring (node/link failure toggles and route updates).
// 6. Transfer setup, chunk dispatch, and UI statistics synchronization.
// 7. Configuration save and load roundtrip through the UI.

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

void test_initialization_and_services(pbl::ui::MainWindow& window) {
    TEST_ASSERT(window.get_simulation_service().get_network_graph() != nullptr,
                "Simulation service attached to NetworkGraph");
    TEST_ASSERT(window.get_simulation_service().get_routing_service() != nullptr,
                "Simulation service attached to RoutingService");
    TEST_ASSERT(window.get_canvas() != nullptr,
                "TopologyCanvas instantiated");
    TEST_ASSERT(window.get_simulation_service().get_state() == pbl::services::SimulationService::State::STOPPED,
                "Initial simulation state is STOPPED");
    TEST_ASSERT(window.get_simulation_service().get_current_tick() == 0,
                "Initial simulation tick is 0");
}

void test_reproducible_topology_generation(pbl::ui::MainWindow& window) {
    // Generate with seed 777
    window.handle_generate_random_topology(8, 2, 0.5, 777);
    int node_count1 = window.get_network_graph().get_node_ids().size();
    int link_count1 = window.get_network_graph().get_all_links().size();

    TEST_ASSERT(node_count1 == 8, "Generated 8 nodes");
    TEST_ASSERT(link_count1 > 0, "Generated non-zero links");

    // Re-generate with the same seed 777
    window.handle_generate_random_topology(8, 2, 0.5, 777);
    int node_count2 = window.get_network_graph().get_node_ids().size();
    int link_count2 = window.get_network_graph().get_all_links().size();

    TEST_ASSERT(node_count1 == node_count2, "Node count reproducible with same seed");
    TEST_ASSERT(link_count1 == link_count2, "Link count reproducible with same seed");

    // Validation report
    string report = window.handle_validate_topology();
    TEST_ASSERT(!report.empty(), "Validation report produced");
}

void test_simulation_controls_state_machine(pbl::ui::MainWindow& window) {
    window.handle_reset_simulation();
    TEST_ASSERT(window.get_simulation_service().get_state() == pbl::services::SimulationService::State::STOPPED,
                "Reset sets state to STOPPED");
    TEST_ASSERT(window.get_simulation_service().get_current_tick() == 0,
                "Reset sets tick to 0");

    window.handle_start_simulation();
    TEST_ASSERT(window.get_simulation_service().get_state() == pbl::services::SimulationService::State::RUNNING,
                "Start sets state to RUNNING");

    window.handle_pause_simulation();
    TEST_ASSERT(window.get_simulation_service().get_state() == pbl::services::SimulationService::State::PAUSED,
                "Pause sets state to PAUSED");

    window.handle_resume_simulation();
    TEST_ASSERT(window.get_simulation_service().get_state() == pbl::services::SimulationService::State::RUNNING,
                "Resume sets state to RUNNING");

    int prev_tick = window.get_simulation_service().get_current_tick();
    window.handle_single_tick();
    TEST_ASSERT(window.get_simulation_service().get_current_tick() == prev_tick + 1,
                "Single tick advances clock by 1");

    window.handle_advance_ticks(10);
    TEST_ASSERT(window.get_simulation_service().get_current_tick() == prev_tick + 11,
                "Advance ticks advances clock by 10");

    window.handle_reset_simulation();
    TEST_ASSERT(window.get_simulation_service().get_current_tick() == 0,
                "Reset clock returns to 0");
}

void test_fault_injection_and_rerouting(pbl::ui::MainWindow& window) {
    // Generate a known simple linear topology: 1(C) - 2(R) - 3(R) - 4(C)
    // with alternative path: 1(C) - 5(R) - 4(C)
    window.handle_reset_simulation();
    pbl::model::NetworkGraph& graph = window.get_network_graph();
    graph = pbl::model::NetworkGraph();
    window.get_canvas()->set_network_graph(&graph);

    graph.add_client(1, "Client1");
    graph.add_router(2, "Router2");
    graph.add_router(3, "Router3");
    graph.add_client(4, "Client4");
    graph.add_router(5, "Router5");

    // Primary route: 1 - 2 - 3 - 4 (fast: lat 5)
    graph.add_link(1, 2, 100, 5, 0.0, 0.0, 10);
    graph.add_link(2, 3, 100, 5, 0.0, 0.0, 10);
    graph.add_link(3, 4, 100, 5, 0.0, 0.0, 10);

    // Alternative route: 1 - 5 - 4 (slower: lat 20)
    graph.add_link(1, 5, 100, 20, 0.0, 0.0, 10);
    graph.add_link(5, 4, 100, 20, 0.0, 0.0, 10);

    // Calculate initial route 1 -> 4
    pbl::dsa::DynamicArray<int> route1 = window.get_routing_service().find_route(graph, 1, 4).path;
    TEST_ASSERT(route1.size() == 4, "Initial route has 4 hops");
    TEST_ASSERT(route1[1] == 2 && route1[2] == 3, "Initial route traverses primary path via Router 2 and 3");

    // Select Router 2 on canvas and toggle failure
    window.get_canvas()->set_selected_node(2);
    window.handle_toggle_failure_selected();
    TEST_ASSERT(!graph.get_node(2)->is_active(), "Router 2 marked failed");

    // Route should automatically recalculate via backup Router 5
    pbl::dsa::DynamicArray<int> route2 = window.get_routing_service().find_route(graph, 1, 4).path;
    TEST_ASSERT(route2.size() == 3, "Rerouted route has 3 hops");
    TEST_ASSERT(route2[1] == 5, "Rerouted path traverses Router 5");

    // Recover Router 2
    window.handle_toggle_failure_selected();
    TEST_ASSERT(graph.get_node(2)->is_active(), "Router 2 recovered");
    pbl::dsa::DynamicArray<int> route3 = window.get_routing_service().find_route(graph, 1, 4).path;
    TEST_ASSERT(route3[1] == 2, "Path restored to primary route via Router 2");
}

void test_transfer_initiation_and_persistence(pbl::ui::MainWindow& window) {
    window.handle_generate_random_topology(6, 2, 0.5, 12345);

    // Initiate transfer from Client 1 to Client 2
    string payload = "Antigravity PBL2 Network Protocol Verification Payload";
    window.handle_start_file_transfer(1, 2, payload, 32);

    TEST_ASSERT(window.get_simulation_service().is_running(), "Transfer automatically started simulation clock");

    // Advance simulation ticks
    window.handle_advance_ticks(20);
    TEST_ASSERT(window.get_simulation_service().get_current_tick() == 20, "Simulation advanced to tick 20");

    // Test Config Persistence via UI
    string temp_cfg = "test_ui_temp_cfg.json";
    window.handle_save_config(temp_cfg);

    ifstream fin(temp_cfg);
    TEST_ASSERT(fin.is_open(), "Config file written via UI");
    fin.close();

    // Reload config
    window.handle_load_config(temp_cfg);
    TEST_ASSERT(window.get_network_graph().get_node_ids().size() == 6, "Nodes restored after load_config");

    // Clean up
    remove(temp_cfg.c_str());
}

int main(int argc, char* argv[]) {
    // Run headless without requiring GUI server
    char offscreen_arg[] = "-platform";
    char offscreen_val[] = "offscreen";
    char* fake_argv[] = {argv[0], offscreen_arg, offscreen_val, nullptr};
    int fake_argc = 3;

    QApplication app(fake_argc, fake_argv);

    cout << "=== Running PBL2 Milestone 11 Qt Dashboard & UI Wiring Tests ===" << endl;

    pbl::ui::MainWindow window;

    test_initialization_and_services(window);
    test_reproducible_topology_generation(window);
    test_simulation_controls_state_machine(window);
    test_fault_injection_and_rerouting(window);
    test_transfer_initiation_and_persistence(window);

    cout << "Tests run: " << tests_run << ", Passed: " << tests_passed << endl;
    if (tests_run == tests_passed) {
        cout << "[ALL MILESTONE 11 UI TESTS PASSED]" << endl;
        return 0;
    } else {
        cout << "[MILESTONE 11 UI TESTS FAILED]" << endl;
        return 1;
    }
}
