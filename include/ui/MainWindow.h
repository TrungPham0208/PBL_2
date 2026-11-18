#pragma once

#include <string>
#include <vector>

#ifdef emit
#pragma push_macro("emit")
#undef emit
#define PBL_RESTORE_EMIT_MW
#endif
#include "model/NetworkGraph.h"
#include "pbl/dsa/DynamicArray.h"
#include "services/FileStorageService.h"
#include "services/RoutingService.h"
#include "services/SimulationService.h"
#include "services/TransferService.h"
#ifdef PBL_RESTORE_EMIT_MW
#pragma pop_macro("emit")
#undef PBL_RESTORE_EMIT_MW
#endif

#include "ui/DemoPanel.h"
#include "ui/FaultInspectorPanel.h"
#include "ui/RoutingPanel.h"
#include "ui/TopologyCanvas.h"
#include "ui/TopologyPanel.h"
#include "ui/TransferPanel.h"

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPushButton>
#include <QTableWidget>
#include <QTimer>

namespace pbl {
namespace ui {

/**
 * @brief Main dashboard coordinator composing domain services, TopologyCanvas, and modular UI panels.
 */
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

    // Public accessors for testing & verification
    model::NetworkGraph& get_network_graph();
    services::RoutingService& get_routing_service();
    services::SimulationService& get_simulation_service();
    services::TransferService& get_transfer_service();
    TopologyCanvas* get_canvas();

    // Public actions exposed for direct automated UI wiring verification
    void handle_start_simulation();
    void handle_pause_simulation();
    void handle_resume_simulation();
    void handle_reset_simulation();
    void handle_single_tick();
    void handle_advance_ticks(int tick_count);

    void handle_generate_random_topology(int node_count, int client_count, double density, int seed);
    std::string handle_validate_topology();
    void handle_start_file_transfer(int src_id, int dst_id, const std::string& payload, int chunk_size);
    void handle_toggle_failure_selected();
    void handle_inject_loss_selected();
    void handle_inject_corruption_selected();

    void handle_save_config(const std::string& file_path);
    void handle_load_config(const std::string& file_path);
    void handle_export_experiment_results(const std::string& file_path);

    // End-to-End Coherent Demonstrations (SPEC.md Section 3)
    void handle_send_route_probe();
    void handle_run_recovery_demonstration();
    void handle_run_retry_limit_demonstration();
    std::string get_discovered_probe_path() const;

private slots:
    void on_simulation_timer_tick();
    void on_node_selected(int node_id);
    void on_link_selected(int node_a, int node_b);
    void on_canvas_clicked();
    void on_speed_changed(int index);

private:
    // Core Domain Services
    model::NetworkGraph graph_;
    services::RoutingService routing_service_;
    services::SimulationService simulation_service_;
    services::TransferService transfer_service_;
    services::FileStorageService storage_service_;

    // Active Transfer Session State
    int active_source_id_;
    int active_destination_id_;
    std::string transfer_payload_;
    int next_chunk_to_send_idx_;
    int last_processed_delivered_idx_;
    std::string discovered_probe_path_;
    dsa::DynamicArray<services::TransferService::Chunk> transfer_chunks_;
    bool topology_modified_manually_;

    // Simulation Playback Timer
    QTimer* sim_timer_;
    int sim_interval_ms_;

    // Canvas
    TopologyCanvas* canvas_;

    // Top Toolbar
    QPushButton* btn_start_;
    QPushButton* btn_pause_;
    QPushButton* btn_resume_;
    QPushButton* btn_reset_;
    QPushButton* btn_step1_;
    QPushButton* btn_step10_;
    QLineEdit* edit_custom_step_;
    QPushButton* btn_step_custom_;
    QComboBox* combo_sim_speed_;
    QLabel* lbl_sim_status_;
    QLabel* lbl_sim_tick_;
    QPushButton* btn_save_cfg_;
    QPushButton* btn_load_cfg_;
    QPushButton* btn_export_exp_;

    // Modular Sidebar Panels
    TopologyPanel* topology_panel_;
    TransferPanel* transfer_panel_;
    FaultInspectorPanel* fault_panel_;
    RoutingPanel* routing_panel_;
    DemoPanel* demo_panel_;

    // Guided Transfer & Demo Stepper State
    QTimer* demo_timer_;
    int demo_mode_;
    int demo_step_;
    int demo_watch_stage_;
    int demo_failed_router_;
    dsa::DynamicArray<int> demo_alt_route_;

    QTimer* xfer_timer_;
    bool xfer_step_active_;
    int xfer_step_;
    int xfer_watch_stage_;
    int xfer_src_id_;
    int xfer_dst_id_;
    int xfer_chunk_size_;
    std::string xfer_payload_;

    // Bottom Panel: Event Log & Statistics
    QTableWidget* table_event_log_;
    QPushButton* btn_clear_log_;
    QLabel* lbl_stats_summary_;

    // Internal Helpers
    void setup_ui();
    void setup_initial_startup_topology();
    QWidget* create_top_toolbar();
    QWidget* create_sidebar_tabs();
    QWidget* create_bottom_panel();

    void refresh_client_combos();
    void update_route_display();
    void update_queue_table();
    void update_transfer_display();
    void update_statistics_display();
    void append_log_entry(int tick, const std::string& source, const std::string& desc, const std::string& details);

    void initialize_transfer_session(int src_id, int dst_id, const std::string& payload, int chunk_size, bool start_playback);
    void execute_transfer_step();

    bool confirm_preset_demo_if_modified();
    void start_animated_demo1();
    void start_animated_demo2();
    void apply_demo_step(int step);
    void on_demo_prev_step();
    void on_demo_next_step();
    void on_demo_toggle_auto();
    void on_demo_timer_tick();

    void start_guided_transfer(int src_id, int dst_id, const std::string& payload, int chunk_size);
    void apply_xfer_step(int step);
    void on_xfer_prev_step();
    void on_xfer_next_step();
    void on_xfer_toggle_auto();
    void on_xfer_timer_tick();

    struct SavedNode {
        int id;
        std::string name;
        std::string type;
        bool active;
    };
    struct SavedLink {
        int u;
        int v;
        int bandwidth;
        int latency;
        double loss_prob;
        double corr_prob;
        int queue_cap;
        bool failed;
    };
    struct TopologySnapshot {
        std::vector<SavedNode> nodes;
        std::vector<SavedLink> links;
    };
    std::vector<TopologySnapshot> undo_stack_;
    void push_topology_undo();
    void handle_undo_topology();
};

}  // namespace ui
}  // namespace pbl
