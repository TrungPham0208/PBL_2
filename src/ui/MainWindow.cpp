#include <bits/stdc++.h>

#include <QApplication>
#include <QBoxLayout>
#include <QSplitter>
#include <QGroupBox>
#include <QHeaderView>
#include <QMessageBox>
#include <QFileDialog>
#include <QDateTime>

#include "ui/MainWindow.h"

using namespace std;

namespace pbl {
namespace ui {

// Milestone 11: Supporting/boilerplate — Qt dashboard and topology controls
MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    sim_timer_ = nullptr;
    canvas_ = nullptr;

    active_source_id_ = -1;
    active_destination_id_ = -1;
    next_chunk_to_send_idx_ = 0;
    last_processed_delivered_idx_ = 0;
    discovered_probe_path_ = "";

    // Attach services
    simulation_service_.set_network_graph(&graph_);
    simulation_service_.set_routing_service(&routing_service_);

    setup_ui();

    // Default simulation playback timer: fixed 50 ms tick rate
    sim_timer_ = new QTimer(this);
    connect(sim_timer_, &QTimer::timeout, this, &MainWindow::on_simulation_timer_tick);

    // Initial default topology for immediate interactive demonstration
    handle_generate_random_topology(6, 2, 0.45, 12345);

    setWindowTitle("PBL2 Network Protocol & Dijkstra Simulation Dashboard");
    resize(1280, 820);
}

model::NetworkGraph& MainWindow::get_network_graph() {
    return graph_;
}

services::RoutingService& MainWindow::get_routing_service() {
    return routing_service_;
}

services::SimulationService& MainWindow::get_simulation_service() {
    return simulation_service_;
}

services::TransferService& MainWindow::get_transfer_service() {
    return transfer_service_;
}

services::FileStorageService& MainWindow::get_storage_service() {
    return storage_service_;
}

TopologyCanvas* MainWindow::get_canvas() {
    return canvas_;
}

void MainWindow::setup_ui() {
    QWidget* central_widget = new QWidget(this);
    QVBoxLayout* root_layout = new QVBoxLayout(central_widget);
    root_layout->setContentsMargins(8, 8, 8, 8);
    root_layout->setSpacing(8);

    // 1. Top Simulation Control Toolbar
    root_layout->addWidget(create_top_toolbar());

    // 2. Middle Main Splitter: Canvas on Left, Multi-Tab Controls on Right
    QSplitter* mid_splitter = new QSplitter(Qt::Horizontal, central_widget);

    // Interactive Topology Canvas
    canvas_ = new TopologyCanvas(mid_splitter);
    canvas_->set_network_graph(&graph_);
    canvas_->set_routing_service(&routing_service_);
    canvas_->set_simulation_service(&simulation_service_);

    connect(canvas_, &TopologyCanvas::node_selected, this, &MainWindow::on_node_selected);
    connect(canvas_, &TopologyCanvas::link_selected, this, &MainWindow::on_link_selected);
    connect(canvas_, &TopologyCanvas::canvas_clicked, this, &MainWindow::on_canvas_clicked);

    mid_splitter->addWidget(canvas_);
    mid_splitter->addWidget(create_sidebar_tabs());
    mid_splitter->setStretchFactor(0, 3);
    mid_splitter->setStretchFactor(1, 2);

    root_layout->addWidget(mid_splitter, 1);

    // 3. Bottom Panel: Event Log & Live Statistics
    root_layout->addWidget(create_bottom_panel());

    setCentralWidget(central_widget);
}

QWidget* MainWindow::create_top_toolbar() {
    QGroupBox* box = new QGroupBox("Simulation Controls & Status", this);
    QHBoxLayout* layout = new QHBoxLayout(box);
    layout->setContentsMargins(10, 6, 10, 6);
    layout->setSpacing(10);

    btn_start_ = new QPushButton("▶ Start", box);
    btn_start_->setStyleSheet("background-color: #28a745; color: white; font-weight: bold; padding: 5px 12px;");
    connect(btn_start_, &QPushButton::clicked, this, [this]() { handle_start_simulation(); });

    btn_pause_ = new QPushButton("⏸ Pause", box);
    btn_pause_->setStyleSheet("background-color: #ffc107; color: black; font-weight: bold; padding: 5px 12px;");
    connect(btn_pause_, &QPushButton::clicked, this, [this]() { handle_pause_simulation(); });

    btn_resume_ = new QPushButton("⏯ Resume", box);
    btn_resume_->setStyleSheet("background-color: #17a2b8; color: white; font-weight: bold; padding: 5px 12px;");
    connect(btn_resume_, &QPushButton::clicked, this, [this]() { handle_resume_simulation(); });

    btn_reset_ = new QPushButton("↺ Reset", box);
    btn_reset_->setStyleSheet("background-color: #dc3545; color: white; font-weight: bold; padding: 5px 12px;");
    connect(btn_reset_, &QPushButton::clicked, this, [this]() { handle_reset_simulation(); });

    btn_step_1_ = new QPushButton("Step +1ms", box);
    connect(btn_step_1_, &QPushButton::clicked, this, [this]() { handle_single_tick(); });

    btn_step_10_ = new QPushButton("Step +10ms", box);
    connect(btn_step_10_, &QPushButton::clicked, this, [this]() { handle_advance_ticks(10); });

    lbl_sim_status_ = new QLabel("Status: STOPPED", box);
    lbl_sim_status_->setStyleSheet("font-weight: bold; color: #6c757d;");

    lbl_sim_tick_ = new QLabel("Tick: 0 ms", box);
    lbl_sim_tick_->setStyleSheet("font-weight: bold; color: #0d6efd;");

    layout->addWidget(btn_start_);
    layout->addWidget(btn_pause_);
    layout->addWidget(btn_resume_);
    layout->addWidget(btn_reset_);
    layout->addWidget(btn_step_1_);
    layout->addWidget(btn_step_10_);
    layout->addSpacing(20);
    layout->addWidget(lbl_sim_status_);
    layout->addWidget(lbl_sim_tick_);
    layout->addStretch();

    QPushButton* btn_save = new QPushButton("Save Config", box);
    connect(btn_save, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getSaveFileName(this, "Save Configuration", "config.json", "JSON (*.json)");
        if (!path.isEmpty()) handle_save_config(path.toStdString());
    });

    QPushButton* btn_load = new QPushButton("Load Config", box);
    connect(btn_load, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getOpenFileName(this, "Load Configuration", "", "JSON (*.json)");
        if (!path.isEmpty()) handle_load_config(path.toStdString());
    });

    QPushButton* btn_export = new QPushButton("Export Experiment", box);
    connect(btn_export, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getSaveFileName(this, "Export Experiment Results", "data/experiments/result.json", "JSON (*.json)");
        if (!path.isEmpty()) handle_export_experiment_results(path.toStdString());
    });

    layout->addWidget(btn_save);
    layout->addWidget(btn_load);
    layout->addWidget(btn_export);

    return box;
}

