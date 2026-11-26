#include "ui/MainWindow.h"

#include <algorithm>
#include <exception>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIntValidator>
#include <QKeySequence>
#include <QMessageBox>
#include <QShortcut>
#include <QSplitter>
#include <QTabWidget>
#include <QVBoxLayout>

#include "ui/UiTheme.h"

namespace pbl {
namespace ui {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    sim_timer_ = nullptr;
    demo_timer_ = nullptr;
    xfer_timer_ = nullptr;
    canvas_ = nullptr;

    active_source_id_ = -1;
    active_destination_id_ = -1;
    next_chunk_to_send_idx_ = 0;
    last_processed_delivered_idx_ = 0;
    discovered_probe_path_ = "";
    topology_modified_manually_ = false;
    sim_interval_ms_ = 50;

    demo_mode_ = 0;
    demo_step_ = 0;
    demo_watch_stage_ = 0;
    demo_failed_router_ = -1;
    xfer_step_active_ = false;
    xfer_step_ = 0;
    xfer_watch_stage_ = 0;
    xfer_src_id_ = -1;
    xfer_dst_id_ = -1;
    xfer_chunk_size_ = 1024;

    simulation_engine_.set_network_graph(&graph_);
    simulation_engine_.set_routing_engine(&routing_engine_);

    setup_ui();
    setStyleSheet(build_dark_stylesheet());

    sim_timer_ = new QTimer(this);
    connect(sim_timer_, &QTimer::timeout, this, &MainWindow::on_simulation_timer_tick);

    demo_timer_ = new QTimer(this);
    connect(demo_timer_, &QTimer::timeout, this, &MainWindow::on_demo_timer_tick);

    xfer_timer_ = new QTimer(this);
    connect(xfer_timer_, &QTimer::timeout, this, &MainWindow::on_xfer_timer_tick);

    setup_initial_startup_topology();

    new QShortcut(QKeySequence::Undo, this, [this]() { handle_undo_topology(); });

