#include <bits/stdc++.h>

#include <QApplication>
#include "ui/MainWindow.h"

using namespace std;

// Milestone 12: Supporting/boilerplate — Integration and coherent demonstration
// Comprehensive verification of the two documented end-to-end scenarios:
// Scenario 1: Reproducible topology -> Route probe -> Transfer -> Injected failure -> Dijkstra reroute -> Retransmit -> 100% Recovery -> Export
// Scenario 2: Transfer -> Destination isolation -> Consecutive timeouts -> Max retry-limit (3 attempts) reached -> Controlled failure -> Export

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

void test_scenario_1_recovery_demonstration(pbl::ui::MainWindow& window) {
    cout << "--- Testing Scenario 1: Dynamic Failure Recovery & Rerouting ---" << endl;

    // 1. Reproducible topology generation with seed 12345
    window.handle_generate_random_topology(6, 2, 0.45, 12345);
    pbl::model::NetworkGraph& graph = window.get_network_graph();
    TEST_ASSERT(graph.node_count() == 6, "Topology generated with 6 nodes");
    TEST_ASSERT(graph.get_client_ids().size() == 2, "Topology has 2 clients");

    // 2. Validate topology
    string report = window.handle_validate_topology();
    TEST_ASSERT(!report.empty(), "Topology validation executed");

    // 3. Hop-by-hop Route Probe from Client 1 to Client 2
    window.handle_send_route_probe();
    // Advance ticks to allow probe to traverse
    window.handle_advance_ticks(60);
    TEST_ASSERT(!window.get_discovered_probe_path().empty(), "Route probe delivered and path discovered");

    // 4. Start file transfer
    string payload = "Demonstration Payload for PBL2 Simulator: Verifying seamless retransmission and Dijkstra dynamic rerouting upon network failure!";
    window.handle_start_file_transfer(1, 2, payload, 32);

    TEST_ASSERT(window.get_simulation_service().is_running(), "Simulation clock is running for transfer");
    TEST_ASSERT(!window.get_transfer_service().is_transfer_completed(), "Transfer in progress");

    // 5. Advance ticks to put chunk 0 in flight
    window.handle_advance_ticks(15);

    // 6. Inject mid-transfer failure on primary intermediate router
    pbl::services::RoutingService::Route primary_route = window.get_routing_service().find_route(graph, 1, 2);
    TEST_ASSERT(primary_route.found && primary_route.path.size() >= 3, "Primary route traverses intermediate router");
    int failed_router = primary_route.path[1];

    window.get_simulation_service().fail_node_manually(failed_router);
    TEST_ASSERT(!graph.get_node(failed_router)->is_active(), "Intermediate router manually failed");

    // Recalculate route and verify alternate path exists
    pbl::services::RoutingService::Route alt_route = window.get_routing_service().find_route(graph, 1, 2);
    TEST_ASSERT(alt_route.found, "Alternate Dijkstra route discovered after router failure");
    TEST_ASSERT(alt_route.path[1] != failed_router, "New route avoids failed router");

    // 7. Advance ticks through timeout, retransmission over alternate route, and delivery
    window.handle_advance_ticks(400);

    // 8. Verify transfer completed successfully despite failure
    TEST_ASSERT(window.get_transfer_service().is_transfer_completed(), "Transfer completed successfully after reroute");
    TEST_ASSERT(!window.get_transfer_service().is_transfer_failed(), "Transfer did not fail");

    // 9. Verify reconstructed file matches original byte-for-byte
    string reconstructed = window.get_transfer_service().reconstruct_file();
    TEST_ASSERT(reconstructed == payload, "Reconstructed file content exactly matches original payload");

    // 10. Verify result export to data/experiments/demonstration_recovery.json
    filesystem::create_directories("data/experiments");
    string exp_file = "data/experiments/demonstration_recovery.json";
    window.handle_export_experiment_results(exp_file);

    pbl::services::FileStorageService storage;
    pbl::services::FileStorageService::ExperimentResult saved_result = storage.load_experiment_result(exp_file);
    TEST_ASSERT(saved_result.outcome == "SUCCESS", "Saved result outcome is SUCCESS");
    TEST_ASSERT(saved_result.simulation_stats.delivered_count > 0, "Delivered count recorded");
    TEST_ASSERT(saved_result.transfer_stats.acknowledged_count > 0, "Transfer acknowledged count recorded");
    TEST_ASSERT(saved_result.packet_events.size() > 0, "Packet event log preserved");
}