QWidget* MainWindow::create_sidebar_tabs() {
    QTabWidget* tabs = new QTabWidget(this);

    // ==========================================
    // TAB 1: Topology Generation & Editing
    // ==========================================
    QWidget* tab_topo = new QWidget(tabs);
    QVBoxLayout* topo_layout = new QVBoxLayout(tab_topo);

    QGroupBox* gen_box = new QGroupBox("Random Topology Generator", tab_topo);
    QGridLayout* gen_grid = new QGridLayout(gen_box);

    gen_grid->addWidget(new QLabel("Total Nodes:"), 0, 0);
    spin_node_count_ = new QSpinBox(gen_box);
    spin_node_count_->setRange(2, 50);
    spin_node_count_->setValue(6);
    gen_grid->addWidget(spin_node_count_, 0, 1);

    gen_grid->addWidget(new QLabel("Client Count:"), 1, 0);
    spin_client_count_ = new QSpinBox(gen_box);
    spin_client_count_->setRange(1, 20);
    spin_client_count_->setValue(2);
    gen_grid->addWidget(spin_client_count_, 1, 1);

    gen_grid->addWidget(new QLabel("Connection Density:"), 2, 0);
    spin_density_ = new QDoubleSpinBox(gen_box);
    spin_density_->setRange(0.1, 1.0);
    spin_density_->setSingleStep(0.05);
    spin_density_->setValue(0.45);
    gen_grid->addWidget(spin_density_, 2, 1);

    gen_grid->addWidget(new QLabel("Random Seed:"), 3, 0);
    spin_seed_ = new QSpinBox(gen_box);
    spin_seed_->setRange(1, 9999999);
    spin_seed_->setValue(12345);
    gen_grid->addWidget(spin_seed_, 3, 1);

    btn_generate_ = new QPushButton("Generate Reproducible Topology", gen_box);
    btn_generate_->setStyleSheet("background-color: #0d6efd; color: white; font-weight: bold;");
    connect(btn_generate_, &QPushButton::clicked, this, [this]() {
        handle_generate_random_topology(spin_node_count_->value(),
                                        spin_client_count_->value(),
                                        spin_density_->value(),
                                        spin_seed_->value());
    });
    gen_grid->addWidget(btn_generate_, 4, 0, 1, 2);

    btn_validate_ = new QPushButton("Validate Topology", gen_box);
    connect(btn_validate_, &QPushButton::clicked, this, [this]() {
        string res = handle_validate_topology();
        QMessageBox::information(this, "Topology Validation", QString::fromStdString(res));
    });
    gen_grid->addWidget(btn_validate_, 5, 0, 1, 2);

    lbl_validation_status_ = new QLabel("Status: Ready", gen_box);
    gen_grid->addWidget(lbl_validation_status_, 6, 0, 1, 2);

    topo_layout->addWidget(gen_box);

    // Manual Node / Link Add Box
    QGroupBox* manual_box = new QGroupBox("Manual Node & Link Creation", tab_topo);
    QGridLayout* manual_grid = new QGridLayout(manual_box);

    manual_grid->addWidget(new QLabel("Node Name:"), 0, 0);
    edit_node_name_ = new QLineEdit(manual_box);
    edit_node_name_->setPlaceholderText("Node name");
    manual_grid->addWidget(edit_node_name_, 0, 1);

    manual_grid->addWidget(new QLabel("Node Type:"), 1, 0);
    combo_node_type_ = new QComboBox(manual_box);
    combo_node_type_->addItem("Client");
    combo_node_type_->addItem("Router");
    manual_grid->addWidget(combo_node_type_, 1, 1);

    btn_add_node_ = new QPushButton("Add Node", manual_box);
    connect(btn_add_node_, &QPushButton::clicked, this, [this]() {
        int next_id = 1;
        const dsa::DynamicArray<int>& existing = graph_.get_node_ids();
        for (int i = 0; i < existing.size(); ++i) {
            if (existing[i] >= next_id) next_id = existing[i] + 1;
        }
        string name = edit_node_name_->text().toStdString();
        if (name.empty()) name = "Node" + to_string(next_id);
        if (combo_node_type_->currentText() == "Client") {
            graph_.add_client(next_id, name);
        } else {
            graph_.add_router(next_id, name);
        }
        canvas_->update_layout();
        refresh_client_combos();
        canvas_->update();
        append_log_entry(simulation_service_.get_current_tick(), "Topology", "Added " + combo_node_type_->currentText().toStdString() + " ID " + to_string(next_id), name);
    });
    manual_grid->addWidget(btn_add_node_, 2, 0, 1, 2);

    manual_grid->addWidget(new QLabel("Link Nodes (A - B):"), 3, 0);
    QHBoxLayout* link_nodes_layout = new QHBoxLayout();
    spin_link_a_ = new QSpinBox(manual_box);
    spin_link_b_ = new QSpinBox(manual_box);
    link_nodes_layout->addWidget(spin_link_a_);
    link_nodes_layout->addWidget(new QLabel("-"));
    link_nodes_layout->addWidget(spin_link_b_);
    manual_grid->addLayout(link_nodes_layout, 3, 1);

    manual_grid->addWidget(new QLabel("Bandwidth (KB/s):"), 4, 0);
    spin_link_bw_ = new QSpinBox(manual_box);
    spin_link_bw_->setRange(1, 10000);
    spin_link_bw_->setValue(100);
    manual_grid->addWidget(spin_link_bw_, 4, 1);

    manual_grid->addWidget(new QLabel("Latency (ms):"), 5, 0);
    spin_link_lat_ = new QSpinBox(manual_box);
    spin_link_lat_->setRange(1, 1000);
    spin_link_lat_->setValue(10);
    manual_grid->addWidget(spin_link_lat_, 5, 1);

    manual_grid->addWidget(new QLabel("Loss Prob:"), 6, 0);
    spin_link_loss_ = new QDoubleSpinBox(manual_box);
    spin_link_loss_->setRange(0.0, 1.0);
    spin_link_loss_->setSingleStep(0.01);
    spin_link_loss_->setValue(0.0);
    manual_grid->addWidget(spin_link_loss_, 6, 1);

    manual_grid->addWidget(new QLabel("Corruption Prob:"), 7, 0);
    spin_link_corr_ = new QDoubleSpinBox(manual_box);
    spin_link_corr_->setRange(0.0, 1.0);
    spin_link_corr_->setSingleStep(0.01);
    spin_link_corr_->setValue(0.0);
    manual_grid->addWidget(spin_link_corr_, 7, 1);

    btn_add_link_ = new QPushButton("Add / Update Link", manual_box);
    connect(btn_add_link_, &QPushButton::clicked, this, [this]() {
        int u = spin_link_a_->value();
        int v = spin_link_b_->value();
        if (u == v || graph_.get_node(u) == nullptr || graph_.get_node(v) == nullptr) {
            QMessageBox::warning(this, "Invalid Link", "Please select two distinct existing node IDs.");
            return;
        }
        graph_.add_link(u, v, spin_link_bw_->value(), spin_link_lat_->value(),
                        spin_link_loss_->value(), spin_link_corr_->value(), 10);
        canvas_->update();
        update_route_display();
        append_log_entry(simulation_service_.get_current_tick(), "Topology", "Added/updated link (" + to_string(u) + " - " + to_string(v) + ")", "");
    });
    manual_grid->addWidget(btn_add_link_, 8, 0, 1, 2);

    btn_delete_selected_ = new QPushButton("Delete Selected Node / Link", manual_box);
    btn_delete_selected_->setStyleSheet("color: #dc3545;");
    connect(btn_delete_selected_, &QPushButton::clicked, this, [this]() {
        int sel_node = canvas_->get_selected_node();
        pair<int, int> sel_link = canvas_->get_selected_link();
        if (sel_node != -1) {
            graph_.remove_node(sel_node);
            canvas_->set_selected_node(-1);
            refresh_client_combos();
            canvas_->update();
            update_route_display();
        } else if (sel_link.first != -1) {
            graph_.remove_link(sel_link.first, sel_link.second);
            canvas_->set_selected_link(-1, -1);
            canvas_->update();
            update_route_display();
        }
    });
    manual_grid->addWidget(btn_delete_selected_, 9, 0, 1, 2);

    topo_layout->addWidget(manual_box);
    topo_layout->addStretch();
    tabs->addTab(tab_topo, "Topology");

    // ==========================================
    // TAB 2: File Transfer Setup & Progress
    // ==========================================
    QWidget* tab_transfer = new QWidget(tabs);
    QVBoxLayout* transfer_layout = new QVBoxLayout(tab_transfer);

    QGroupBox* setup_box = new QGroupBox("Reliable File Transfer Setup", tab_transfer);
    QGridLayout* setup_grid = new QGridLayout(setup_box);

    setup_grid->addWidget(new QLabel("Source Client:"), 0, 0);
    combo_src_client_ = new QComboBox(setup_box);
    setup_grid->addWidget(combo_src_client_, 0, 1);

    setup_grid->addWidget(new QLabel("Destination Client:"), 1, 0);
    combo_dst_client_ = new QComboBox(setup_box);
    setup_grid->addWidget(combo_dst_client_, 1, 1);

    setup_grid->addWidget(new QLabel("Chunk Size (bytes):"), 2, 0);
    spin_chunk_size_ = new QSpinBox(setup_box);
    spin_chunk_size_->setRange(16, 65536);
    spin_chunk_size_->setValue(1024);
    setup_grid->addWidget(spin_chunk_size_, 2, 1);

    setup_grid->addWidget(new QLabel("Payload / Content:"), 3, 0);
    edit_payload_ = new QTextEdit(setup_box);
    edit_payload_->setPlainText("Lorem ipsum dolor sit amet, consectetur adipiscing elit. Network simulation payload for reliable transfer demonstration.");
    edit_payload_->setMaximumHeight(60);
    setup_grid->addWidget(edit_payload_, 3, 1);

    btn_start_transfer_ = new QPushButton("Start Reliable File Transfer", setup_box);
    btn_start_transfer_->setStyleSheet("background-color: #198754; color: white; font-weight: bold; padding: 6px;");
    connect(btn_start_transfer_, &QPushButton::clicked, this, [this]() {
        if (combo_src_client_->count() == 0 || combo_dst_client_->count() == 0) {
            QMessageBox::warning(this, "Transfer Error", "Need at least two clients in the topology.");
            return;
        }
        int src_id = combo_src_client_->currentData().toInt();
        int dst_id = combo_dst_client_->currentData().toInt();
        if (src_id == dst_id) {
            QMessageBox::warning(this, "Transfer Error", "Source and Destination clients must be distinct.");
            return;
        }
        handle_start_file_transfer(src_id, dst_id, edit_payload_->toPlainText().toStdString(), spin_chunk_size_->value());
    });
    setup_grid->addWidget(btn_start_transfer_, 4, 0, 1, 2);

    transfer_layout->addWidget(setup_box);

    // Progress Box
    QGroupBox* progress_box = new QGroupBox("Transfer Progress & Chunks", tab_transfer);
    QVBoxLayout* prog_layout = new QVBoxLayout(progress_box);

    lbl_transfer_status_ = new QLabel("Transfer Status: IDLE", progress_box);
    lbl_transfer_status_->setStyleSheet("font-weight: bold; color: #495057;");
    prog_layout->addWidget(lbl_transfer_status_);

    progress_transfer_ = new QProgressBar(progress_box);
    progress_transfer_->setRange(0, 100);
    progress_transfer_->setValue(0);
    prog_layout->addWidget(progress_transfer_);

    table_chunks_ = new QTableWidget(progress_box);
    table_chunks_->setColumnCount(5);
    table_chunks_->setHorizontalHeaderLabels({"Seq #", "State", "Attempts", "Size", "Checksum"});
    table_chunks_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    prog_layout->addWidget(table_chunks_);

    transfer_layout->addWidget(progress_box);
    tabs->addTab(tab_transfer, "Transfer");

    // ==========================================
    // TAB 3: Inspector & Fault Injection
    // ==========================================
    QWidget* tab_faults = new QWidget(tabs);
    QVBoxLayout* faults_layout = new QVBoxLayout(tab_faults);

    QGroupBox* inspector_box = new QGroupBox("Selected Node / Link Inspector", tab_faults);
    QVBoxLayout* insp_layout = new QVBoxLayout(inspector_box);

    lbl_selected_info_ = new QLabel("Click any Node or Link on the canvas to inspect.", inspector_box);
    lbl_selected_info_->setWordWrap(true);
    lbl_selected_info_->setStyleSheet("padding: 8px; background-color: #e9ecef; border-radius: 4px;");
    insp_layout->addWidget(lbl_selected_info_);

    btn_toggle_fail_ = new QPushButton("Toggle Selected Failure / Recovery", inspector_box);
    btn_toggle_fail_->setStyleSheet("background-color: #dc3545; color: white; font-weight: bold; padding: 6px;");
    connect(btn_toggle_fail_, &QPushButton::clicked, this, &MainWindow::handle_toggle_failure_selected);
    insp_layout->addWidget(btn_toggle_fail_);

    btn_inject_loss_ = new QPushButton("Force Drop Next Packet on Selected Link", inspector_box);
    connect(btn_inject_loss_, &QPushButton::clicked, this, &MainWindow::handle_inject_loss_selected);
    insp_layout->addWidget(btn_inject_loss_);

    btn_inject_corruption_ = new QPushButton("Force Corrupt Next Packet on Selected Link", inspector_box);
    connect(btn_inject_corruption_, &QPushButton::clicked, this, &MainWindow::handle_inject_corruption_selected);
    insp_layout->addWidget(btn_inject_corruption_);

    faults_layout->addWidget(inspector_box);
    faults_layout->addStretch();
    tabs->addTab(tab_faults, "Inspector & Faults");

    // ==========================================
    // TAB 4: Routing & Queues
    // ==========================================
    QWidget* tab_routing = new QWidget(tabs);
    QVBoxLayout* routing_layout = new QVBoxLayout(tab_routing);

    QGroupBox* route_box = new QGroupBox("Current Lowest-Cost Shortest Path", tab_routing);
    QVBoxLayout* r_box_layout = new QVBoxLayout(route_box);
    lbl_active_route_ = new QLabel("No active route calculated.", route_box);
    lbl_active_route_->setStyleSheet("font-weight: bold; color: #198754; font-size: 13px;");
    lbl_route_cost_ = new QLabel("Cost: N/A", route_box);
    r_box_layout->addWidget(lbl_active_route_);
    r_box_layout->addWidget(lbl_route_cost_);
    routing_layout->addWidget(route_box);

    QGroupBox* queue_box = new QGroupBox("Link Queue Occupancy", tab_routing);
    QVBoxLayout* q_layout = new QVBoxLayout(queue_box);
    table_queues_ = new QTableWidget(queue_box);
    table_queues_->setColumnCount(4);
    table_queues_->setHorizontalHeaderLabels({"Link", "Queue Depth", "Capacity", "Status"});
    table_queues_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    q_layout->addWidget(table_queues_);
    routing_layout->addWidget(queue_box);

    tabs->addTab(tab_routing, "Routing & Queues");

    // ==========================================
    // TAB 5: Coherent Demonstrations (SPEC.md Section 3)
    // ==========================================
    QWidget* tab_demo = new QWidget(tabs);
    QVBoxLayout* demo_layout = new QVBoxLayout(tab_demo);

    QGroupBox* demo_box = new QGroupBox("End-to-End Demonstrations", tab_demo);
    QVBoxLayout* d_layout = new QVBoxLayout(demo_box);

    QPushButton* btn_probe = new QPushButton("Send Hop-by-Hop Route Probe", demo_box);
    btn_probe->setStyleSheet("background-color: #6f42c1; color: white; font-weight: bold; padding: 6px;");
    connect(btn_probe, &QPushButton::clicked, this, &MainWindow::handle_send_route_probe);
    d_layout->addWidget(btn_probe);

    QPushButton* btn_demo1 = new QPushButton("Run Scenario 1: Dynamic Recovery & Rerouting", demo_box);
    btn_demo1->setStyleSheet("background-color: #0d6efd; color: white; font-weight: bold; padding: 8px;");
    connect(btn_demo1, &QPushButton::clicked, this, &MainWindow::handle_run_recovery_demonstration);
    d_layout->addWidget(btn_demo1);

    QPushButton* btn_demo2 = new QPushButton("Run Scenario 2: Controlled Retry-Limit Failure", demo_box);
    btn_demo2->setStyleSheet("background-color: #dc3545; color: white; font-weight: bold; padding: 8px;");
    connect(btn_demo2, &QPushButton::clicked, this, &MainWindow::handle_run_retry_limit_demonstration);
    d_layout->addWidget(btn_demo2);

    QLabel* lbl_demo_desc = new QLabel("<b>Scenario 1 Story:</b> Transfer starts ➔ Intermediate router fails mid-transfer ➔ Dijkstra recalculates route ➔ Missing chunks retransmit over alternate path ➔ File reconstructed.<br><br>"
                                      "<b>Scenario 2 Story:</b> Transfer starts ➔ Destination becomes unreachable ➔ ACKs timeout ➔ Retransmission attempts hit limit (3) ➔ Transfer fails with evidence.", demo_box);
    lbl_demo_desc->setWordWrap(true);
    lbl_demo_desc->setStyleSheet("padding: 8px; background-color: #f8f9fa; border: 1px solid #dee2e6; border-radius: 4px;");
    d_layout->addWidget(lbl_demo_desc);

    demo_layout->addWidget(demo_box);
    demo_layout->addStretch();
    tabs->addTab(tab_demo, "Demonstrations");

    return tabs;
}