    setWindowTitle("PBL2 Network Protocol & Dijkstra Simulation Dashboard");
    resize(1280, 820);
}

model::NetworkGraph& MainWindow::get_network_graph() { return graph_; }
engine::RoutingEngine& MainWindow::get_routing_engine() { return routing_engine_; }
engine::SimulationEngine& MainWindow::get_simulation_engine() { return simulation_engine_; }
engine::TransferEngine& MainWindow::get_transfer_engine() { return transfer_engine_; }
TopologyCanvas* MainWindow::get_canvas() { return canvas_; }

void MainWindow::setup_ui() {
    QWidget* central = new QWidget(this);
    QVBoxLayout* main_layout = new QVBoxLayout(central);
    main_layout->setSpacing(6);
    main_layout->setContentsMargins(8, 8, 8, 8);

    main_layout->addWidget(create_top_toolbar());

    QSplitter* mid_splitter = new QSplitter(Qt::Horizontal, central);
    canvas_ = new TopologyCanvas(mid_splitter);
    canvas_->set_network_graph(&graph_);
    canvas_->set_routing_engine(&routing_engine_);
    canvas_->set_simulation_engine(&simulation_engine_);

    connect(canvas_, &TopologyCanvas::node_selected, this, &MainWindow::on_node_selected);
    connect(canvas_, &TopologyCanvas::link_selected, this, &MainWindow::on_link_selected);
    connect(canvas_, &TopologyCanvas::canvas_clicked, this, &MainWindow::on_canvas_clicked);

    // Canvas right-click context menu actions
    connect(canvas_, &TopologyCanvas::toggle_node_failure_requested, this, [this](int node_id) {
        canvas_->set_selected_node(node_id);
        handle_toggle_failure_selected();
        on_node_selected(node_id);
    });
    connect(canvas_, &TopologyCanvas::delete_node_requested, this, [this](int node_id) {
        push_topology_undo();
        graph_.remove_node(node_id);
        canvas_->set_selected_node(-1);
        topology_modified_manually_ = true;
        refresh_client_combos();
        canvas_->update();
        update_route_display();
        update_queue_table();
        fault_panel_->reset_selection_prompt();
    });
    connect(canvas_, &TopologyCanvas::connect_link_requested, this, [this](int u, int v) {
        if (graph_.has_node(u) && graph_.has_node(v) && u != v && graph_.get_link(u, v) == nullptr) {
            push_topology_undo();
            graph_.add_link(u, v, 100, 10, 0.0, 0.0, 10);
            topology_modified_manually_ = true;
            canvas_->update();
            update_route_display();
            update_queue_table();
            append_log_entry(simulation_engine_.get_current_tick(), "Topology",
                             "Connected link " + std::to_string(u) + " <-> " + std::to_string(v), "100 KB/s, 10 ms");
        }
    });
    connect(canvas_, &TopologyCanvas::toggle_link_failure_requested, this, [this](int u, int v) {
        canvas_->set_selected_link(u, v);
        handle_toggle_failure_selected();
        on_link_selected(u, v);
    });
    connect(canvas_, &TopologyCanvas::edit_link_params_requested, this, [this](int u, int v) {
        model::Link* l = graph_.get_link(u, v);
        if (!l) return;
        QDialog dlg(this);
        dlg.setWindowTitle(QString("Edit Link %1 <-> %2").arg(u).arg(v));
        QFormLayout* form = new QFormLayout(&dlg);
        QSpinBox* spin_lat = new QSpinBox(&dlg);
        spin_lat->setRange(1, 500);
        spin_lat->setValue(l->get_latency());
        QSpinBox* spin_bw = new QSpinBox(&dlg);
        spin_bw->setRange(1, 10000);
        spin_bw->setValue(l->get_bandwidth());
        form->addRow("Latency (ms):", spin_lat);
        form->addRow("Bandwidth (KB/s):", spin_bw);
        QDialogButtonBox* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
        connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
        connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
        form->addRow(btns);
        if (dlg.exec() == QDialog::Accepted) {
            push_topology_undo();
            l->set_latency(spin_lat->value());
            l->set_bandwidth(spin_bw->value());
            topology_modified_manually_ = true;
            canvas_->set_selected_link(u, v);
            canvas_->update();
            update_route_display();
            update_queue_table();
            on_link_selected(u, v);
            append_log_entry(simulation_engine_.get_current_tick(), "Topology",
                             "Updated Link " + std::to_string(u) + " <-> " + std::to_string(v) +
                             " -> " + std::to_string(spin_lat->value()) + "ms, " + std::to_string(spin_bw->value()) + "KB/s",
                             "Dijkstra shortest path recalculated");
        }
    });
    connect(canvas_, &TopologyCanvas::inject_link_loss_requested, this, [this](int u, int v) {
        canvas_->set_selected_link(u, v);
        handle_inject_loss_selected();
    });
    connect(canvas_, &TopologyCanvas::inject_link_corruption_requested, this, [this](int u, int v) {
        canvas_->set_selected_link(u, v);
        handle_inject_corruption_selected();
    });
    connect(canvas_, &TopologyCanvas::delete_link_requested, this, [this](int u, int v) {
        push_topology_undo();
        graph_.remove_link(u, v);
        canvas_->set_selected_link(-1, -1);
        topology_modified_manually_ = true;
        canvas_->update();
        update_route_display();
        update_queue_table();
        fault_panel_->reset_selection_prompt();
    });

    mid_splitter->addWidget(canvas_);
    mid_splitter->addWidget(create_sidebar_tabs());
    mid_splitter->setStretchFactor(0, 6);
    mid_splitter->setStretchFactor(1, 4);

    QSplitter* v_splitter = new QSplitter(Qt::Vertical, central);
    v_splitter->addWidget(mid_splitter);
    v_splitter->addWidget(create_bottom_panel());
    v_splitter->setStretchFactor(0, 7);
    v_splitter->setStretchFactor(1, 3);

    main_layout->addWidget(v_splitter);
    setCentralWidget(central);
}

QWidget* MainWindow::create_top_toolbar() {
    QGroupBox* bar_box = new QGroupBox("Simulation Controls • Status", this);
    QHBoxLayout* layout = new QHBoxLayout(bar_box);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(8);

    btn_start_ = new QPushButton("Start / Reset", bar_box);
    btn_start_->setToolTip("Start simulation playback (auto-launches a transfer if idle, or resets if finished).");
    btn_start_->setStyleSheet("background-color: #10b981; color: white; font-weight: bold; border: none; border-radius: 6px; padding: 6px 14px;");
    btn_pause_ = new QPushButton("Pause", bar_box);
    btn_pause_->setToolTip("Pause the simulation clock.");
    btn_pause_->setStyleSheet("background-color: #f59e0b; color: white; font-weight: bold; border: none; border-radius: 6px; padding: 6px 14px;");
    btn_resume_ = new QPushButton("Resume", bar_box);
    btn_resume_->setToolTip("Resume simulation playback from the current tick.");
    btn_resume_->setStyleSheet("background-color: #06b6d4; color: white; font-weight: bold; border: none; border-radius: 6px; padding: 6px 14px;");
    btn_reset_ = new QPushButton("Reset", bar_box);
    btn_reset_->setToolTip("Stop simulation, reset Tick to 0 ms, and clear in-flight packets.");
    btn_reset_->setStyleSheet("background-color: #ef4444; color: white; font-weight: bold; border: none; border-radius: 6px; padding: 6px 14px;");
    btn_step1_ = new QPushButton("Step +1ms", bar_box);
    btn_step1_->setToolTip("Advance the simulation clock by exactly 1 ms.");
    btn_step10_ = new QPushButton("Step +10ms", bar_box);
    btn_step10_->setToolTip("Advance the simulation clock by 10 ms.");

    edit_custom_step_ = new QLineEdit(bar_box);
    edit_custom_step_->setValidator(new QIntValidator(1, 5000, edit_custom_step_));
    edit_custom_step_->setText("25");
    edit_custom_step_->setPlaceholderText("ms");
    edit_custom_step_->setAlignment(Qt::AlignCenter);
    edit_custom_step_->setFixedWidth(68);
    edit_custom_step_->setToolTip("Type any number of ms (1 - 5000) and click 'Step +' or press Enter.");

    btn_step_custom_ = new QPushButton("Step +", bar_box);
    btn_step_custom_->setToolTip("Advance the simulation clock by the number entered in the box.");
    btn_step_custom_->setStyleSheet("background-color: #1e293b; color: #38bdf8; border: 1px solid #0284c7; border-radius: 6px; padding: 6px 10px; font-weight: bold;");

    auto step_by_ticks = [this](int ticks) {
        if (transfer_chunks_.empty()) {
            int src_id = transfer_panel_->selected_src_id();
            int dst_id = transfer_panel_->selected_dst_id();
            if (src_id != -1 && dst_id != -1 && src_id != dst_id) {
                initialize_transfer_session(src_id, dst_id, transfer_panel_->payload_text(), transfer_panel_->chunk_size(), false);
            }
        }
        handle_advance_ticks(ticks);
    };

    auto apply_custom_step = [this, step_by_ticks]() {
        bool ok = false;
        int val = edit_custom_step_->text().toInt(&ok);
        if (!ok || val < 1) val = 1;
        if (val > 5000) val = 5000;
        edit_custom_step_->setText(QString::number(val));
        step_by_ticks(val);
    };

    connect(btn_start_, &QPushButton::clicked, this, [this]() {
        if (transfer_chunks_.empty() || transfer_engine_.is_transfer_completed() || transfer_engine_.is_transfer_failed()) {
            int src_id = transfer_panel_->selected_src_id();
            int dst_id = transfer_panel_->selected_dst_id();
            if (src_id != -1 && dst_id != -1 && src_id != dst_id) {
                handle_start_file_transfer(src_id, dst_id, transfer_panel_->payload_text(), transfer_panel_->chunk_size());
                return;
            }
        }
        handle_start_simulation();
    });
    connect(btn_pause_, &QPushButton::clicked, this, [this]() { handle_pause_simulation(); });
    connect(btn_resume_, &QPushButton::clicked, this, [this]() { handle_resume_simulation(); });
    connect(btn_reset_, &QPushButton::clicked, this, [this]() { handle_reset_simulation(); });
    connect(btn_step1_, &QPushButton::clicked, this, [step_by_ticks]() { step_by_ticks(1); });
    connect(btn_step10_, &QPushButton::clicked, this, [step_by_ticks]() { step_by_ticks(10); });
    connect(btn_step_custom_, &QPushButton::clicked, this, apply_custom_step);
    connect(edit_custom_step_, &QLineEdit::returnPressed, this, apply_custom_step);

    QLabel* lbl_speed = new QLabel("Speed:", bar_box);
    lbl_speed->setStyleSheet("color: #94a3b8; font-weight: bold;");
    combo_sim_speed_ = new QComboBox(bar_box);
    combo_sim_speed_->addItem("0.25x", 200);
    combo_sim_speed_->addItem("0.5x", 100);
    combo_sim_speed_->addItem("1x", 50);
    combo_sim_speed_->addItem("2x", 25);
    combo_sim_speed_->addItem("5x", 10);
    combo_sim_speed_->setCurrentIndex(2);
    combo_sim_speed_->setToolTip("Adjust simulation timer playback speed");
    connect(combo_sim_speed_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::on_speed_changed);

    lbl_sim_status_ = new QLabel("Status: STOPPED", bar_box);
    lbl_sim_status_->setStyleSheet("background-color: rgba(100, 116, 139, 0.2); color: #94a3b8; border: 1px solid #64748b; border-radius: 12px; padding: 4px 12px; font-weight: bold; font-size: 11px;");
    lbl_sim_tick_ = new QLabel("Tick: 0 ms", bar_box);
    lbl_sim_tick_->setStyleSheet("background-color: rgba(56, 189, 248, 0.15); color: #38bdf8; border: 1px solid #0284c7; border-radius: 12px; padding: 4px 12px; font-weight: bold; font-size: 11px;");

    btn_save_cfg_ = new QPushButton("Save Config", bar_box);
    btn_load_cfg_ = new QPushButton("Load Config", bar_box);
    btn_export_exp_ = new QPushButton("Export Experiment", bar_box);

    connect(btn_save_cfg_, &QPushButton::clicked, this, [this]() {
        QString file = QFileDialog::getSaveFileName(this, "Save Topology Config", "data/configs/topology.json", "JSON (*.json)");
        if (!file.isEmpty()) handle_save_config(file.toStdString());
    });
    connect(btn_load_cfg_, &QPushButton::clicked, this, [this]() {
        QString file = QFileDialog::getOpenFileName(this, "Load Topology Config", "data/configs", "JSON (*.json)");
        if (!file.isEmpty()) handle_load_config(file.toStdString());
    });
    connect(btn_export_exp_, &QPushButton::clicked, this, [this]() {
        QString file = QFileDialog::getSaveFileName(this, "Export Experiment Results", "data/experiments/results.json", "JSON (*.json)");
        if (!file.isEmpty()) handle_export_experiment_results(file.toStdString());
    });

    layout->addWidget(btn_start_);
    layout->addWidget(btn_pause_);
    layout->addWidget(btn_resume_);
    layout->addWidget(btn_reset_);
    layout->addWidget(btn_step1_);
    layout->addWidget(btn_step10_);
    layout->addWidget(edit_custom_step_);
    layout->addWidget(btn_step_custom_);
    layout->addWidget(lbl_speed);
    layout->addWidget(combo_sim_speed_);
    layout->addSpacing(8);
    layout->addWidget(lbl_sim_status_);
    layout->addWidget(lbl_sim_tick_);
    layout->addStretch();
    layout->addWidget(btn_save_cfg_);
    layout->addWidget(btn_load_cfg_);
    layout->addWidget(btn_export_exp_);

    return bar_box;
}

QWidget* MainWindow::create_sidebar_tabs() {
    QTabWidget* tabs = new QTabWidget(this);

    topology_panel_ = new TopologyPanel(tabs);
    transfer_panel_ = new TransferPanel(tabs);
    fault_panel_ = new FaultInspectorPanel(tabs);
    routing_panel_ = new RoutingPanel(tabs);
    demo_panel_ = new DemoPanel(tabs);

    // Tab 1 wiring
    connect(topology_panel_, &TopologyPanel::generateRequested, this, [this](int n, int c, double d, int s) {
        handle_generate_random_topology(n, c, d, s);
        topology_modified_manually_ = true;
    });
    connect(topology_panel_, &TopologyPanel::validateRequested, this, [this]() {
        std::string res = handle_validate_topology();
        QMessageBox::information(this, "Topology Validation", QString::fromStdString(res));
    });
    connect(topology_panel_, &TopologyPanel::addNodeRequested, this, [this](const std::string& name, const std::string& type) {
        push_topology_undo();
        int new_id = 1;
        while (graph_.has_node(new_id)) new_id++;
        std::string final_name = name.empty() ? (type + std::to_string(new_id)) : name;
        if (type == "Client") {
            graph_.add_client(new_id, final_name);
        } else {
            graph_.add_router(new_id, final_name);
        }
        topology_modified_manually_ = true;
        canvas_->update_layout();
        refresh_client_combos();
        canvas_->update();
        update_route_display();
        update_queue_table();
        append_log_entry(simulation_engine_.get_current_tick(), "Topology",
                         "Added " + type + " " + final_name + " (ID " + std::to_string(new_id) + ")", "");
    });
    connect(topology_panel_, &TopologyPanel::addLinkRequested, this, [this](int u, int v, int bw, int lat, double loss) {
        if (!graph_.has_node(u) || !graph_.has_node(v) || u == v || graph_.get_link(u, v) != nullptr) {
            QMessageBox::warning(this, "Link Error", "Cannot create link. Ensure both nodes exist, are distinct, and are not already linked.");
        } else {
            push_topology_undo();
            graph_.add_link(u, v, bw, lat, loss, 0.0, 10);
            topology_modified_manually_ = true;
            canvas_->update();
            update_route_display();
            update_queue_table();
            append_log_entry(simulation_engine_.get_current_tick(), "Topology",
                             "Added link " + std::to_string(u) + " <-> " + std::to_string(v),
                             std::to_string(bw) + " KB/s, " + std::to_string(lat) + " ms");
        }
    });
    connect(topology_panel_, &TopologyPanel::deleteSelectedRequested, this, [this]() {
        int sel_node = canvas_->get_selected_node();
        std::pair<int, int> sel_link = canvas_->get_selected_link();
        if (sel_node != -1) {
            push_topology_undo();
            graph_.remove_node(sel_node);
            canvas_->set_selected_node(-1);
            topology_modified_manually_ = true;
            refresh_client_combos();
            canvas_->update();
            update_route_display();
            update_queue_table();
            fault_panel_->reset_selection_prompt();
        } else if (sel_link.first != -1) {
            push_topology_undo();
            graph_.remove_link(sel_link.first, sel_link.second);
            canvas_->set_selected_link(-1, -1);
            topology_modified_manually_ = true;
            canvas_->update();
            update_route_display();
            update_queue_table();
            fault_panel_->reset_selection_prompt();
        }
    });
    connect(topology_panel_, &TopologyPanel::undoRequested, this, &MainWindow::handle_undo_topology);

    // Tab 2 wiring
    connect(transfer_panel_, &TransferPanel::startTransferRequested, this, &MainWindow::start_guided_transfer);
    connect(transfer_panel_, &TransferPanel::prevStepRequested, this, &MainWindow::on_xfer_prev_step);
    connect(transfer_panel_, &TransferPanel::nextStepRequested, this, &MainWindow::on_xfer_next_step);
    connect(transfer_panel_, &TransferPanel::toggleAutoRequested, this, &MainWindow::on_xfer_toggle_auto);

    // Tab 3 wiring
    connect(fault_panel_, &FaultInspectorPanel::toggleFailureRequested, this, &MainWindow::handle_toggle_failure_selected);
    connect(fault_panel_, &FaultInspectorPanel::injectLossRequested, this, &MainWindow::handle_inject_loss_selected);
    connect(fault_panel_, &FaultInspectorPanel::injectCorruptionRequested, this, &MainWindow::handle_inject_corruption_selected);
    connect(fault_panel_, &FaultInspectorPanel::linkParamsUpdateRequested, this, [this](int latency_ms, int bandwidth_kbps) {
        std::pair<int, int> sel_link = canvas_->get_selected_link();
        if (sel_link.first == -1) return;
        model::Link* l = graph_.get_link(sel_link.first, sel_link.second);
        if (!l) return;
        push_topology_undo();
        l->set_latency(latency_ms);
        l->set_bandwidth(bandwidth_kbps);
        topology_modified_manually_ = true;
        canvas_->update();
        update_route_display();
        update_queue_table();
        on_link_selected(sel_link.first, sel_link.second);
        append_log_entry(simulation_engine_.get_current_tick(), "Topology",
                         "Updated Link " + std::to_string(sel_link.first) + " <-> " + std::to_string(sel_link.second) +
                         " -> " + std::to_string(latency_ms) + "ms, " + std::to_string(bandwidth_kbps) + "KB/s",
                         "Dijkstra shortest path recalculated");
    });

    // Tab 5 wiring
    connect(demo_panel_, &DemoPanel::probeRequested, this, [this]() {
        int src_id = transfer_panel_->selected_src_id();
        int dst_id = transfer_panel_->selected_dst_id();
        if (src_id == -1 || dst_id == -1) {
            if (active_source_id_ == -1 || active_destination_id_ == -1) return;
            src_id = active_source_id_;
            dst_id = active_destination_id_;
        } else {
            active_source_id_ = src_id;
            active_destination_id_ = dst_id;
        }
        engine::RoutingEngine::Route r = routing_engine_.find_route(graph_, src_id, dst_id);
        if (!r.found || r.path.size() < 2) {
            demo_panel_->set_live_step_text("Route Probe failed: No reachable path between selected clients.");
            append_log_entry(simulation_engine_.get_current_tick(), "Probe", "Route Probe failed: No valid path", "");
            return;
        }
        std::string route_trace;
        for (int i = 0; i < r.path.size(); ++i) {
            route_trace += std::to_string(r.path[i]);
            if (i < r.path.size() - 1) route_trace += "->";
        }
        canvas_->clear_probe_visuals();
        canvas_->set_active_route(r.path);
        update_route_display();
        model::Packet probe_pkt(model::Packet::Type::ROUTE_PROBE, src_id, dst_id, r.hop_count(), route_trace, 16);
        simulation_engine_.send_packet_along_route(r.path, probe_pkt);
        append_log_entry(simulation_engine_.get_current_tick(), "Probe",
                         "Dispatched Hop-by-Hop ROUTE_PROBE (" + std::to_string(src_id) + " -> " + std::to_string(dst_id) + ")",
                         "Expected Trace: " + route_trace);
        demo_watch_stage_ = 20;
        demo_panel_->set_live_step_text(QString("Route Probe In-Flight: Tracing hop-by-hop path %1...").arg(QString::fromStdString(route_trace)));
        handle_start_simulation();
    });
    connect(demo_panel_, &DemoPanel::scenario1Requested, this, [this]() {
        if (confirm_preset_demo_if_modified()) start_animated_demo1();
    });
    connect(demo_panel_, &DemoPanel::scenario2Requested, this, [this]() {
        if (confirm_preset_demo_if_modified()) start_animated_demo2();
    });
    connect(demo_panel_, &DemoPanel::prevStepRequested, this, &MainWindow::on_demo_prev_step);
    connect(demo_panel_, &DemoPanel::nextStepRequested, this, &MainWindow::on_demo_next_step);
    connect(demo_panel_, &DemoPanel::toggleAutoRequested, this, &MainWindow::on_demo_toggle_auto);

    tabs->addTab(topology_panel_, "Topology");
    tabs->addTab(transfer_panel_, "Transfer");
    tabs->addTab(fault_panel_, "Inspector • Faults");
    tabs->addTab(routing_panel_, "Routing • Queues");
    tabs->addTab(demo_panel_, "Demonstrations");

    return tabs;
}

QWidget* MainWindow::create_bottom_panel() {
    QGroupBox* bottom_box = new QGroupBox("Packet Events • Statistics", this);
    QVBoxLayout* layout = new QVBoxLayout(bottom_box);

    QHBoxLayout* top_row = new QHBoxLayout();
    lbl_stats_summary_ = new QLabel(
        "Sent: 0 | Delivered: 0 | Dropped: 0 | Lost: 0 | Corrupted: 0 | Heartbeats: 0 | Acks: 0 | Retries: 0",
        bottom_box);
    lbl_stats_summary_->setStyleSheet("font-weight: bold; color: #f1f5f9; font-size: 13px;");
    btn_clear_log_ = new QPushButton("Clear Log", bottom_box);
    connect(btn_clear_log_, &QPushButton::clicked, this, [this]() { table_event_log_->setRowCount(0); });

    top_row->addWidget(lbl_stats_summary_);
    top_row->addStretch();
    top_row->addWidget(btn_clear_log_);
    layout->addLayout(top_row);

    table_event_log_ = new QTableWidget(bottom_box);
    table_event_log_->setColumnCount(4);
    table_event_log_->setHorizontalHeaderLabels({"Tick", "Source", "Event Description", "Details"});
    table_event_log_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table_event_log_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table_event_log_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    table_event_log_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    table_event_log_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_event_log_->setAlternatingRowColors(true);
    layout->addWidget(table_event_log_);

    return bottom_box;
}

void MainWindow::on_speed_changed(int index) {
    if (!combo_sim_speed_ || index < 0) return;
    sim_interval_ms_ = combo_sim_speed_->itemData(index).toInt();
    if (sim_timer_ && sim_timer_->isActive()) {
        sim_timer_->setInterval(sim_interval_ms_);
    }
}

void MainWindow::handle_start_simulation() {
    simulation_engine_.start();
    if (!sim_timer_->isActive()) {
        sim_timer_->start(sim_interval_ms_);
    }
    lbl_sim_status_->setText("Status: RUNNING");
    lbl_sim_status_->setStyleSheet("background-color: rgba(16, 185, 129, 0.2); color: #34d399; border: 1px solid #10b981; border-radius: 12px; padding: 4px 12px; font-weight: bold; font-size: 11px;");
    append_log_entry(simulation_engine_.get_current_tick(), "Simulation", "Simulation started", "");
}

void MainWindow::handle_pause_simulation() {
    simulation_engine_.pause();
    sim_timer_->stop();
    lbl_sim_status_->setText("Status: PAUSED");
    lbl_sim_status_->setStyleSheet("background-color: rgba(245, 158, 11, 0.2); color: #fbbf24; border: 1px solid #f59e0b; border-radius: 12px; padding: 4px 12px; font-weight: bold; font-size: 11px;");
    append_log_entry(simulation_engine_.get_current_tick(), "Simulation", "Simulation paused", "");
}

void MainWindow::handle_resume_simulation() {
    simulation_engine_.resume();
    if (!sim_timer_->isActive()) {
        sim_timer_->start(sim_interval_ms_);
    }
    lbl_sim_status_->setText("Status: RUNNING");
    lbl_sim_status_->setStyleSheet("background-color: rgba(16, 185, 129, 0.2); color: #34d399; border: 1px solid #10b981; border-radius: 12px; padding: 4px 12px; font-weight: bold; font-size: 11px;");
    append_log_entry(simulation_engine_.get_current_tick(), "Simulation", "Simulation resumed", "");
}

void MainWindow::handle_reset_simulation() {
    sim_timer_->stop();
    if (demo_timer_) demo_timer_->stop();
    if (xfer_timer_) xfer_timer_->stop();
    demo_mode_ = 0;
    demo_step_ = 0;
    demo_watch_stage_ = 0;
    xfer_step_active_ = false;
    xfer_step_ = 0;
    xfer_watch_stage_ = 0;
    demo_panel_->update_nav_buttons(0, 0, 0, false);
    transfer_panel_->update_nav_buttons(false, 0, 5, false);

    simulation_engine_.reset();
    active_source_id_ = -1;
    active_destination_id_ = -1;
    next_chunk_to_send_idx_ = 0;
    last_processed_delivered_idx_ = 0;
    discovered_probe_path_ = "";
    transfer_chunks_.clear();

    lbl_sim_status_->setText("Status: STOPPED");
    lbl_sim_status_->setStyleSheet("background-color: rgba(100, 116, 139, 0.2); color: #94a3b8; border: 1px solid #64748b; border-radius: 12px; padding: 4px 12px; font-weight: bold; font-size: 11px;");
    lbl_sim_tick_->setText("Tick: 0 ms");
    transfer_panel_->reset_display();

    canvas_->clear_probe_visuals();
    canvas_->set_active_route(dsa::DynamicArray<int>());
    canvas_->update();
    update_route_display();
    update_queue_table();
    update_statistics_display();
    append_log_entry(0, "Simulation", "Simulation clock and metrics reset", "");
}

void MainWindow::handle_single_tick() {
    handle_advance_ticks(1);
}

void MainWindow::handle_advance_ticks(int tick_count) {
    for (int i = 0; i < tick_count; ++i) {
        simulation_engine_.tick();
        execute_transfer_step();
    }
    lbl_sim_tick_->setText(QString("Tick: %1 ms").arg(simulation_engine_.get_current_tick()));
    canvas_->update();
    update_transfer_display();
    update_queue_table();
    update_statistics_display();
}

void MainWindow::on_simulation_timer_tick() {
    handle_advance_ticks(1);

    auto has_ack_in_flight = [this]() -> bool {
        const dsa::DynamicArray<engine::SimulationEngine::InFlightPacket>& pkts = simulation_engine_.get_in_flight_packets();
        for (int i = 0; i < pkts.size(); ++i) {
            if (pkts[i].packet.get_type() == model::Packet::Type::ACK) return true;
        }
        return false;
    };

    if (xfer_step_active_ && xfer_watch_stage_ != 0) {
        auto stats = transfer_engine_.get_statistics();
        if (xfer_watch_stage_ == 1) {
            if (has_ack_in_flight() || stats.acknowledged_count > 0 ||
                transfer_engine_.is_transfer_completed() || transfer_engine_.is_transfer_failed()) {
                xfer_watch_stage_ = 0;
                handle_pause_simulation();
                transfer_panel_->set_guided_step_text(
                    "Step 3/6 — Data Arrived: The data packet reached the Destination! The receiver checked its verification code (no damage) and is ready to send a receipt (ACK) back.");
            }
        } else if (xfer_watch_stage_ == 2) {
            if (stats.acknowledged_count > 0 || transfer_engine_.is_transfer_completed() || transfer_engine_.is_transfer_failed()) {
                xfer_watch_stage_ = 0;
                handle_pause_simulation();
                transfer_panel_->set_guided_step_text(
                    "Step 4/6 — Delivery Confirmed: The Sender got the receipt (ACK) back! Piece #0 is now marked ACKNOWLEDGED (safely delivered).");
            }
        } else if (xfer_watch_stage_ == 3) {
            if (transfer_engine_.is_transfer_completed() || transfer_engine_.is_transfer_failed()) {
                xfer_watch_stage_ = 0;
                handle_pause_simulation();
                if (transfer_engine_.is_transfer_completed()) {
                    transfer_panel_->set_guided_step_text(
                        "Step 5/6 — All Pieces Sent: Every data piece has been delivered and confirmed! Click 'Next >' for the final summary.");
                } else {
                    transfer_panel_->set_guided_step_text(
                        "Step 5/6 — Stopped: Transfer stopped because the destination could not be reached.");
                }
            }
        }
    }

    if (demo_mode_ == 1 && demo_watch_stage_ != 0) {
        auto stats = transfer_engine_.get_statistics();
        if (demo_watch_stage_ == 1) {
            if (has_ack_in_flight() || stats.acknowledged_count > 0 || transfer_engine_.is_transfer_completed()) {
                demo_watch_stage_ = 0;
                handle_pause_simulation();
                demo_panel_->set_live_step_text(
                    "Step 5/7 (Forward DATA): Chunk #0 DATA traveled Client1 -> Router3 -> ... -> Client2! Receiver emitted ACK.");
            }
        } else if (demo_watch_stage_ == 2) {
            if (stats.acknowledged_count > 0 || transfer_engine_.is_transfer_completed()) {
                demo_watch_stage_ = 0;
                handle_pause_simulation();
                demo_panel_->set_live_step_text(
                    "Step 6/7 (Reverse ACK): ACK returned Client2 -> ... -> Router3 -> Client1! Chunk #0 ACKNOWLEDGED.");
            }
        }
    }

    if (demo_mode_ == 2 && demo_watch_stage_ != 0) {
        if (demo_watch_stage_ == 10) {
            if (simulation_engine_.get_current_tick() >= 16 || simulation_engine_.get_in_flight_packets().empty()) {
                demo_watch_stage_ = 0;
                handle_pause_simulation();
                demo_panel_->set_live_step_text(
                    "Step 1/4: DATA packet reached the last-hop router! Client2 connects via these 2 highlighted last-hop links.");
            }
        } else if (demo_watch_stage_ == 11) {
            if (simulation_engine_.get_current_tick() >= 35 || simulation_engine_.get_in_flight_packets().empty()) {
                demo_watch_stage_ = 0;
                handle_pause_simulation();
                demo_panel_->set_live_step_text(
                    "Step 2/4: Both last-hop links are severed! DATA packet cannot reach Client2 and is dropped.");
            }
        } else if (demo_watch_stage_ == 12) {
            if (transfer_engine_.is_transfer_failed() || simulation_engine_.get_current_tick() >= 150) {
                demo_watch_stage_ = 0;
                handle_pause_simulation();
                demo_panel_->set_live_step_text(
                    "Step 3/4: Unacknowledged DATA timed out and triggered automatic retransmission retries!");
            }
        }
    }

    if (demo_watch_stage_ == 20) {
        if (simulation_engine_.get_in_flight_packets().empty()) {
            demo_watch_stage_ = 0;
            handle_pause_simulation();
            demo_panel_->set_live_step_text(
                QString("Route Probe Arrived! Discovered path: %1").arg(QString::fromStdString(discovered_probe_path_)));
        }
    }
}

void MainWindow::handle_generate_random_topology(int node_count, int client_count, double density, int seed) {
    handle_reset_simulation();

    graph_ = model::NetworkGraph();
    canvas_->set_network_graph(&graph_);

    client_count = std::max(1, std::min(client_count, node_count - 1));
    int router_count = node_count - client_count;

    std::mt19937 rng(seed);
    int id_counter = 1;
    dsa::DynamicArray<int> client_ids;
    for (int i = 0; i < client_count; ++i) {
        graph_.add_client(id_counter, "Client" + std::to_string(id_counter));
        client_ids.push_back(id_counter++);
    }

    dsa::DynamicArray<int> router_ids;
    for (int i = 0; i < router_count; ++i) {
        graph_.add_router(id_counter, "Router" + std::to_string(id_counter));
        router_ids.push_back(id_counter++);
    }

    for (int i = 0; i < client_ids.size(); ++i) {
        int r1 = i % router_ids.size();
        graph_.add_link(client_ids[i], router_ids[r1], 100, 5, 0.0, 0.0, 10);
        if (router_ids.size() > 1) {
            int r2 = (r1 + 1) % router_ids.size();
            graph_.add_link(client_ids[i], router_ids[r2], 80, 15, 0.0, 0.0, 10);
        }
    }

    for (int i = 0; i < router_ids.size(); ++i) {
        for (int j = i + 1; j < router_ids.size(); ++j) {
            double prob = static_cast<double>(rng() % 1000) / 1000.0;
            if (prob <= density || j == (i + 1)) {
                int bw = 100 + (rng() % 5) * 50;
                int lat = 5 + (rng() % 4) * 5;
                graph_.add_link(router_ids[i], router_ids[j], bw, lat, 0.0, 0.0, 10);
            }
        }
    }

    canvas_->clear_probe_visuals();
    canvas_->reset_layout();
    if (client_ids.size() >= 2) {
        active_source_id_ = client_ids[0];
        active_destination_id_ = client_ids[1];
    }
    refresh_client_combos();
    canvas_->update();
    update_route_display();
    update_queue_table();
    append_log_entry(0, "Topology", "Generated topology with seed " + std::to_string(seed),
                     std::to_string(node_count) + " nodes, density=" + std::to_string(density));
}

std::string MainWindow::handle_validate_topology() {
    model::NetworkGraph::ValidationResult report = graph_.validate();
    std::string summary = report.is_valid
        ? "Topology is VALID & fully connected.\n\n"
          "Checks passed:\n"
          "- Every Client has at least one active path to every other Client.\n"
          "- No completely isolated nodes or broken core paths detected."
        : "Topology WARNINGS detected:\n";
    for (int i = 0; i < report.warnings.size(); ++i) {
        summary += "- " + report.warnings[i] + "\n";
    }
    if (!report.is_valid) {
        summary += "\nSuggestion: Add a bridging link or recover failed Nodes/Links in Inspector • Faults.";
    }
    topology_panel_->set_validation_status(report.is_valid, summary);
    return summary;
}

void MainWindow::initialize_transfer_session(int src_id, int dst_id, const std::string& payload, int chunk_size, bool start_playback) {
    active_source_id_ = src_id;
    active_destination_id_ = dst_id;
    transfer_payload_ = payload;
    next_chunk_to_send_idx_ = 0;

    transfer_engine_.init_sender(payload, chunk_size);
    transfer_engine_.init_receiver(transfer_engine_.get_sender_chunk_count());
    transfer_chunks_ = transfer_engine_.get_all_sender_chunks();

    engine::RoutingEngine::Route r = routing_engine_.find_route(graph_, src_id, dst_id);
    canvas_->set_active_route(r.path);
    update_route_display();

    transfer_panel_->set_transfer_status_in_progress();
    transfer_panel_->populate_chunks(transfer_chunks_);

    append_log_entry(simulation_engine_.get_current_tick(), "Transfer",
                     "Initialized transfer from Client " + std::to_string(src_id) + " to Client " + std::to_string(dst_id),
                     std::to_string(transfer_chunks_.size()) + " chunks");

    if (start_playback && !sim_timer_->isActive()) {
        handle_start_simulation();
    }
}

void MainWindow::handle_start_file_transfer(int src_id, int dst_id, const std::string& payload, int chunk_size) {
    initialize_transfer_session(src_id, dst_id, payload, chunk_size, true);
}

void MainWindow::execute_transfer_step() {
    if (active_source_id_ == -1 || active_destination_id_ == -1) return;
    if (transfer_engine_.is_transfer_completed() || transfer_engine_.is_transfer_failed()) return;

    int current_tick = simulation_engine_.get_current_tick();

    if (next_chunk_to_send_idx_ < transfer_chunks_.size()) {
        engine::RoutingEngine::Route r = routing_engine_.find_route(graph_, active_source_id_, active_destination_id_);
        if (r.found && r.path.size() >= 2) {
            model::Packet pkt = transfer_engine_.send_chunk(next_chunk_to_send_idx_, active_source_id_, active_destination_id_, current_tick);
            simulation_engine_.send_packet_along_route(r.path, pkt);
            append_log_entry(current_tick, "Sender",
                             "Dispatched Chunk #" + std::to_string(next_chunk_to_send_idx_),
                             "Hops: " + std::to_string(r.hop_count()));
            next_chunk_to_send_idx_++;
        }
    }

    dsa::DynamicArray<int> timed_out = transfer_engine_.check_timeouts(current_tick);
    for (int i = 0; i < timed_out.size(); ++i) {
        int seq = timed_out[i];
        engine::RoutingEngine::Route r = routing_engine_.find_route(graph_, active_source_id_, active_destination_id_);
        model::Packet pkt = transfer_engine_.send_chunk(seq, active_source_id_, active_destination_id_, current_tick);
        if (r.found && r.path.size() >= 2) {
            simulation_engine_.send_packet_along_route(r.path, pkt);
            append_log_entry(current_tick, "Retransmit",
                             "Timeout! Retrying Chunk #" + std::to_string(seq) + " over route",
                             "Attempt #" + std::to_string(transfer_engine_.get_chunk_progress(seq).attempts));
        } else {
            append_log_entry(current_tick, "Retransmit",
                             "Timeout for Chunk #" + std::to_string(seq) + " (No route available)",
                             "Attempt #" + std::to_string(transfer_engine_.get_chunk_progress(seq).attempts));
        }
    }

    const dsa::DynamicArray<model::Packet>& delivered = simulation_engine_.get_delivered_packets();
    while (last_processed_delivered_idx_ < delivered.size()) {
        const model::Packet& pkt = delivered[last_processed_delivered_idx_++];

        if (pkt.get_type() == model::Packet::Type::DATA && pkt.get_destination_id() == active_destination_id_) {
            bool valid = transfer_engine_.receive_chunk_packet(pkt);
            if (valid) {
                append_log_entry(current_tick, "Receiver",
                                 "Verified Chunk #" + std::to_string(pkt.get_sequence_number()) + " checksum. Emitting ACK.",
                                 "Seq: " + std::to_string(pkt.get_sequence_number()));
                engine::RoutingEngine::Route ack_r = routing_engine_.find_route(graph_, active_destination_id_, active_source_id_);
                if (ack_r.found && ack_r.path.size() >= 2) {
                    model::Packet ack_pkt = transfer_engine_.create_ack_packet(active_destination_id_, active_source_id_, pkt.get_sequence_number());
                    simulation_engine_.send_packet_along_route(ack_r.path, ack_pkt);
                }
            } else {
                append_log_entry(current_tick, "Receiver",
                                 "Checksum mismatch on Chunk #" + std::to_string(pkt.get_sequence_number()) + "! Dropping without ACK.",
                                 "Corruption detected");
            }
        } else if (pkt.get_type() == model::Packet::Type::ACK && pkt.get_destination_id() == active_source_id_) {
            bool acked = transfer_engine_.process_ack_packet(pkt);
            if (acked) {
                append_log_entry(current_tick, "Sender",
                                 "Received ACK for Chunk #" + std::to_string(pkt.get_sequence_number()),
                                 "Seq: " + std::to_string(pkt.get_sequence_number()));
            }
            if (acked && transfer_engine_.is_transfer_completed() && transfer_engine_.is_reconstruction_ready()) {
                std::string reconstructed = transfer_engine_.reconstruct_file();
                append_log_entry(current_tick, "Transfer",
                                 "Transfer COMPLETED successfully! Reconstructed " + std::to_string(reconstructed.size()) + " bytes.",
                                 "Integrity 100%");
            }
        } else if (pkt.get_type() == model::Packet::Type::ROUTE_PROBE && pkt.get_destination_id() == active_destination_id_) {
            discovered_probe_path_ = pkt.get_payload();
            append_log_entry(current_tick, "Probe",
                             "ROUTE_PROBE delivered to destination! Discovered Path: " + discovered_probe_path_,
                             "Hops: " + std::to_string(pkt.get_sequence_number()));
        }
    }
}

void MainWindow::on_node_selected(int node_id) {
    model::Node* n = graph_.get_node(node_id);
    if (n == nullptr) return;

    QString status_badge = n->is_active() ? "<span style='color:#34d399; font-weight:bold;'>● ACTIVE</span>"
                                          : "<span style='color:#f87171; font-weight:bold;'>● FAILED</span>";
    QString info = QString("<div style='line-height: 1.6;'>"
                           "<b style='color:#38bdf8; font-size:14px;'>Selected Node: ID %1</b><br>"
                           "<b>Name:</b> %2<br>"
                           "<b>Type:</b> %3<br>"
                           "<b>Status:</b> %4</div>")
                       .arg(node_id)
                       .arg(QString::fromStdString(n->get_name()))
                       .arg(QString::fromStdString(n->get_type()))
                       .arg(status_badge);
    fault_panel_->set_selected_info_html(info);
    fault_panel_->set_link_params_editor(false, 10, 100);
}

void MainWindow::on_link_selected(int node_a, int node_b) {
    const model::Link* l = graph_.get_link(node_a, node_b);
    if (l == nullptr) return;

    QString status_badge = l->is_failed() ? "<span style='color:#f87171; font-weight:bold;'>● FAILED</span>"
                                          : "<span style='color:#34d399; font-weight:bold;'>● ACTIVE</span>";
    QString info = QString("<div style='line-height: 1.6;'>"
                           "<b style='color:#38bdf8; font-size:14px;'>Selected Link: %1 ⮂ %2</b><br>"
                           "<b>Bandwidth:</b> %3 KB/s<br>"
                           "<b>Latency:</b> %4 ms<br>"
                           "<b>Queue:</b> %5 / %6<br>"
                           "<b>Status:</b> %7</div>")
                       .arg(node_a).arg(node_b)
                       .arg(l->get_bandwidth())
                       .arg(l->get_latency())
                       .arg(l->queue_size()).arg(l->queue_capacity())
                       .arg(status_badge);
    fault_panel_->set_selected_info_html(info);
    fault_panel_->set_link_params_editor(true, l->get_latency(), l->get_bandwidth());
}

void MainWindow::on_canvas_clicked() {
    fault_panel_->reset_selection_prompt();
}

void MainWindow::handle_toggle_failure_selected() {
    int sel_node = canvas_->get_selected_node();
    std::pair<int, int> sel_link = canvas_->get_selected_link();

    if (sel_node != -1) {
        model::Node* n = graph_.get_node(sel_node);
        if (n) {
            if (n->is_active()) {
                simulation_engine_.fail_node_manually(sel_node);
                append_log_entry(simulation_engine_.get_current_tick(), "Fault",
                                 "Node " + std::to_string(sel_node) + " failed manually", "Dijkstra rerouting triggered");
            } else {
                graph_.recover_node(sel_node);
                append_log_entry(simulation_engine_.get_current_tick(), "Fault",
                                 "Node " + std::to_string(sel_node) + " recovered", "Topology restored");
            }
            on_node_selected(sel_node);
        }
    } else if (sel_link.first != -1) {
        const model::Link* l = graph_.get_link(sel_link.first, sel_link.second);
        if (l) {
            if (!l->is_failed()) {
                simulation_engine_.fail_link_manually(sel_link.first, sel_link.second);
                append_log_entry(simulation_engine_.get_current_tick(), "Fault",
                                 "Link (" + std::to_string(sel_link.first) + " - " + std::to_string(sel_link.second) + ") severed", "Dijkstra rerouting");
            } else {
                graph_.recover_link(sel_link.first, sel_link.second);
                append_log_entry(simulation_engine_.get_current_tick(), "Fault",
                                 "Link (" + std::to_string(sel_link.first) + " - " + std::to_string(sel_link.second) + ") recovered", "Link active");
            }
            on_link_selected(sel_link.first, sel_link.second);
        }
    }
    canvas_->update();
    update_route_display();
    update_queue_table();
}

void MainWindow::handle_inject_loss_selected() {
    std::pair<int, int> sel_link = canvas_->get_selected_link();
    if (sel_link.first != -1) {
        simulation_engine_.inject_link_loss(sel_link.first, sel_link.second);
        append_log_entry(simulation_engine_.get_current_tick(), "Fault",
                         "Arming manual packet drop on Link (" + std::to_string(sel_link.first) + " - " + std::to_string(sel_link.second) + ")",
                         "Next packet will drop");
        fault_panel_->set_selected_info_html(
            QString("<b>Armed Force Drop</b> on Link <b>%1 ⮂ %2</b>.<br>The next packet traversing this link will be dropped to trigger timeout &amp; retry.")
                .arg(sel_link.first).arg(sel_link.second));
    } else {
        QMessageBox::information(this, "No Link Selected",
                                 "Force Drop applies to a Link.\nClick any connecting Link on the canvas first, then try again.");
    }
}

void MainWindow::handle_inject_corruption_selected() {
    std::pair<int, int> sel_link = canvas_->get_selected_link();
    if (sel_link.first != -1) {
        simulation_engine_.inject_link_corruption(sel_link.first, sel_link.second);
        append_log_entry(simulation_engine_.get_current_tick(), "Fault",
                         "Arming manual packet corruption on Link (" + std::to_string(sel_link.first) + " - " + std::to_string(sel_link.second) + ")",
                         "Next packet will corrupt");
        fault_panel_->set_selected_info_html(
            QString("<b>Armed Force Corrupt</b> on Link <b>%1 ⮂ %2</b>.<br>The next packet traversing this link will be corrupted to test XOR checksum validation.")
                .arg(sel_link.first).arg(sel_link.second));
    } else {
        QMessageBox::information(this, "No Link Selected",
                                 "Force Corrupt applies to a Link.\nClick any connecting Link on the canvas first, then try again.");
    }
}

void MainWindow::refresh_client_combos() {
    const dsa::DynamicArray<int>& clients = graph_.get_client_ids();
    std::vector<std::pair<int, std::string>> entries;
    entries.reserve(clients.size());
    for (int i = 0; i < clients.size(); ++i) {
        int id = clients[i];
        model::Node* n = graph_.get_node(id);
        std::string name = n ? n->get_name() : std::to_string(id);
        entries.emplace_back(id, name);
    }
    transfer_panel_->set_clients(entries, active_source_id_, active_destination_id_);
}

void MainWindow::update_route_display() {
    if (active_source_id_ == -1 || active_destination_id_ == -1) {
        canvas_->set_active_route(dsa::DynamicArray<int>());
        routing_panel_->update_route_display(graph_, routing_engine_, -1, -1, engine::RoutingEngine::Route());
        return;
    }
    engine::RoutingEngine::Route r = routing_engine_.find_route(graph_, active_source_id_, active_destination_id_);
    canvas_->set_active_route(r.path);
    routing_panel_->update_route_display(graph_, routing_engine_, active_source_id_, active_destination_id_, r);
}

void MainWindow::update_queue_table() {
    routing_panel_->update_queue_table(graph_, routing_engine_);
}

void MainWindow::update_transfer_display() {
    if (active_source_id_ == -1) return;
    transfer_panel_->update_progress(transfer_engine_, transfer_chunks_);
}

void MainWindow::update_statistics_display() {
    engine::SimulationEngine::Statistics sim_stats = simulation_engine_.get_statistics();
    engine::TransferEngine::TransferStatistics tr_stats = transfer_engine_.get_statistics();

    QString summary = QString("<span style='color:#94a3b8;'>Sent:</span> <b style='color:#38bdf8;'>%1</b>  |  "
                              "<span style='color:#94a3b8;'>Delivered:</span> <b style='color:#34d399;'>%2</b>  |  "
                              "<span style='color:#94a3b8;'>Dropped:</span> <b style='color:#f87171;'>%3</b>  |  "
                              "<span style='color:#94a3b8;'>Lost:</span> <b style='color:#fbbf24;'>%4</b>  |  "
                              "<span style='color:#94a3b8;'>Corrupted:</span> <b style='color:#fb7185;'>%5</b>  |  "
                              "<span style='color:#94a3b8;'>Heartbeats:</span> <b style='color:#fb923c;'>%6</b>  |  "
                              "<span style='color:#94a3b8;'>Acks:</span> <b style='color:#2dd4bf;'>%7</b>  |  "
                              "<span style='color:#94a3b8;'>Retries:</span> <b style='color:#fde047;'>%8</b>")
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

void MainWindow::append_log_entry(int tick, const std::string& source, const std::string& desc, const std::string& details) {
    int row = table_event_log_->rowCount();
    table_event_log_->insertRow(row);

    QTableWidgetItem* item_tick = new QTableWidgetItem(QString::number(tick) + " ms");
    item_tick->setForeground(QColor(148, 163, 184));
    table_event_log_->setItem(row, 0, item_tick);

    QTableWidgetItem* item_src = new QTableWidgetItem(QString::fromStdString(source));
    QColor src_col = QColor(148, 163, 184);
    if (source == "Transfer") src_col = QColor(52, 211, 153);
    else if (source == "Fault" || source == "Retransmit") src_col = QColor(248, 113, 113);
    else if (source == "Probe") src_col = QColor(192, 132, 252);
    else if (source == "Simulation") src_col = QColor(96, 165, 250);
    else if (source == "Receiver") src_col = QColor(45, 212, 191);
    else if (source == "Sender") src_col = QColor(56, 189, 248);
    else if (source == "Demo 1" || source == "Demo 2") src_col = QColor(251, 191, 36);
    item_src->setForeground(src_col);
    QFont f = item_src->font();
    f.setBold(true);
    item_src->setFont(f);
    table_event_log_->setItem(row, 1, item_src);

    QTableWidgetItem* item_desc = new QTableWidgetItem(QString::fromStdString(desc));
    item_desc->setForeground(QColor(241, 245, 249));
    table_event_log_->setItem(row, 2, item_desc);

    QTableWidgetItem* item_details = new QTableWidgetItem(QString::fromStdString(details));
    item_details->setForeground(QColor(148, 163, 184));
    table_event_log_->setItem(row, 3, item_details);

    table_event_log_->scrollToBottom();
}

void MainWindow::handle_save_config(const std::string& file_path) {
    try {
        engine::FileStorageEngine::SimulationConfig cfg =
            storage_engine_.extract_config_from_graph(
                graph_, topology_panel_->seed(), transfer_panel_->chunk_size(), active_source_id_, active_destination_id_);
        storage_engine_.save_config(file_path, cfg);
        append_log_entry(simulation_engine_.get_current_tick(), "Storage", "Saved configuration to " + file_path, "");
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Save Error", QString::fromStdString(e.what()));
    }
}

void MainWindow::handle_load_config(const std::string& file_path) {
    try {
        engine::FileStorageEngine::SimulationConfig cfg = storage_engine_.load_config(file_path);
        handle_reset_simulation();
        graph_ = model::NetworkGraph();
        storage_engine_.apply_config_to_graph(cfg, graph_);
        topology_modified_manually_ = true;
        canvas_->update_layout();
        refresh_client_combos();
        canvas_->update();
        update_route_display();
        update_queue_table();
        append_log_entry(simulation_engine_.get_current_tick(), "Storage", "Loaded configuration from " + file_path, "");
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Load Error", QString::fromStdString(e.what()));
    }
}

void MainWindow::handle_export_experiment_results(const std::string& file_path) {
    try {
        engine::FileStorageEngine::ExperimentResult result;
        result.experiment_id = "EXP-" + std::to_string(QDateTime::currentMSecsSinceEpoch());
        result.outcome = transfer_engine_.is_transfer_completed() ? "SUCCESS" : "FAILURE";
        result.total_ticks = simulation_engine_.get_current_tick();
        result.config = storage_engine_.extract_config_from_graph(
            graph_, topology_panel_->seed(), transfer_panel_->chunk_size(), active_source_id_, active_destination_id_);
        result.simulation_stats = simulation_engine_.get_statistics();
        result.transfer_stats = transfer_engine_.get_statistics();

        for (int r = 0; r < table_event_log_->rowCount(); ++r) {
            QString entry = QString("[%1] %2: %3 (%4)")
                                .arg(table_event_log_->item(r, 0)->text())
                                .arg(table_event_log_->item(r, 1)->text())
                                .arg(table_event_log_->item(r, 2)->text())
                                .arg(table_event_log_->item(r, 3)->text());
            result.packet_events.push_back(entry.toStdString());
        }
        storage_engine_.save_experiment_result(file_path, result);
        append_log_entry(simulation_engine_.get_current_tick(), "Storage", "Exported experiment report to " + file_path, result.outcome);
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Export Error", QString::fromStdString(e.what()));
    }
}

void MainWindow::handle_send_route_probe() {
    int src_id = transfer_panel_->selected_src_id();
    int dst_id = transfer_panel_->selected_dst_id();
    if (src_id == -1 || dst_id == -1) {
        if (active_source_id_ == -1 || active_destination_id_ == -1) return;
        src_id = active_source_id_;
        dst_id = active_destination_id_;
    } else {
        active_source_id_ = src_id;
        active_destination_id_ = dst_id;
    }

    engine::RoutingEngine::Route r = routing_engine_.find_route(graph_, src_id, dst_id);
    if (!r.found || r.path.size() < 2) {
        append_log_entry(simulation_engine_.get_current_tick(), "Probe", "Route Probe failed: No valid path", "");
        return;
    }

    std::string route_trace;
    for (int i = 0; i < r.path.size(); ++i) {
        route_trace += std::to_string(r.path[i]);
        if (i < r.path.size() - 1) route_trace += "->";
    }

    model::Packet probe_pkt(model::Packet::Type::ROUTE_PROBE, src_id, dst_id, r.hop_count(), route_trace, 16);
    simulation_engine_.send_packet_along_route(r.path, probe_pkt);

    append_log_entry(simulation_engine_.get_current_tick(), "Probe",
                     "Dispatched Hop-by-Hop ROUTE_PROBE (" + std::to_string(src_id) + " -> " + std::to_string(dst_id) + ")",
                     "Expected Trace: " + route_trace);

    canvas_->set_active_route(r.path);
    update_route_display();
    handle_advance_ticks(100);
}

void MainWindow::handle_run_recovery_demonstration() {
    handle_generate_random_topology(6, 2, 0.45, 12345);
    handle_validate_topology();

    active_source_id_ = 1;
    active_destination_id_ = 2;

    append_log_entry(0, "Demo 1", "=== STARTING COHERENT DEMO 1: DYNAMIC FAILURE RECOVERY ===", "");
    handle_send_route_probe();

    std::string payload = "Demonstration Payload for PBL2 Simulator: Verifying seamless retransmission and Dijkstra dynamic rerouting upon network failure!";
    handle_start_file_transfer(1, 2, payload, 32);
    handle_advance_ticks(20);

    engine::RoutingEngine::Route r = routing_engine_.find_route(graph_, 1, 2);
    if (r.found && r.path.size() >= 3) {
        int failed_router = r.path[1];
        simulation_engine_.fail_node_manually(failed_router);
        append_log_entry(simulation_engine_.get_current_tick(), "Demo 1",
                         "INJECTED FAILURE: Disabled Router ID " + std::to_string(failed_router) + " mid-transfer!",
                         "Triggered immediate Dijkstra rerouting");
        update_route_display();
    }

    handle_advance_ticks(350);

    if (transfer_engine_.is_transfer_completed()) {
        append_log_entry(simulation_engine_.get_current_tick(), "Demo 1",
                         "DEMO 1 SUCCESS: File reconstructed with 100% integrity despite failure!",
                         "Saved to data/experiments/demonstration_recovery.json");
        handle_export_experiment_results("data/experiments/demonstration_recovery.json");
    }
}

void MainWindow::handle_run_retry_limit_demonstration() {
    handle_generate_random_topology(6, 2, 0.45, 12345);
    active_source_id_ = 1;
    active_destination_id_ = 2;

    append_log_entry(0, "Demo 2", "=== STARTING COHERENT DEMO 2: RETRY-LIMIT EXHAUSTION ===", "");
    std::string payload = "Controlled Retry Limit Failure Test Payload";
    handle_start_file_transfer(1, 2, payload, 64);
    handle_advance_ticks(5);

    const dsa::DynamicArray<int>& neighbors = graph_.get_neighbors(2);
    for (int i = 0; i < neighbors.size(); ++i) {
        simulation_engine_.fail_link_manually(2, neighbors[i]);
    }
    append_log_entry(simulation_engine_.get_current_tick(), "Demo 2",
                     "ISOLATED DESTINATION: Severed all links to Client 2", "Destination completely unreachable");
    update_route_display();

    handle_advance_ticks(350);

    if (transfer_engine_.is_transfer_failed()) {
        append_log_entry(simulation_engine_.get_current_tick(), "Demo 2",
                         "DEMO 2 SUCCESS: Transfer correctly aborted after exceeding maximum retry limit (3 attempts)!",
                         "Evidence preserved in statistics");
        handle_export_experiment_results("data/experiments/demonstration_failure.json");
    }
}

std::string MainWindow::get_discovered_probe_path() const {
    return discovered_probe_path_;
}

void MainWindow::setup_initial_startup_topology() {
    handle_reset_simulation();

    graph_ = model::NetworkGraph();
    canvas_->set_network_graph(&graph_);

    graph_.add_client(1, "Client1");
    graph_.add_router(2, "Router2");
    graph_.add_router(3, "Router3");
    graph_.add_router(4, "Router4");
    graph_.add_router(5, "Router5");
    graph_.add_client(6, "Client6");

    graph_.add_link(1, 2, 120, 5, 0.0, 0.0, 10);
    graph_.add_link(1, 3, 80, 15, 0.0, 0.0, 10);
    graph_.add_link(2, 4, 150, 10, 0.0, 0.0, 10);
    graph_.add_link(2, 5, 100, 15, 0.0, 0.0, 10);
    graph_.add_link(3, 4, 100, 15, 0.0, 0.0, 10);
    graph_.add_link(3, 5, 200, 10, 0.0, 0.0, 10);
    graph_.add_link(2, 3, 150, 10, 0.0, 0.0, 10);
    graph_.add_link(4, 5, 150, 10, 0.0, 0.0, 10);
    graph_.add_link(4, 6, 120, 5, 0.0, 0.0, 10);
    graph_.add_link(5, 6, 80, 15, 0.0, 0.0, 10);

    canvas_->reset_layout();
    active_source_id_ = 1;
    active_destination_id_ = 6;
    refresh_client_combos();
    canvas_->update();
    update_route_display();
    update_queue_table();
    handle_validate_topology();
    topology_modified_manually_ = false;
    append_log_entry(0, "Topology", "Initialized clean startup topology (Client1 -> Routers 2..5 -> Client6)", "6 nodes, 10 links");
}

bool MainWindow::confirm_preset_demo_if_modified() {
    if (!topology_modified_manually_) return true;
    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        "Preset Scenario Confirmation",
        "Running this Preset Scenario will reset your current custom topology to the 6-node demonstration network.\nDo you want to continue?",
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    return reply == QMessageBox::Yes;
}

void MainWindow::start_animated_demo1() {
    if (demo_timer_) demo_timer_->stop();
    demo_panel_->set_auto_checked(false);
    demo_mode_ = 1;
    demo_step_ = 0;
    apply_demo_step(0);
}

void MainWindow::start_animated_demo2() {
    if (demo_timer_) demo_timer_->stop();
    demo_panel_->set_auto_checked(false);
    demo_mode_ = 2;
    demo_step_ = 0;
    apply_demo_step(0);
}

void MainWindow::on_demo_prev_step() {
    if (demo_mode_ == 0 || demo_step_ <= 0) return;
    if (demo_timer_) demo_timer_->stop();
    demo_panel_->set_auto_checked(false);
    apply_demo_step(demo_step_ - 1);
}

void MainWindow::on_demo_next_step() {
    if (demo_mode_ == 0) return;
    int max_step = (demo_mode_ == 1) ? 6 : 3;
    if (demo_step_ >= max_step) return;
    apply_demo_step(demo_step_ + 1);
}

void MainWindow::on_demo_toggle_auto() {
    if (!demo_timer_) return;
    if (demo_panel_->is_auto_checked() && demo_mode_ != 0) {
        int max_step = (demo_mode_ == 1) ? 6 : 3;
        if (demo_step_ < max_step) apply_demo_step(demo_step_ + 1);
        if (demo_panel_->is_auto_checked() && demo_step_ < max_step) {
            demo_timer_->start(3000);
        }
    } else {
        demo_timer_->stop();
        demo_panel_->update_nav_buttons(demo_mode_, demo_step_, (demo_mode_ == 1) ? 6 : 3, false);
    }
}

void MainWindow::on_demo_timer_tick() {
    if (demo_mode_ == 0) {
        if (demo_timer_) demo_timer_->stop();
        return;
    }
    int max_step = (demo_mode_ == 1) ? 6 : 3;
    if (demo_step_ < max_step) {
        apply_demo_step(demo_step_ + 1);
    } else {
        if (demo_timer_) demo_timer_->stop();
        demo_panel_->set_auto_checked(false);
        demo_panel_->update_nav_buttons(demo_mode_, demo_step_, max_step, false);
    }
}

void MainWindow::apply_demo_step(int step) {
    int saved_mode = demo_mode_;
    bool was_auto = demo_panel_->is_auto_checked();
    if (saved_mode == 1) {
        int target_step = std::max(0, std::min(step, 6));
        if (demo_timer_) demo_timer_->stop();
        handle_generate_random_topology(6, 2, 0.45, 12345);
        demo_mode_ = 1;
        demo_step_ = target_step;
        demo_panel_->set_auto_checked(was_auto);
        handle_validate_topology();
        active_source_id_ = 1;
        active_destination_id_ = 2;
        demo_failed_router_ = 4;
        simulation_engine_.fail_node_manually(demo_failed_router_);
        update_route_display();

        engine::RoutingEngine::Route r = routing_engine_.find_route(graph_, 1, 2);
        demo_alt_route_ = r.path;

        if (demo_step_ == 0) {
            QVector<TopologyCanvas::ProbeEdge> probes;
            probes.push_back({1, 4, 0, "Probing..."});
            probes.push_back({1, 3, 0, "Probing..."});
            canvas_->set_probe_visuals(probes);
            demo_panel_->set_live_step_text("Step 1/7: Client1 probes adjacent routers Router4 and Router3...\n(Click 'Next >' to inspect results)");
        } else if (demo_step_ == 1) {
            QVector<TopologyCanvas::ProbeEdge> probes;
            probes.push_back({1, 4, 1, "X OFFLINE"});
            probes.push_back({1, 3, 2, "OK 5ms • 100KB/s"});
            canvas_->set_probe_visuals(probes);
            demo_panel_->set_live_step_text("Step 2/7: Router4 is OFFLINE (marked with red X). Client1 fails over to Router3 successfully!");
        } else if (demo_step_ == 2) {
            QVector<TopologyCanvas::ProbeEdge> probes;
            probes.push_back({1, 3, 2, "Selected"});
            const dsa::DynamicArray<int>& nbrs = graph_.get_neighbors(3);
            for (int i = 0; i < nbrs.size(); ++i) {
                int nb = nbrs[i];
                if (nb == 1 || nb == 4) continue;
                probes.push_back({3, nb, 0, "Compare ms/KB/s"});
            }
            canvas_->set_probe_visuals(probes);
            demo_panel_->set_live_step_text("Step 3/7: At Router3, outgoing links are evaluated by latency (ms) and bandwidth (KB/s)...");
        } else if (demo_step_ == 3) {
            QVector<TopologyCanvas::ProbeEdge> probes;
            int chosen_next = (demo_alt_route_.size() >= 3) ? demo_alt_route_[2] : 5;
            probes.push_back({1, 3, 2, "OK"});
            const dsa::DynamicArray<int>& nbrs = graph_.get_neighbors(3);
            for (int i = 0; i < nbrs.size(); ++i) {
                int nb = nbrs[i];
                if (nb == 1 || nb == 4) continue;
                if (nb == chosen_next) {
                    probes.push_back({3, nb, 2, "Faster Path"});
                } else {
                    probes.push_back({3, nb, 3, "Slower (Dropped)"});
                }
            }
            for (int i = 2; i < demo_alt_route_.size() - 1; ++i) {
                probes.push_back({demo_alt_route_[i], demo_alt_route_[i + 1], 2, "Optimal"});
            }
            canvas_->set_probe_visuals(probes);
            demo_panel_->set_live_step_text("Step 4/7: Slower alternative link is discarded; lowest-cost path to Client2 is locked in!");
        } else if (demo_step_ == 4) {
            canvas_->clear_probe_visuals();
            std::string payload = "Demonstration Payload for PBL2 Simulator: Verifying seamless retransmission and Dijkstra dynamic rerouting upon network failure!";
            initialize_transfer_session(1, 2, payload, 32, false);
            handle_advance_ticks(1);
            demo_watch_stage_ = 1;
            handle_start_simulation();
            demo_panel_->set_live_step_text("Step 5/7 (Forward DATA In-Flight): Watching DATA packet travel Client1 -> Router3 -> ... -> Client2...");
        } else if (demo_step_ == 5) {
            canvas_->clear_probe_visuals();
            std::string payload = "Demonstration Payload for PBL2 Simulator: Verifying seamless retransmission and Dijkstra dynamic rerouting upon network failure!";
            initialize_transfer_session(1, 2, payload, 32, false);
            bool has_ack = false;
            for (int t = 0; t < 150; ++t) {
                handle_advance_ticks(1);
                const dsa::DynamicArray<engine::SimulationEngine::InFlightPacket>& pkts = simulation_engine_.get_in_flight_packets();
                for (int i = 0; i < pkts.size(); ++i) {
                    if (pkts[i].packet.get_type() == model::Packet::Type::ACK) {
                        has_ack = true;
                        break;
                    }
                }
                if (has_ack || transfer_engine_.get_statistics().acknowledged_count > 0) break;
            }
            if (has_ack && transfer_engine_.get_statistics().acknowledged_count == 0) {
                demo_watch_stage_ = 2;
                handle_start_simulation();
                demo_panel_->set_live_step_text("Step 6/7 (Reverse ACK In-Flight): Watching ACK return Client2 -> ... -> Router3 -> Client1...");
            } else {
                demo_panel_->set_live_step_text("Step 6/7 (Reverse ACK Returned): ACK returned Client2 -> ... -> Router3 -> Client1!");
            }
        } else {
            canvas_->clear_probe_visuals();
            std::string payload = "Demonstration Payload for PBL2 Simulator: Verifying seamless retransmission and Dijkstra dynamic rerouting upon network failure!";
            initialize_transfer_session(1, 2, payload, 32, false);
            handle_advance_ticks(370);
            if (transfer_engine_.is_transfer_completed()) {
                append_log_entry(simulation_engine_.get_current_tick(), "Demo 1",
                                 "DEMO 1 SUCCESS: File reconstructed with 100% integrity despite failure!",
                                 "Saved to data/experiments/demonstration_recovery.json");
                handle_export_experiment_results("data/experiments/demonstration_recovery.json");
            }
            demo_panel_->set_live_step_text("Step 7/7 (Completed): 100% file integrity reconstructed over alternate route despite Router4 failure!");
        }
        if (was_auto && demo_step_ < 6 && demo_timer_) demo_timer_->start(3000);
        demo_panel_->update_nav_buttons(demo_mode_, demo_step_, 6, was_auto && demo_step_ < 6);
    } else if (saved_mode == 2) {
        int target_step = std::max(0, std::min(step, 3));
        if (demo_timer_) demo_timer_->stop();
        handle_generate_random_topology(6, 2, 0.45, 12345);
        demo_mode_ = 2;
        demo_step_ = target_step;
        demo_panel_->set_auto_checked(was_auto);
        active_source_id_ = 1;
        active_destination_id_ = 2;
        std::string payload = "Controlled Retry Limit Failure Test Payload";
        initialize_transfer_session(1, 2, payload, 64, false);

        const dsa::DynamicArray<int>& neighbors = graph_.get_neighbors(2);
        if (demo_step_ == 0) {
            handle_advance_ticks(1);
            QVector<TopologyCanvas::ProbeEdge> probes;
            for (int i = 0; i < neighbors.size(); ++i) {
                probes.push_back({2, neighbors[i], 0, "Last-Hop Link"});
            }
            canvas_->set_probe_visuals(probes);
            demo_watch_stage_ = 10;
            handle_start_simulation();
            demo_panel_->set_live_step_text("Step 1/4: Sending DATA toward Client2 across the network (watching live travel)...");
        } else if (demo_step_ == 1) {
            handle_advance_ticks(15);
            QVector<TopologyCanvas::ProbeEdge> probes;
            for (int i = 0; i < neighbors.size(); ++i) {
                simulation_engine_.fail_link_manually(2, neighbors[i]);
                probes.push_back({2, neighbors[i], 1, "X Link Severed"});
            }
            canvas_->set_probe_visuals(probes);
            update_route_display();
            demo_watch_stage_ = 11;
            handle_start_simulation();
            demo_panel_->set_live_step_text("Step 2/4: Both last-hop links to Client2 are severed while DATA is in flight -> Watching packet fail to cross...");
        } else if (demo_step_ == 2) {
            handle_advance_ticks(15);
            for (int i = 0; i < neighbors.size(); ++i) {
                simulation_engine_.fail_link_manually(2, neighbors[i]);
            }
            canvas_->clear_probe_visuals();
            update_route_display();
            demo_watch_stage_ = 12;
            handle_start_simulation();
            demo_panel_->set_live_step_text("Step 3/4: Watching unacknowledged packet time out and trigger automatic retries (1..3)...");
        } else {
            for (int i = 0; i < neighbors.size(); ++i) {
                simulation_engine_.fail_link_manually(2, neighbors[i]);
            }
            canvas_->clear_probe_visuals();
            update_route_display();
            handle_advance_ticks(360);
            if (transfer_engine_.is_transfer_failed()) {
                append_log_entry(simulation_engine_.get_current_tick(), "Demo 2",
                                 "DEMO 2 SUCCESS: Transfer correctly aborted after exceeding maximum retry limit (3 attempts)!",
                                 "Evidence preserved in statistics");
                handle_export_experiment_results("data/experiments/demonstration_failure.json");
            }
            demo_panel_->set_live_step_text("Step 4/4 (Completed): Max retry limit (3) reached -> Transfer aborted safely with failure evidence.");
        }
        if (was_auto && demo_step_ < 3 && demo_timer_) demo_timer_->start(3500);
        demo_panel_->update_nav_buttons(demo_mode_, demo_step_, 3, was_auto && demo_step_ < 3);
    }
}

void MainWindow::start_guided_transfer(int src_id, int dst_id, const std::string& payload, int chunk_size) {
    if (xfer_timer_) xfer_timer_->stop();
    transfer_panel_->set_auto_checked(false);
    xfer_src_id_ = src_id;
    xfer_dst_id_ = dst_id;
    xfer_payload_ = payload;
    xfer_chunk_size_ = chunk_size;
    xfer_step_active_ = true;
    xfer_step_ = 0;
    apply_xfer_step(0);
}

void MainWindow::on_xfer_prev_step() {
    if (!xfer_step_active_ || xfer_step_ <= 0) return;
    if (xfer_timer_) xfer_timer_->stop();
    transfer_panel_->set_auto_checked(false);
    apply_xfer_step(xfer_step_ - 1);
}

void MainWindow::on_xfer_next_step() {
    if (!xfer_step_active_ || xfer_step_ >= 5) return;
    apply_xfer_step(xfer_step_ + 1);
}

void MainWindow::on_xfer_toggle_auto() {
    if (!xfer_timer_) return;
    if (transfer_panel_->is_auto_checked() && xfer_step_active_) {
        if (xfer_step_ < 5) apply_xfer_step(xfer_step_ + 1);
        if (transfer_panel_->is_auto_checked() && xfer_step_ < 5) {
            xfer_timer_->start(3000);
        }
    } else {
        xfer_timer_->stop();
        transfer_panel_->update_nav_buttons(xfer_step_active_, xfer_step_, 5, false);
    }
}

void MainWindow::on_xfer_timer_tick() {
    if (!xfer_step_active_) {
        if (xfer_timer_) xfer_timer_->stop();
        return;
    }
    if (xfer_step_ < 5) {
        apply_xfer_step(xfer_step_ + 1);
    } else {
        if (xfer_timer_) xfer_timer_->stop();
        transfer_panel_->set_auto_checked(false);
        transfer_panel_->update_nav_buttons(xfer_step_active_, xfer_step_, 5, false);
    }
}

void MainWindow::apply_xfer_step(int step) {
    int target_step = std::max(0, std::min(step, 5));
    bool was_auto = transfer_panel_->is_auto_checked();
    if (xfer_timer_) xfer_timer_->stop();

    handle_reset_simulation();
    xfer_step_active_ = true;
    xfer_step_ = target_step;
    transfer_panel_->set_auto_checked(was_auto);

    // IMPORTANT: Do NOT auto-start sim_timer_ during static guided steps (0, 1)
    initialize_transfer_session(xfer_src_id_, xfer_dst_id_, xfer_payload_, xfer_chunk_size_, false);
    engine::RoutingEngine::Route r = routing_engine_.find_route(graph_, xfer_src_id_, xfer_dst_id_);

    if (xfer_step_ == 0) {
        update_transfer_display();
        transfer_panel_->set_guided_step_text(
            QString("Step 1/6 — Prepare Data: Your message is split into %1 small piece(s) (chunks), like numbered pages of a letter, so it travels safely. Each piece starts as UNSENT with a verification code (Checksum) to catch errors.")
                .arg(transfer_chunks_.size()));
    } else if (xfer_step_ == 1) {
        QVector<TopologyCanvas::ProbeEdge> probes;
        QString route_names;
        for (int i = 0; i < r.path.size(); ++i) {
            model::Node* n = graph_.get_node(r.path[i]);
            route_names += n ? QString::fromStdString(n->get_name()) : QString::number(r.path[i]);
            if (i < r.path.size() - 1) {
                route_names += " -> ";
                probes.push_back({r.path[i], r.path[i + 1], 2, "Optimal Route"});
            }
        }
        canvas_->set_probe_visuals(probes);
        update_transfer_display();
        if (r.found) {
            transfer_panel_->set_guided_step_text(
                QString("Step 2/6 — Find Fastest Path: The network compares all roads and picks the quickest route: %1 (path score = %2, highlighted in green on the map).")
                    .arg(route_names).arg(r.total_cost, 0, 'f', 2));
        } else {
            transfer_panel_->set_guided_step_text("Step 2/6 — No Path Found: There is no open road connecting the Sender to the Destination.");
        }
    } else if (xfer_step_ == 2) {
        canvas_->clear_probe_visuals();
        handle_advance_ticks(1);
        xfer_watch_stage_ = 1;
        handle_start_simulation();
        transfer_panel_->set_guided_step_text(
            "Step 3/6 — Sending Data: Watch the glowing data packet travel from the Sender, through the Routers, to the Destination (State = IN_FLIGHT)...");
    } else if (xfer_step_ == 3) {
        canvas_->clear_probe_visuals();
        bool has_ack = false;
        for (int t = 0; t < 150; ++t) {
            handle_advance_ticks(1);
            const dsa::DynamicArray<engine::SimulationEngine::InFlightPacket>& pkts = simulation_engine_.get_in_flight_packets();
            for (int i = 0; i < pkts.size(); ++i) {
                if (pkts[i].packet.get_type() == model::Packet::Type::ACK) {
                    has_ack = true;
                    break;
                }
            }
            if (has_ack || transfer_engine_.get_statistics().acknowledged_count > 0 || transfer_engine_.is_transfer_failed()) {
                break;
            }
        }
        if (has_ack && transfer_engine_.get_statistics().acknowledged_count == 0) {
            xfer_watch_stage_ = 2;
            handle_start_simulation();
            transfer_panel_->set_guided_step_text(
                "Step 4/6 — Confirming Delivery: Watch the green receipt packet (ACK) travel back from the Destination to the Sender...");
        } else {
            transfer_panel_->set_guided_step_text(
                "Step 4/6 — Delivery Confirmed: The Sender got the receipt (ACK) back! Piece #0 is now marked ACKNOWLEDGED (safely delivered).");
        }
    } else if (xfer_step_ == 4) {
        canvas_->clear_probe_visuals();
        for (int t = 0; t < 150; ++t) {
            handle_advance_ticks(1);
            if (transfer_engine_.get_statistics().acknowledged_count > 0 ||
                transfer_engine_.is_transfer_completed() ||
                transfer_engine_.is_transfer_failed()) {
                break;
            }
        }
        if (!transfer_engine_.is_transfer_completed() && !transfer_engine_.is_transfer_failed()) {
            xfer_watch_stage_ = 3;
            handle_start_simulation();
            transfer_panel_->set_guided_step_text(
                "Step 5/6 — Sending Remaining Pieces: Sending and confirming any remaining data pieces across the network...");
        } else {
            transfer_panel_->set_guided_step_text(
                "Step 5/6 — All Pieces Sent: Every data piece has been delivered and confirmed! Click 'Next >' for the final summary.");
        }
    } else {
        canvas_->clear_probe_visuals();
        for (int t = 0; t < 400; ++t) {
            if (transfer_engine_.is_transfer_completed() || transfer_engine_.is_transfer_failed()) break;
            handle_advance_ticks(2);
        }
        if (transfer_engine_.is_transfer_completed()) {
            transfer_panel_->set_guided_step_text(
                "Step 6/6 — Complete (100%): All pieces are ACKNOWLEDGED and joined back together into the exact original message with zero errors!");
        } else {
            transfer_panel_->set_guided_step_text(
                "Step 6/6 — Stopped: The destination could not be reached after 3 retry attempts, so the transfer stopped safely.");
        }
    }

    if (was_auto && xfer_step_ < 5 && xfer_timer_) {
        xfer_timer_->start(3000);
    }
    transfer_panel_->update_nav_buttons(xfer_step_active_, xfer_step_, 5, was_auto && xfer_step_ < 5);
}

void MainWindow::push_topology_undo() {
    TopologySnapshot snap;
    const dsa::DynamicArray<int>& node_ids = graph_.get_node_ids();
    for (int i = 0; i < node_ids.size(); ++i) {
        const model::Node* n = graph_.get_node(node_ids[i]);
        if (!n) continue;
        snap.nodes.push_back({n->get_id(), n->get_name(), n->get_type(), n->is_active()});
    }
    const dsa::DynamicArray<model::Link*>& links = graph_.get_all_links();
    for (int i = 0; i < links.size(); ++i) {
        const model::Link* l = links[i];
        if (!l) continue;
        snap.links.push_back({
            l->get_first_node_id(),
            l->get_second_node_id(),
            l->get_bandwidth(),
            l->get_latency(),
            l->get_loss_probability(),
            l->get_corruption_probability(),
            l->queue_capacity(),
            l->is_failed()
        });
    }
    undo_stack_.push_back(std::move(snap));
    if (undo_stack_.size() > 25) {
        undo_stack_.erase(undo_stack_.begin());
    }
    topology_panel_->set_undo_enabled(true);
}

void MainWindow::handle_undo_topology() {
    if (undo_stack_.empty()) return;
    TopologySnapshot snap = std::move(undo_stack_.back());
    undo_stack_.pop_back();

    handle_reset_simulation();
    graph_ = model::NetworkGraph();
    canvas_->set_network_graph(&graph_);

    for (const SavedNode& sn : snap.nodes) {
        if (sn.type == "Client") {
            graph_.add_client(sn.id, sn.name);
        } else {
            graph_.add_router(sn.id, sn.name);
        }
        if (!sn.active) {
            simulation_engine_.fail_node_manually(sn.id);
        }
    }
    for (const SavedLink& sl : snap.links) {
        graph_.add_link(sl.u, sl.v, sl.bandwidth, sl.latency, sl.loss_prob, sl.corr_prob, sl.queue_cap);
        if (sl.failed) {
            simulation_engine_.fail_link_manually(sl.u, sl.v);
        }
    }

    canvas_->set_selected_node(-1);
    canvas_->set_selected_link(-1, -1);
    canvas_->update_layout();
    refresh_client_combos();
    canvas_->update();
    update_route_display();
    update_queue_table();
    fault_panel_->reset_selection_prompt();
    topology_panel_->set_undo_enabled(!undo_stack_.empty());

    append_log_entry(simulation_engine_.get_current_tick(), "Topology",
                     "Undid last topology change (restored nodes & links)",
                     "Remaining undo steps: " + std::to_string(undo_stack_.size()));
}

}  // namespace ui
}  // namespace pbl