void test_scenario_2_retry_limit_demonstration(pbl::ui::MainWindow& window) {
    cout << "--- Testing Scenario 2: Controlled Retry-Limit Failure ---" << endl;

    // 1. Generate topology
    window.handle_generate_random_topology(6, 2, 0.45, 12345);
    pbl::model::NetworkGraph& graph = window.get_network_graph();

    // 2. Start transfer to Client 2
    string payload = "Controlled Retry Limit Failure Test Payload";
    window.handle_start_file_transfer(1, 2, payload, 64);
    // Advance 5 ticks so chunk 0 is dispatched into flight (attempt #1)
    window.handle_advance_ticks(5);

    // 3. Sever all connectivity to destination Client 2
    const pbl::dsa::DynamicArray<int>& neighbors = graph.get_neighbors(2);
    for (int i = 0; i < neighbors.size(); ++i) {
        window.get_simulation_service().fail_link_manually(2, neighbors[i]);
    }

    // Verify destination is unreachable
    pbl::services::RoutingService::Route no_route = window.get_routing_service().find_route(graph, 1, 2);
    TEST_ASSERT(!no_route.found, "Destination is strictly unreachable");

    // 4. Advance ticks past 3 timeout intervals (3 * 100 ms + margin = 350 ms)
    window.handle_advance_ticks(350);

    // 5. Verify transfer failed due to retry limit exhaustion
    TEST_ASSERT(window.get_transfer_service().is_transfer_failed(), "Transfer marked FAILED after retry limit reached");
    TEST_ASSERT(!window.get_transfer_service().is_transfer_completed(), "Transfer is not completed");
    TEST_ASSERT(window.get_transfer_service().get_statistics().failed_count > 0, "Failed count recorded in statistics");

    // 6. Verify result export to data/experiments/demonstration_failure.json
    string exp_file = "data/experiments/demonstration_failure.json";
    window.handle_export_experiment_results(exp_file);

    pbl::services::FileStorageService storage;
    pbl::services::FileStorageService::ExperimentResult saved_result = storage.load_experiment_result(exp_file);
    TEST_ASSERT(saved_result.outcome == "FAILURE", "Saved result outcome is FAILURE");
    TEST_ASSERT(saved_result.transfer_stats.failed_count > 0, "Failed count preserved in exported JSON");
    TEST_ASSERT(saved_result.packet_events.size() > 0, "Failure event logs preserved");
}

int main(int argc, char* argv[]) {
    // Run headless without requiring GUI server
    char offscreen_arg[] = "-platform";
    char offscreen_val[] = "offscreen";
    char* fake_argv[] = {argv[0], offscreen_arg, offscreen_val, nullptr};
    int fake_argc = 3;

    QApplication app(fake_argc, fake_argv);

    cout << "=== Running PBL2 Milestone 12 Coherent Demonstration Tests ===" << endl;

    pbl::ui::MainWindow window;

    test_scenario_1_recovery_demonstration(window);
    test_scenario_2_retry_limit_demonstration(window);

    cout << "Tests run: " << tests_run << ", Passed: " << tests_passed << endl;
    if (tests_run == tests_passed) {
        cout << "[ALL MILESTONE 12 DEMONSTRATION TESTS PASSED]" << endl;
        return 0;
    } else {
        cout << "[MILESTONE 12 DEMONSTRATION TESTS FAILED]" << endl;
        return 1;
    }
}