QWidget* MainWindow::create_bottom_panel() {
    QGroupBox* bottom_box = new QGroupBox("Packet Events & Statistics", this);
    QVBoxLayout* layout = new QVBoxLayout(bottom_box);
    layout->setContentsMargins(8, 4, 8, 4);

    QHBoxLayout* top_bar = new QHBoxLayout();
    lbl_stats_summary_ = new QLabel("Sent: 0 | Delivered: 0 | Dropped: 0 | Lost: 0 | Corrupted: 0 | Heartbeat Timeouts: 0 | Acks: 0 | Retries: 0", bottom_box);
    lbl_stats_summary_->setStyleSheet("font-weight: bold; color: #212529;");
    top_bar->addWidget(lbl_stats_summary_);
    top_bar->addStretch();

    btn_clear_log_ = new QPushButton("Clear Log", bottom_box);
    connect(btn_clear_log_, &QPushButton::clicked, this, [this]() {
        table_event_log_->setRowCount(0);
    });
    top_bar->addWidget(btn_clear_log_);
    layout->addLayout(top_bar);

    table_event_log_ = new QTableWidget(bottom_box);
    table_event_log_->setColumnCount(4);
    table_event_log_->setHorizontalHeaderLabels({"Tick", "Source", "Event Description", "Details"});
    table_event_log_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table_event_log_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table_event_log_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    table_event_log_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    table_event_log_->setMaximumHeight(140);
    layout->addWidget(table_event_log_);

    return bottom_box;
}

