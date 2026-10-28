#pragma once

#include <bits/stdc++.h>

#include <QMainWindow>
#include <QTimer>
#include <QPushButton>
#include <QLabel>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QLineEdit>
#include <QTextEdit>
#include <QTableWidget>
#include <QProgressBar>
#include <QTabWidget>

#include "model/NetworkGraph.h"
#include "services/FileStorageService.h"
#include "services/RoutingService.h"
#include "services/SimulationService.h"
#include "services/TransferService.h"
#include "ui/TopologyCanvas.h"

using namespace std;

namespace pbl {
namespace ui {

// Milestone 11: Supporting/boilerplate — Qt dashboard and topology controls
// MainWindow wires together presentation controls for topology editing, configuration,
// simulation playback, Dijkstra routing display, packet/event logs, transfer progress, and statistics.
// Invariant: Network rules, Dijkstra, packet queues, retry limits, checksums, and JSON persistence
// remain strictly in model/services, NOT in MainWindow.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    // Qt parent-child hierarchy automatically deallocates all child widgets upon MainWindow destruction.
    // MainWindow does not hold unmanaged raw heap pointers, so no custom destructor is needed.

    // Service accessors for programmatic interaction and unit testing
    model::NetworkGraph& get_network_graph();
    services::RoutingService& get_routing_service();
    services::SimulationService& get_simulation_service();
    services::TransferService& get_transfer_service();
    services::FileStorageService& get_storage_service();
    TopologyCanvas* get_canvas();

    // Programmatic control handlers
    void handle_start_simulation();
    void handle_pause_simulation();
    void handle_resume_simulation();
    void handle_reset_simulation();
    void handle_single_tick();
    void handle_advance_ticks(int tick_count);

    void handle_generate_random_topology(int node_count, int client_count, double density, int seed);
    string handle_validate_topology();

    void handle_start_file_transfer(int src_id, int dst_id, const string& payload, int chunk_size);
    void handle_toggle_failure_selected();
    void handle_inject_loss_selected();
    void handle_inject_corruption_selected();

    // Milestone 12: Coherent Demonstration Scenarios
    void handle_send_route_probe();
    void handle_run_recovery_demonstration();
    void handle_run_retry_limit_demonstration();
    string get_discovered_probe_path() const;

    void handle_save_config(const string& file_path);
    void handle_load_config(const string& file_path);
    void handle_export_experiment_results(const string& file_path);

public slots:
    void on_simulation_timer_tick();
    void on_node_selected(int node_id);
    void on_link_selected(int node_a, int node_b);
    void on_canvas_clicked();

private:
    // Core Domain Models and Services (composition)
    model::NetworkGraph graph_;
    services::RoutingService routing_service_;
    services::SimulationService simulation_service_;
    services::TransferService transfer_service_;
    services::FileStorageService storage_service_;

    // Playback clock timer
    QTimer* sim_timer_;

    // Visual interactive topology canvas
    TopologyCanvas* canvas_;

    // Active transfer session metadata
    int active_source_id_;
    int active_destination_id_;
    string transfer_payload_;
    dsa::DynamicArray<services::TransferService::Chunk> transfer_chunks_;
    int next_chunk_to_send_idx_;
    int last_processed_delivered_idx_;
    string discovered_probe_path_;

    // UI Widgets: Simulation Controls
    QPushButton* btn_start_;
    QPushButton* btn_pause_;
    QPushButton* btn_resume_;
    QPushButton* btn_reset_;
    QPushButton* btn_step_1_;
    QPushButton* btn_step_10_;
    QLabel* lbl_sim_status_;
    QLabel* lbl_sim_tick_;

    // UI Widgets: Topology Generation & Editing
    QSpinBox* spin_node_count_;
    QSpinBox* spin_client_count_;
    QDoubleSpinBox* spin_density_;
    QSpinBox* spin_seed_;
    QPushButton* btn_generate_;
    QPushButton* btn_validate_;
    QLabel* lbl_validation_status_;

    QLineEdit* edit_node_name_;
    QComboBox* combo_node_type_;
    QPushButton* btn_add_node_;

    QSpinBox* spin_link_a_;
    QSpinBox* spin_link_b_;
    QSpinBox* spin_link_bw_;
    QSpinBox* spin_link_lat_;
    QDoubleSpinBox* spin_link_loss_;
    QDoubleSpinBox* spin_link_corr_;
    QPushButton* btn_add_link_;
    QPushButton* btn_delete_selected_;

    // UI Widgets: Inspector & Fault Injection
    QLabel* lbl_selected_info_;
    QPushButton* btn_toggle_fail_;
    QPushButton* btn_inject_loss_;
    QPushButton* btn_inject_corruption_;

    // UI Widgets: File Transfer Setup
    QComboBox* combo_src_client_;
    QComboBox* combo_dst_client_;
    QSpinBox* spin_chunk_size_;
    QTextEdit* edit_payload_;
    QPushButton* btn_start_transfer_;
    QProgressBar* progress_transfer_;
    QLabel* lbl_transfer_status_;
    QTableWidget* table_chunks_;

    // UI Widgets: Routing & Queues
    QLabel* lbl_active_route_;
    QLabel* lbl_route_cost_;
    QTableWidget* table_routing_;
    QTableWidget* table_queues_;

    // UI Widgets: Event Log & Statistics
    QTableWidget* table_event_log_;
    QPushButton* btn_clear_log_;
    QLabel* lbl_stats_summary_;

    // Helper UI setup routines
    void setup_ui();
    QWidget* create_top_toolbar();
    QWidget* create_sidebar_tabs();
    QWidget* create_bottom_panel();

    void refresh_client_combos();
    void update_route_display();
    void update_routing_table();
    void update_queue_table();
    void update_transfer_display();
    void update_statistics_display();
    void append_log_entry(int tick, const string& source, const string& desc, const string& details);

    void execute_transfer_step();
};

}
}