void MainWindow::refresh_client_combos() {
    combo_src_client_->clear();
    combo_dst_client_->clear();

    const dsa::DynamicArray<int>& node_ids = graph_.get_node_ids();
    for (int i = 0; i < node_ids.size(); ++i) {
        int id = node_ids[i];
        model::Node* node = graph_.get_node(id);
        if (node != nullptr && node->get_type() == "Client") {
            QString item_text = QString("Client %1 (%2)").arg(id).arg(QString::fromStdString(node->get_name()));
            combo_src_client_->addItem(item_text, id);
            combo_dst_client_->addItem(item_text, id);
        }
    }
    if (combo_dst_client_->count() > 1) {
        combo_dst_client_->setCurrentIndex(1);
    }
}

void MainWindow::handle_start_simulation() {
    simulation_service_.start();
    sim_timer_->start(50);
    lbl_sim_status_->setText("Status: RUNNING");
    lbl_sim_status_->setStyleSheet("font-weight: bold; color: #28a745;");
    append_log_entry(simulation_service_.get_current_tick(), "Simulation", "Simulation started", "");
}

void MainWindow::handle_pause_simulation() {
    simulation_service_.pause();
    sim_timer_->stop();
    lbl_sim_status_->setText("Status: PAUSED");
    lbl_sim_status_->setStyleSheet("font-weight: bold; color: #ffc107;");
    append_log_entry(simulation_service_.get_current_tick(), "Simulation", "Simulation paused", "");
}

void MainWindow::handle_resume_simulation() {
    simulation_service_.resume();
    sim_timer_->start(50);
    lbl_sim_status_->setText("Status: RUNNING");
    lbl_sim_status_->setStyleSheet("font-weight: bold; color: #28a745;");
    append_log_entry(simulation_service_.get_current_tick(), "Simulation", "Simulation resumed", "");
}

void MainWindow::handle_reset_simulation() {
    sim_timer_->stop();
    simulation_service_.reset();
    active_source_id_ = -1;
    active_destination_id_ = -1;
    next_chunk_to_send_idx_ = 0;
    last_processed_delivered_idx_ = 0;
    discovered_probe_path_ = "";
    transfer_chunks_.clear();

    lbl_sim_status_->setText("Status: STOPPED");
    lbl_sim_status_->setStyleSheet("font-weight: bold; color: #6c757d;");
    lbl_sim_tick_->setText("Tick: 0 ms");
    lbl_transfer_status_->setText("Transfer Status: IDLE");
    progress_transfer_->setValue(0);
    table_chunks_->setRowCount(0);

    canvas_->set_active_route(dsa::DynamicArray<int>());
    canvas_->update();
    update_route_display();
    update_statistics_display();
    append_log_entry(0, "Simulation", "Simulation clock and metrics reset", "");
}

void MainWindow::handle_single_tick() {
    handle_advance_ticks(1);
}

void MainWindow::handle_advance_ticks(int tick_count) {
    for (int i = 0; i < tick_count; ++i) {
        simulation_service_.tick();
        execute_transfer_step();
    }
    lbl_sim_tick_->setText(QString("Tick: %1 ms").arg(simulation_service_.get_current_tick()));
    canvas_->update();
    update_transfer_display();
    update_queue_table();
    update_statistics_display();
}

void MainWindow::on_simulation_timer_tick() {
    handle_advance_ticks(1);
}

void MainWindow::handle_generate_random_topology(int node_count, int client_count, double density, int seed) {
    handle_reset_simulation();

    graph_ = model::NetworkGraph();
    canvas_->set_network_graph(&graph_);

    client_count = max(1, min(client_count, node_count - 1));
    int router_count = node_count - client_count;

    // Deterministic random generation using standard linear congruential / mt19937 engine
    mt19937 rng(seed);

    int id_counter = 1;
    dsa::DynamicArray<int> client_ids;
    dsa::DynamicArray<int> router_ids;

    // Add Clients
    for (int i = 0; i < client_count; ++i) {
        int id = id_counter++;
        graph_.add_client(id, "Client" + to_string(id));
        client_ids.push_back(id);
    }

    // Add Routers
    for (int i = 0; i < router_count; ++i) {
        int id = id_counter++;
        graph_.add_router(id, "Router" + to_string(id));
        router_ids.push_back(id);
    }

    // Connect clients to routers with redundant paths for dynamic failure recovery
    for (int i = 0; i < client_ids.size(); ++i) {
        int r1 = i % router_ids.size();
        graph_.add_link(client_ids[i], router_ids[r1], 100, 5, 0.0, 0.0, 10);
        if (router_ids.size() > 1) {
            int r2 = (r1 + 1) % router_ids.size();
            graph_.add_link(client_ids[i], router_ids[r2], 80, 15, 0.0, 0.0, 10);
        }
    }

    // Connect routers into a resilient mesh
    for (int i = 0; i < router_ids.size(); ++i) {
        for (int j = i + 1; j < router_ids.size(); ++j) {
            double prob = (double)(rng() % 1000) / 1000.0;
            if (prob <= density || j == (i + 1)) { // Ensure backbone connectivity
                int bw = 100 + (rng() % 5) * 50;
                int lat = 5 + (rng() % 4) * 5;
                graph_.add_link(router_ids[i], router_ids[j], bw, lat, 0.0, 0.0, 10);
            }
        }
    }

    canvas_->update_layout();
    refresh_client_combos();
    if (client_ids.size() >= 2) {
        active_source_id_ = client_ids[0];
        active_destination_id_ = client_ids[1];
    }
    canvas_->update();
    update_route_display();
    append_log_entry(0, "Topology", "Generated topology with seed " + to_string(seed),
                     to_string(node_count) + " nodes, density=" + to_string(density));
}

string MainWindow::handle_validate_topology() {
    model::NetworkGraph::ValidationResult report = graph_.validate();
    string summary = report.is_valid ? "Topology is VALID & fully connected." : "Topology WARNINGS detected:\n";
    for (int i = 0; i < report.warnings.size(); ++i) {
        summary += "- " + report.warnings[i] + "\n";
    }
    lbl_validation_status_->setText(report.is_valid ? "Status: VALID" : "Status: WARNINGS");
    lbl_validation_status_->setStyleSheet(report.is_valid ? "color: #198754; font-weight: bold;" : "color: #dc3545; font-weight: bold;");
    return summary;
}

void MainWindow::handle_start_file_transfer(int src_id, int dst_id, const string& payload, int chunk_size) {
    active_source_id_ = src_id;
    active_destination_id_ = dst_id;
    transfer_payload_ = payload;
    next_chunk_to_send_idx_ = 0;

    transfer_service_.init_sender(payload, chunk_size);
    transfer_service_.init_receiver(transfer_service_.get_sender_chunk_count());
    transfer_chunks_ = transfer_service_.get_all_sender_chunks();

    // Compute route
    services::RoutingService::Route r = routing_service_.find_route(graph_, src_id, dst_id);
    canvas_->set_active_route(r.path);
    update_route_display();

    lbl_transfer_status_->setText("Transfer Status: IN_PROGRESS");
    lbl_transfer_status_->setStyleSheet("font-weight: bold; color: #0d6efd;");
    progress_transfer_->setValue(0);

    table_chunks_->setRowCount(transfer_chunks_.size());
    for (int i = 0; i < transfer_chunks_.size(); ++i) {
        table_chunks_->setItem(i, 0, new QTableWidgetItem(QString::number(transfer_chunks_[i].sequence_number)));
        table_chunks_->setItem(i, 1, new QTableWidgetItem("UNSENT"));
        table_chunks_->setItem(i, 2, new QTableWidgetItem("0"));
        table_chunks_->setItem(i, 3, new QTableWidgetItem(QString::number(transfer_chunks_[i].size) + " B"));
        table_chunks_->setItem(i, 4, new QTableWidgetItem(QString::number(transfer_chunks_[i].checksum)));
    }

    append_log_entry(simulation_service_.get_current_tick(), "Transfer",
                     "Initiated file transfer: Client " + to_string(src_id) + " -> Client " + to_string(dst_id),
                     to_string(transfer_chunks_.size()) + " chunks");

    // Automatically start simulation clock if not running
    if (!simulation_service_.is_running()) {
        handle_start_simulation();
    }
}

void MainWindow::execute_transfer_step() {
    if (active_source_id_ == -1 || active_destination_id_ == -1) {
        return;
    }

    int current_tick = simulation_service_.get_current_tick();

    // 1. Dispatch unsent chunks if route is available
    if (next_chunk_to_send_idx_ < transfer_chunks_.size()) {
        services::RoutingService::Route r = routing_service_.find_route(graph_, active_source_id_, active_destination_id_);
        if (r.found && r.path.size() >= 2) {
            model::Packet pkt = transfer_service_.send_chunk(next_chunk_to_send_idx_, active_source_id_,
                                                            active_destination_id_, current_tick);
            bool dispatched = simulation_service_.send_packet_along_route(r.path, pkt);
            if (dispatched) {
                append_log_entry(current_tick, "Sender",
                                 "Dispatched DATA Chunk #" + to_string(next_chunk_to_send_idx_),
                                 "Seq: " + to_string(next_chunk_to_send_idx_));
                next_chunk_to_send_idx_++;
            }
        }
    }

    // 2. Check timeouts and retransmit if necessary
    dsa::DynamicArray<int> timed_out = transfer_service_.check_timeouts(current_tick);
    for (int i = 0; i < timed_out.size(); ++i) {
        int seq = timed_out[i];
        model::Packet retrans = transfer_service_.send_chunk(seq, active_source_id_,
                                                            active_destination_id_, current_tick);
        services::RoutingService::Route r = routing_service_.find_route(graph_, active_source_id_, active_destination_id_);
        if (r.found && r.path.size() >= 2) {
            bool sent = simulation_service_.send_packet_along_route(r.path, retrans);
            if (sent) {
                append_log_entry(current_tick, "Retransmit",
                                 "Retransmitted DATA Chunk #" + to_string(seq),
                                 "Attempt: " + to_string(transfer_service_.get_chunk_attempts(seq)));
            }
        } else {
            append_log_entry(current_tick, "Retransmit",
                             "Retransmit attempted for Chunk #" + to_string(seq) + " but no route available",
                             "Attempt: " + to_string(transfer_service_.get_chunk_attempts(seq)));
        }
    }

    // 3. Process newly delivered packets from simulation service
    const dsa::DynamicArray<model::Packet>& delivered = simulation_service_.get_delivered_packets();
    while (last_processed_delivered_idx_ < delivered.size()) {
        const model::Packet& pkt = delivered[last_processed_delivered_idx_++];

        if (pkt.get_type() == model::Packet::Type::DATA && pkt.get_destination_id() == active_destination_id_) {
            bool valid = transfer_service_.receive_chunk_packet(pkt);
            if (valid) {
                append_log_entry(current_tick, "Receiver",
                                 "Verified Chunk #" + to_string(pkt.get_sequence_number()) + " checksum. Emitting ACK.",
                                 "Seq: " + to_string(pkt.get_sequence_number()));

                // Send ACK packet back to sender along current best route
                services::RoutingService::Route ack_r = routing_service_.find_route(graph_, active_destination_id_, active_source_id_);
                if (ack_r.found && ack_r.path.size() >= 2) {
                    model::Packet ack_pkt = transfer_service_.create_ack_packet(active_destination_id_, active_source_id_, pkt.get_sequence_number());
                    simulation_service_.send_packet_along_route(ack_r.path, ack_pkt);
                }
            } else {
                append_log_entry(current_tick, "Receiver",
                                 "Checksum mismatch on Chunk #" + to_string(pkt.get_sequence_number()) + "! Dropping without ACK.",
                                 "Corruption detected");
            }
        } else if (pkt.get_type() == model::Packet::Type::ACK && pkt.get_destination_id() == active_source_id_) {
            bool acked = transfer_service_.process_ack_packet(pkt);
            if (acked) {
                append_log_entry(current_tick, "Sender",
                                 "Received ACK for Chunk #" + to_string(pkt.get_sequence_number()),
                                 "Seq: " + to_string(pkt.get_sequence_number()));
            }
            if (transfer_service_.is_transfer_completed()) {
                string reconstructed = transfer_service_.reconstruct_file();
                append_log_entry(current_tick, "Transfer",
                                 "Transfer COMPLETED successfully! Reconstructed " + to_string(reconstructed.size()) + " bytes.",
                                 "Integrity 100%");
            }
        } else if (pkt.get_type() == model::Packet::Type::ROUTE_PROBE && pkt.get_destination_id() == active_destination_id_) {
            discovered_probe_path_ = pkt.get_payload();
            append_log_entry(current_tick, "Probe",
                             "ROUTE_PROBE delivered to destination! Discovered Path: " + discovered_probe_path_,
                             "Hops: " + to_string(pkt.get_sequence_number()));
        }
    }
}

void MainWindow::on_node_selected(int node_id) {
    model::Node* n = graph_.get_node(node_id);
    if (n == nullptr) return;

    QString info = QString("<b>Selected Node:</b> ID %1<br>"
                           "<b>Name:</b> %2<br>"
                           "<b>Type:</b> %3<br>"
                           "<b>Status:</b> %4")
                       .arg(node_id)
                       .arg(QString::fromStdString(n->get_name()))
                       .arg(QString::fromStdString(n->get_type()))
                       .arg(n->is_active() ? "<font color='green'>ACTIVE</font>" : "<font color='red'>FAILED</font>");
    lbl_selected_info_->setText(info);
}

void MainWindow::on_link_selected(int node_a, int node_b) {
    model::Link* l = graph_.get_link(node_a, node_b);
    if (l == nullptr) return;

    QString info = QString("<b>Selected Link:</b> (%1 - %2)<br>"
                           "<b>Bandwidth:</b> %3 KB/s<br>"
                           "<b>Latency:</b> %4 ms<br>"
                           "<b>Loss Prob:</b> %5<br>"
                           "<b>Corruption Prob:</b> %6<br>"
                           "<b>Queue:</b> %7 / %8<br>"
                           "<b>Status:</b> %9")
                       .arg(node_a).arg(node_b)
                       .arg(l->get_bandwidth())
                       .arg(l->get_latency())
                       .arg(l->get_loss_probability())
                       .arg(l->get_corruption_probability())
                       .arg(l->queue_size())
                       .arg(l->queue_capacity())
                       .arg(l->is_failed() ? "<font color='red'>FAILED</font>" : "<font color='green'>ACTIVE</font>");
    lbl_selected_info_->setText(info);
}

void MainWindow::on_canvas_clicked() {
    lbl_selected_info_->setText("Click any Node or Link on the canvas to inspect.");
}

void MainWindow::handle_toggle_failure_selected() {
    int sel_node = canvas_->get_selected_node();
    pair<int, int> sel_link = canvas_->get_selected_link();

    int tick = simulation_service_.get_current_tick();

    if (sel_node != -1) {
        model::Node* n = graph_.get_node(sel_node);
        if (n != nullptr) {
            if (n->is_active()) {
                simulation_service_.fail_node_manually(sel_node);
                append_log_entry(tick, "Fault", "Manually failed Node ID " + to_string(sel_node), "Triggered reroute");
            } else {
                n->set_active(true);
                append_log_entry(tick, "Fault", "Manually recovered Node ID " + to_string(sel_node), "Available for routing");
            }
            on_node_selected(sel_node);
        }
    } else if (sel_link.first != -1) {
        model::Link* l = graph_.get_link(sel_link.first, sel_link.second);
        if (l != nullptr) {
            if (!l->is_failed()) {
                simulation_service_.fail_link_manually(sel_link.first, sel_link.second);
                append_log_entry(tick, "Fault", "Manually failed Link (" + to_string(sel_link.first) + " - " + to_string(sel_link.second) + ")", "");
            } else {
                l->recover();
                append_log_entry(tick, "Fault", "Manually recovered Link (" + to_string(sel_link.first) + " - " + to_string(sel_link.second) + ")", "");
            }
            on_link_selected(sel_link.first, sel_link.second);
        }
    }

    update_route_display();
    canvas_->update();
}

void MainWindow::handle_inject_loss_selected() {
    pair<int, int> sel_link = canvas_->get_selected_link();
    if (sel_link.first != -1) {
        simulation_service_.inject_link_loss(sel_link.first, sel_link.second);
        append_log_entry(simulation_service_.get_current_tick(), "Fault",
                         "Arming manual packet drop on Link (" + to_string(sel_link.first) + " - " + to_string(sel_link.second) + ")", "Next packet will drop");
    }
}

void MainWindow::handle_inject_corruption_selected() {
    pair<int, int> sel_link = canvas_->get_selected_link();
    if (sel_link.first != -1) {
        simulation_service_.inject_link_corruption(sel_link.first, sel_link.second);
        append_log_entry(simulation_service_.get_current_tick(), "Fault",
                         "Arming manual packet corruption on Link (" + to_string(sel_link.first) + " - " + to_string(sel_link.second) + ")", "Next packet will corrupt");
    }
}

void MainWindow::update_route_display() {
    if (active_source_id_ == -1 || active_destination_id_ == -1) {
        lbl_active_route_->setText("Select Source & Destination clients to compute route.");
        lbl_route_cost_->setText("Cost: N/A");
        return;
    }

    services::RoutingService::Route r = routing_service_.find_route(graph_, active_source_id_, active_destination_id_);
    canvas_->set_active_route(r.path);

    if (!r.found || r.path.size() < 2) {
        lbl_active_route_->setText("<font color='red'>NO ROUTE AVAILABLE (Destination Unreachable)</font>");
        lbl_route_cost_->setText("Cost: INFINITY");
    } else {
        QString path_str = "";
        for (int i = 0; i < r.path.size(); ++i) {
            model::Node* n = graph_.get_node(r.path[i]);
            QString name = n ? QString::fromStdString(n->get_name()) : QString::number(r.path[i]);
            path_str += name;
            if (i < r.path.size() - 1) path_str += " ➔ ";
        }
        lbl_active_route_->setText(path_str);
        lbl_route_cost_->setText(QString("Total Dijkstra Cost: %1").arg(r.total_cost, 0, 'f', 2));
    }
}

void MainWindow::update_queue_table() {
    const dsa::DynamicArray<model::Link*>& links = graph_.get_all_links();
    table_queues_->setRowCount(links.size());

    for (int i = 0; i < links.size(); ++i) {
        model::Link* l = links[i];
        if (l == nullptr) continue;
        QString name = QString("%1 ⮂ %2").arg(l->get_first_node_id()).arg(l->get_second_node_id());
        table_queues_->setItem(i, 0, new QTableWidgetItem(name));
        table_queues_->setItem(i, 1, new QTableWidgetItem(QString::number(l->queue_size())));
        table_queues_->setItem(i, 2, new QTableWidgetItem(QString::number(l->queue_capacity())));
        table_queues_->setItem(i, 3, new QTableWidgetItem(l->is_failed() ? "FAILED" : (l->queue_full() ? "FULL" : "NORMAL")));
    }
}

void MainWindow::update_transfer_display() {
    if (active_source_id_ == -1) return;

    services::TransferService::TransferStatistics stats = transfer_service_.get_statistics();
    int total = transfer_chunks_.size();
    int acks = stats.acknowledged_count;

    if (total > 0) {
        int pct = (acks * 100) / total;
        progress_transfer_->setValue(pct);
    }

    if (transfer_service_.is_transfer_completed()) {
        lbl_transfer_status_->setText("Transfer Status: COMPLETED");
        lbl_transfer_status_->setStyleSheet("font-weight: bold; color: #198754;");
    } else if (transfer_service_.is_transfer_failed()) {
        lbl_transfer_status_->setText("Transfer Status: FAILED (Retry Limit Exceeded)");
        lbl_transfer_status_->setStyleSheet("font-weight: bold; color: #dc3545;");
    }

    for (int i = 0; i < transfer_chunks_.size(); ++i) {
        services::TransferService::ChunkProgress prog = transfer_service_.get_chunk_progress(i);
        QString state_str = "UNSENT";
        if (prog.state == services::TransferService::ChunkState::IN_FLIGHT) state_str = "IN_FLIGHT";
        else if (prog.state == services::TransferService::ChunkState::ACKNOWLEDGED) state_str = "ACKNOWLEDGED";
        else if (prog.state == services::TransferService::ChunkState::TIMED_OUT) state_str = "TIMED_OUT";
        else if (prog.state == services::TransferService::ChunkState::FAILED) state_str = "FAILED";

        table_chunks_->setItem(i, 1, new QTableWidgetItem(state_str));
        table_chunks_->setItem(i, 2, new QTableWidgetItem(QString::number(prog.attempts)));
    }
}

void MainWindow::update_statistics_display() {
    services::SimulationService::Statistics sim_stats = simulation_service_.get_statistics();
    services::TransferService::TransferStatistics tr_stats = transfer_service_.get_statistics();

    QString summary = QString("Sent: %1 | Delivered: %2 | Dropped: %3 | Lost: %4 | Corrupted: %5 | Heartbeats: %6 | Acks: %7 | Retries: %8")
                          .arg(sim_stats.sent_count)
                          .arg(sim_stats.delivered_count)
                          .arg(sim_stats.dropped_count)
                          .arg(sim_stats.lost_count)
                          .arg(sim_stats.corrupted_count)
                          .arg(sim_stats.heartbeat_timeout_count)
                          .arg(tr_stats.acknowledged_count)
                          .arg(tr_stats.retransmitted_count);
    lbl_stats_summary_->setText(summary);
}

void MainWindow::append_log_entry(int tick, const string& source, const string& desc, const string& details) {
    int row = table_event_log_->rowCount();
    table_event_log_->insertRow(row);
    table_event_log_->setItem(row, 0, new QTableWidgetItem(QString::number(tick) + " ms"));
    table_event_log_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(source)));
    table_event_log_->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(desc)));
    table_event_log_->setItem(row, 3, new QTableWidgetItem(QString::fromStdString(details)));
    table_event_log_->scrollToBottom();
}

void MainWindow::handle_save_config(const string& file_path) {
    try {
        services::FileStorageService::SimulationConfig cfg =
            storage_service_.extract_config_from_graph(graph_, 12345, 1024, active_source_id_, active_destination_id_);
        storage_service_.save_config(file_path, cfg);
        append_log_entry(simulation_service_.get_current_tick(), "Storage", "Saved configuration to " + file_path, "");
    } catch (const exception& e) {
        QMessageBox::critical(this, "Save Error", QString::fromStdString(e.what()));
    }
}

void MainWindow::handle_load_config(const string& file_path) {
    try {
        services::FileStorageService::SimulationConfig cfg = storage_service_.load_config(file_path);
        handle_reset_simulation();
        graph_ = model::NetworkGraph();
        storage_service_.apply_config_to_graph(cfg, graph_);
        canvas_->update_layout();
        refresh_client_combos();
        canvas_->update();
        update_route_display();
        append_log_entry(simulation_service_.get_current_tick(), "Storage", "Loaded configuration from " + file_path, "");
    } catch (const exception& e) {
        QMessageBox::critical(this, "Load Error", QString::fromStdString(e.what()));
    }
}

void MainWindow::handle_export_experiment_results(const string& file_path) {
    try {
        services::FileStorageService::ExperimentResult result;
        result.experiment_id = "EXP-" + to_string(QDateTime::currentMSecsSinceEpoch());
        result.outcome = transfer_service_.is_transfer_completed() ? "SUCCESS" : "FAILURE";
        result.total_ticks = simulation_service_.get_current_tick();
        result.config = storage_service_.extract_config_from_graph(graph_, 12345, 1024, active_source_id_, active_destination_id_);
        result.simulation_stats = simulation_service_.get_statistics();
        result.transfer_stats = transfer_service_.get_statistics();

        for (int r = 0; r < table_event_log_->rowCount(); ++r) {
            QString entry = QString("[%1] %2: %3 (%4)")
                                .arg(table_event_log_->item(r, 0)->text())
                                .arg(table_event_log_->item(r, 1)->text())
                                .arg(table_event_log_->item(r, 2)->text())
                                .arg(table_event_log_->item(r, 3)->text());
            result.packet_events.push_back(entry.toStdString());
        }

        storage_service_.save_experiment_result(file_path, result);
        append_log_entry(simulation_service_.get_current_tick(), "Storage", "Exported experiment results to " + file_path, "");
    } catch (const exception& e) {
        QMessageBox::critical(this, "Export Error", QString::fromStdString(e.what()));
    }
}

void MainWindow::handle_send_route_probe() {
    if (active_source_id_ == -1 || active_destination_id_ == -1) {
        const dsa::DynamicArray<int>& clients = graph_.get_client_ids();
        if (clients.size() >= 2) {
            active_source_id_ = clients[0];
            active_destination_id_ = clients[1];
        } else if (combo_src_client_->count() > 0 && combo_dst_client_->count() > 1) {
            active_source_id_ = combo_src_client_->currentData().toInt();
            active_destination_id_ = combo_dst_client_->currentData().toInt();
        } else {
            return;
        }
    }

    services::RoutingService::Route r = routing_service_.find_route(graph_, active_source_id_, active_destination_id_);
    if (!r.found || r.path.size() < 2) {
        append_log_entry(simulation_service_.get_current_tick(), "Probe", "Cannot send route probe: destination unreachable", "");
        return;
    }

    string path_str = "";
    for (int i = 0; i < r.path.size(); ++i) {
        path_str += to_string(r.path[i]);
        if (i < r.path.size() - 1) path_str += " -> ";
    }

    model::Packet probe(model::Packet::Type::ROUTE_PROBE, active_source_id_, active_destination_id_,
                        r.hop_count(), path_str, 64, 0);

    simulation_service_.send_packet_along_route(r.path, probe);
    append_log_entry(simulation_service_.get_current_tick(), "Probe",
                     "Dispatched hop-by-hop ROUTE_PROBE from Client " + to_string(active_source_id_) + " to Client " + to_string(active_destination_id_),
                     "Planned Route: " + path_str);
}

void MainWindow::handle_run_recovery_demonstration() {
    // Coherent Scenario 1: Setup -> Probe -> Transfer -> Failure -> Reroute -> Recovery -> File Reconstruct
    handle_generate_random_topology(6, 2, 0.45, 12345);
    handle_validate_topology();

    active_source_id_ = 1;
    active_destination_id_ = 2;

    append_log_entry(0, "Demo 1", "=== STARTING COHERENT DEMO 1: DYNAMIC FAILURE RECOVERY ===", "");

    // 1. Send hop-by-hop route probe
    handle_send_route_probe();

    // 2. Start reliable multi-chunk file transfer
    string payload = "Demonstration Payload for PBL2 Simulator: Verifying seamless retransmission and Dijkstra dynamic rerouting upon network failure!";
    handle_start_file_transfer(1, 2, payload, 32);

    // 3. Advance ticks to put packets in flight
    handle_advance_ticks(20);

    // 4. Inject mid-transfer failure on intermediate router along active route
    services::RoutingService::Route r = routing_service_.find_route(graph_, 1, 2);
    if (r.found && r.path.size() >= 3) {
        int failed_router = r.path[1];
        simulation_service_.fail_node_manually(failed_router);
        append_log_entry(simulation_service_.get_current_tick(), "Demo 1",
                         "INJECTED FAILURE: Disabled Router ID " + to_string(failed_router) + " mid-transfer!",
                         "Triggered immediate Dijkstra rerouting");
        update_route_display();
    }

    // 5. Advance ticks through timeout, retransmission over alternate route, and final delivery
    handle_advance_ticks(350);

    // 6. Verify and save results
    if (transfer_service_.is_transfer_completed()) {
        append_log_entry(simulation_service_.get_current_tick(), "Demo 1",
                         "DEMO 1 SUCCESS: File reconstructed with 100% integrity despite failure!",
                         "Saved to data/experiments/demonstration_recovery.json");
        handle_export_experiment_results("data/experiments/demonstration_recovery.json");
    }
}

void MainWindow::handle_run_retry_limit_demonstration() {
    // Coherent Scenario 2: Setup -> Transfer -> Severe Disconnection -> Consecutive Timeouts -> Limit Reached
    handle_generate_random_topology(6, 2, 0.45, 12345);
    active_source_id_ = 1;
    active_destination_id_ = 2;

    append_log_entry(0, "Demo 2", "=== STARTING COHERENT DEMO 2: RETRY-LIMIT EXHAUSTION ===", "");

    // 1. Start transfer
    string payload = "Controlled Retry Limit Failure Test Payload";
    handle_start_file_transfer(1, 2, payload, 64);
    handle_advance_ticks(5);

    // 2. Sever all links to destination Client 2
    const dsa::DynamicArray<int>& neighbors = graph_.get_neighbors(2);
    for (int i = 0; i < neighbors.size(); ++i) {
        simulation_service_.fail_link_manually(2, neighbors[i]);
    }
    append_log_entry(simulation_service_.get_current_tick(), "Demo 2",
                     "ISOLATED DESTINATION: Severed all links to Client 2", "Destination completely unreachable");
    update_route_display();

    // 3. Advance ticks past 3 timeout intervals (3 * 100 ms + margin = 350 ms)
    handle_advance_ticks(350);

    // 4. Verify failure and save results
    if (transfer_service_.is_transfer_failed()) {
        append_log_entry(simulation_service_.get_current_tick(), "Demo 2",
                         "DEMO 2 SUCCESS: Transfer correctly aborted after exceeding maximum retry limit (3 attempts)!",
                         "Evidence preserved in statistics");
        handle_export_experiment_results("data/experiments/demonstration_failure.json");
    }
}

string MainWindow::get_discovered_probe_path() const {
    return discovered_probe_path_;
}

}
}
