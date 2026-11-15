#include <bits/stdc++.h>

#include <QApplication>
#include <QBoxLayout>
#include <QSplitter>
#include <QGroupBox>
#include <QHeaderView>
#include <QMessageBox>
#include <QFileDialog>
#include <QDateTime>
#include <QScrollArea>

#include "ui/MainWindow.h"

using namespace std;

namespace pbl {
namespace ui {

// Milestone 11: Supporting/boilerplate — Qt dashboard and topology controls
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

    demo_mode_ = 0;
    demo_step_ = 0;
    demo_failed_router_ = -1;
    xfer_step_active_ = false;
    xfer_step_ = 0;
    xfer_src_id_ = -1;
    xfer_dst_id_ = -1;
    xfer_chunk_size_ = 1024;

    // Attach services
    simulation_service_.set_network_graph(&graph_);
    simulation_service_.set_routing_service(&routing_service_);

    setup_ui();
    apply_modern_theme();

    // Default simulation playback timer: fixed 50 ms tick rate
    sim_timer_ = new QTimer(this);
    connect(sim_timer_, &QTimer::timeout, this, &MainWindow::on_simulation_timer_tick);

    // Step-by-step animated demonstration timer
    demo_timer_ = new QTimer(this);
    connect(demo_timer_, &QTimer::timeout, this, &MainWindow::on_demo_timer_tick);

    // Step-by-step guided file transfer timer
    xfer_timer_ = new QTimer(this);
    connect(xfer_timer_, &QTimer::timeout, this, &MainWindow::on_xfer_timer_tick);

    // Initial startup topology: Client1 (ID 1) on left, Router2..5 in middle, Client6 (ID 6) on right
    setup_initial_startup_topology();

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

void MainWindow::apply_modern_theme() {
    QString qss = R"(
        QMainWindow {
            background-color: #0b0f19;
            color: #f1f5f9;
            font-family: 'Segoe UI', 'Inter', sans-serif;
        }
        QWidget {
            color: #f1f5f9;
            font-family: 'Segoe UI', 'Inter', sans-serif;
        }
        QGroupBox {
            background-color: #111827;
            border: 1px solid #1f293d;
            border-radius: 8px;
            margin-top: 16px;
            padding-top: 12px;
            font-weight: bold;
            font-size: 11px;
            color: #94a3b8;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            left: 12px;
            padding: 0 6px;
            background-color: #111827;
            color: #38bdf8;
            font-weight: bold;
        }
        QTabWidget::pane {
            border: 1px solid #1f293d;
            background-color: #111827;
            border-radius: 8px;
            top: -1px;
        }
        QTabBar {
            background-color: #0b0f19;
            qproperty-drawBase: 0;
        }
        QTabBar::tab {
            background-color: #1e293b;
            color: #94a3b8;
            border: 1px solid #1f293d;
            border-bottom: none;
            border-top-left-radius: 6px;
            border-top-right-radius: 6px;
            padding: 8px 16px;
            margin-right: 3px;
            font-weight: 600;
        }
        QTabBar::tab:hover {
            background-color: #334155;
            color: #f1f5f9;
        }
        QTabBar::tab:selected {
            background-color: #111827;
            color: #38bdf8;
            border-bottom: 2px solid #38bdf8;
            font-weight: bold;
        }
        QPushButton {
            background-color: #1e293b;
            color: #f1f5f9;
            border: 1px solid #334155;
            border-radius: 6px;
            padding: 6px 14px;
            font-weight: 600;
        }
        QPushButton:hover {
            background-color: #334155;
            border-color: #475569;
        }
        QPushButton:pressed {
            background-color: #0f172a;
        }
        QPushButton:disabled {
            background-color: #1e293b;
            color: #64748b;
            border-color: #1e293b;
        }
        QLineEdit, QTextEdit, QSpinBox, QDoubleSpinBox, QComboBox {
            background-color: #0b0f19;
            color: #f1f5f9;
            border: 1px solid #334155;
            border-radius: 6px;
            padding: 5px 8px;
            selection-background-color: #2563eb;
        }
        QLineEdit:focus, QTextEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus {
            border: 1px solid #38bdf8;
        }
        QComboBox::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 20px;
            border-left: 1px solid #334155;
        }
        QComboBox QAbstractItemView {
            background-color: #0b0f19;
            color: #f1f5f9;
            selection-background-color: #1e3a5f;
            selection-color: #38bdf8;
            border: 1px solid #334155;
        }
        QTableWidget {
            background-color: #0b0f19;
            alternate-background-color: #111827;
            color: #f1f5f9;
            gridline-color: #1f293d;
            border: 1px solid #1f293d;
            border-radius: 6px;
            selection-background-color: #1e3a5f;
            selection-color: #38bdf8;
        }
        QHeaderView::section {
            background-color: #172033;
            color: #94a3b8;
            font-weight: bold;
            font-size: 11px;
            border: none;
            border-bottom: 2px solid #2563eb;
            padding: 6px;
        }
        QTableCornerButton::section {
            background-color: #172033;
            border: none;
            border-bottom: 2px solid #2563eb;
        }
        QAbstractScrollArea::corner {
            background-color: #0b0f19;
            border: none;
        }
        QProgressBar {
            background-color: #0b0f19;
            border: 1px solid #334155;
            border-radius: 6px;
            text-align: center;
            color: #ffffff;
            font-weight: bold;
            height: 18px;
        }
        QProgressBar::chunk {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #10b981, stop:1 #06b6d4);
            border-radius: 5px;
        }
        QSplitter::handle {
            background-color: #1f293d;
        }
        QSplitter::handle:hover {
            background-color: #38bdf8;
        }
        QScrollBar:vertical {
            background-color: #0b0f19;
            width: 8px;
            margin: 0;
            border-radius: 4px;
        }
        QScrollBar::handle:vertical {
            background-color: #334155;
            min-height: 20px;
            border-radius: 4px;
        }
        QScrollBar::handle:vertical:hover {
            background-color: #475569;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical,
        QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {
            background: #0b0f19;
        }
        QScrollBar:horizontal {
            background-color: #0b0f19;
            height: 8px;
            margin: 0;
            border-radius: 4px;
        }
        QScrollBar::handle:horizontal {
            background-color: #334155;
            min-width: 20px;
            border-radius: 4px;
        }
        QScrollBar::handle:horizontal:hover {
            background-color: #475569;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            width: 0px;
        }
        QScrollArea, QWidget#qt_scrollarea_viewport {
            background-color: #111827;
            border: none;
        }
        QScrollArea > QWidget > QWidget {
            background-color: #111827;
        }
        QMessageBox {
            background-color: #111827;
            color: #f1f5f9;
        }
        QMessageBox QLabel {
            color: #f1f5f9;
            font-size: 12px;
        }
    )";
    setStyleSheet(qss);
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
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(8);

    btn_start_ = new QPushButton("▶ Start", box);
    btn_start_->setStyleSheet("background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #10b981, stop:1 #059669); color: white; font-weight: bold; border: none; border-radius: 6px; padding: 6px 14px;");
    connect(btn_start_, &QPushButton::clicked, this, [this]() { handle_start_simulation(); });

    btn_pause_ = new QPushButton("⏸ Pause", box);
    btn_pause_->setStyleSheet("background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #f59e0b, stop:1 #d97706); color: #0b0f19; font-weight: bold; border: none; border-radius: 6px; padding: 6px 14px;");
    connect(btn_pause_, &QPushButton::clicked, this, [this]() { handle_pause_simulation(); });

    btn_resume_ = new QPushButton("⏯ Resume", box);
    btn_resume_->setStyleSheet("background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #06b6d4, stop:1 #0891b2); color: white; font-weight: bold; border: none; border-radius: 6px; padding: 6px 14px;");
    connect(btn_resume_, &QPushButton::clicked, this, [this]() { handle_resume_simulation(); });

    btn_reset_ = new QPushButton("↺ Reset", box);
    btn_reset_->setStyleSheet("background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ef4444, stop:1 #dc2626); color: white; font-weight: bold; border: none; border-radius: 6px; padding: 6px 14px;");
    connect(btn_reset_, &QPushButton::clicked, this, [this]() { handle_reset_simulation(); });

    btn_step_1_ = new QPushButton("Step +1ms", box);
    connect(btn_step_1_, &QPushButton::clicked, this, [this]() { handle_single_tick(); });

    btn_step_10_ = new QPushButton("Step +10ms", box);
    connect(btn_step_10_, &QPushButton::clicked, this, [this]() { handle_advance_ticks(10); });

    lbl_sim_status_ = new QLabel("Status: STOPPED", box);
    lbl_sim_status_->setStyleSheet("background-color: rgba(100, 116, 139, 0.2); color: #94a3b8; border: 1px solid #64748b; border-radius: 12px; padding: 4px 12px; font-weight: bold; font-size: 11px;");

    lbl_sim_tick_ = new QLabel("Tick: 0 ms", box);
    lbl_sim_tick_->setStyleSheet("background-color: #1e293b; color: #38bdf8; border: 1px solid #334155; border-radius: 12px; padding: 4px 12px; font-family: 'Consolas', 'Courier New', monospace; font-weight: bold; font-size: 11px;");

    layout->addWidget(btn_start_);
    layout->addWidget(btn_pause_);
    layout->addWidget(btn_resume_);
    layout->addWidget(btn_reset_);
    layout->addWidget(btn_step_1_);
    layout->addWidget(btn_step_10_);
    layout->addSpacing(16);
    layout->addWidget(lbl_sim_status_);
    layout->addWidget(lbl_sim_tick_);
    layout->addStretch();

    QPushButton* btn_save = new QPushButton("💾 Save Config", box);
    connect(btn_save, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getSaveFileName(this, "Save Configuration", "config.json", "JSON (*.json)");
        if (!path.isEmpty()) handle_save_config(path.toStdString());
    });

    QPushButton* btn_load = new QPushButton("📂 Load Config", box);
    connect(btn_load, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getOpenFileName(this, "Load Configuration", "", "JSON (*.json)");
        if (!path.isEmpty()) handle_load_config(path.toStdString());
    });

    QPushButton* btn_export = new QPushButton("📊 Export Experiment", box);
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
    QScrollArea* topo_scroll = new QScrollArea(tabs);
    topo_scroll->setWidgetResizable(true);
    QWidget* tab_topo = new QWidget(topo_scroll);
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
    spin_density_->setToolTip("Xác suất thêm liên kết dự phòng giữa các node (0.10 = thưa, 1.00 = dày đặc, nhiều đường thay thế).");
    gen_grid->addWidget(spin_density_, 2, 1);

    gen_grid->addWidget(new QLabel("Random Seed:"), 3, 0);
    spin_seed_ = new QSpinBox(gen_box);
    spin_seed_->setRange(1, 9999999);
    spin_seed_->setValue(12345);
    spin_seed_->setToolTip("Mã số ngẫu nhiên cố định: nhập cùng một Seed sẽ sinh ra đúng cùng một sơ đồ mạng để tái hiện thí nghiệm.");
    gen_grid->addWidget(spin_seed_, 3, 1);

    QLabel* lbl_gen_hint = new QLabel(
        "<b>Giải thích nhanh:</b><br>"
        "• <b>Connection Density (0.10–1.00):</b> Độ dày liên kết. Giá trị cao tạo nhiều đường dự phòng hơn để Dijkstra chuyển hướng khi có node hỏng.<br>"
        "• <b>Random Seed:</b> Mã số cố định bộ sinh ngẫu nhiên. Cùng thông số + cùng Seed luôn ra 1 sơ đồ giống hệt nhau để đối chứng kết quả.<br>"
        "• <b>Validate Topology:</b> Kiểm tra đồ thị có bị cô lập node hay mất đường thông giữa các Client không.",
        gen_box);
    lbl_gen_hint->setWordWrap(true);
    lbl_gen_hint->setStyleSheet("padding: 8px; background-color: #0b0f19; border: 1px solid #1f293d; border-left: 3px solid #38bdf8; border-radius: 6px; color: #cbd5e1; font-size: 11px;");
    gen_grid->addWidget(lbl_gen_hint, 4, 0, 1, 2);

    btn_generate_ = new QPushButton("🎲 Generate Reproducible Topology", gen_box);
    btn_generate_->setStyleSheet("background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #3b82f6, stop:1 #2563eb); color: white; font-weight: bold; border: none; border-radius: 6px; padding: 8px;");
    connect(btn_generate_, &QPushButton::clicked, this, [this]() {
        handle_generate_random_topology(spin_node_count_->value(),
                                        spin_client_count_->value(),
                                        spin_density_->value(),
                                        spin_seed_->value());
    });
    gen_grid->addWidget(btn_generate_, 5, 0, 1, 2);

    btn_validate_ = new QPushButton("✔ Validate Topology", gen_box);
    connect(btn_validate_, &QPushButton::clicked, this, [this]() {
        string res = handle_validate_topology();
        QMessageBox::information(this, "Topology Validation", QString::fromStdString(res));
    });
    gen_grid->addWidget(btn_validate_, 6, 0, 1, 2);

    lbl_validation_status_ = new QLabel("Status: Ready", gen_box);
    lbl_validation_status_->setStyleSheet("font-weight: bold; color: #94a3b8; padding: 4px;");
    gen_grid->addWidget(lbl_validation_status_, 7, 0, 1, 2);

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

    btn_add_node_ = new QPushButton("➕ Add Node", manual_box);
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

    btn_add_link_ = new QPushButton("🔗 Add / Update Link", manual_box);
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

    btn_delete_selected_ = new QPushButton("🗑 Delete Selected Node / Link", manual_box);
    btn_delete_selected_->setStyleSheet("background-color: rgba(239, 68, 68, 0.15); color: #f87171; border: 1px solid #ef4444; border-radius: 6px; padding: 6px; font-weight: bold;");
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
    topo_scroll->setWidget(tab_topo);
    tabs->addTab(topo_scroll, "Topology");

    // ==========================================
    // TAB 2: File Transfer Setup & Progress
    // ==========================================
    QScrollArea* transfer_scroll = new QScrollArea(tabs);
    transfer_scroll->setWidgetResizable(true);
    QWidget* tab_transfer = new QWidget(transfer_scroll);
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

    btn_start_transfer_ = new QPushButton("🚀 Start Reliable File Transfer", setup_box);
    btn_start_transfer_->setStyleSheet("background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #10b981, stop:1 #059669); color: white; font-weight: bold; border: none; border-radius: 6px; padding: 8px; font-size: 12px;");
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
        start_guided_transfer(src_id, dst_id, edit_payload_->toPlainText().toStdString(), spin_chunk_size_->value());
    });
    setup_grid->addWidget(btn_start_transfer_, 4, 0, 1, 2);

    lbl_transfer_live_step_ = new QLabel("Nhấn 'Start Reliable File Transfer' để theo dõi từng bước cắt chunk, chọn đường Dijkstra, gửi DATA và xác nhận ACK.", setup_box);
    lbl_transfer_live_step_->setWordWrap(true);
    lbl_transfer_live_step_->setStyleSheet("padding: 8px; background-color: #0f172a; border: 1px solid #38bdf8; border-left: 4px solid #38bdf8; border-radius: 6px; color: #e2e8f0; font-size: 11px; font-weight: bold;");
    setup_grid->addWidget(lbl_transfer_live_step_, 5, 0, 1, 2);

    QHBoxLayout* xfer_nav_layout = new QHBoxLayout();
    btn_xfer_prev_ = new QPushButton("◀ Quay lại", setup_box);
    btn_xfer_prev_->setEnabled(false);
    btn_xfer_prev_->setStyleSheet("background-color: #1e293b; color: #e2e8f0; border: 1px solid #475569; border-radius: 6px; padding: 6px 10px; font-weight: bold;");
    connect(btn_xfer_prev_, &QPushButton::clicked, this, &MainWindow::on_xfer_prev_step);

    btn_xfer_next_ = new QPushButton("Tiếp tục ▶", setup_box);
    btn_xfer_next_->setEnabled(false);
    btn_xfer_next_->setStyleSheet("background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #10b981, stop:1 #059669); color: white; border: none; border-radius: 6px; padding: 6px 12px; font-weight: bold;");
    connect(btn_xfer_next_, &QPushButton::clicked, this, &MainWindow::on_xfer_next_step);

    btn_xfer_auto_ = new QPushButton("⏯ Tự động", setup_box);
    btn_xfer_auto_->setCheckable(true);
    btn_xfer_auto_->setEnabled(false);
    btn_xfer_auto_->setToolTip("Bật/tắt tự động chuyển bước truyền file sau mỗi 2.5 giây.");
    btn_xfer_auto_->setStyleSheet(
        "QPushButton { background-color: #1e293b; color: #38bdf8; border: 1px solid #38bdf8; border-radius: 6px; padding: 6px 10px; font-weight: bold; }"
        "QPushButton:checked { background-color: #0284c7; color: #ffffff; border: 1px solid #7dd3fc; }");
    connect(btn_xfer_auto_, &QPushButton::clicked, this, &MainWindow::on_xfer_toggle_auto);

    xfer_nav_layout->addWidget(btn_xfer_prev_);
    xfer_nav_layout->addWidget(btn_xfer_next_);
    xfer_nav_layout->addWidget(btn_xfer_auto_);
    setup_grid->addLayout(xfer_nav_layout, 6, 0, 1, 2);

    transfer_layout->addWidget(setup_box);

    // Progress Box
    QGroupBox* progress_box = new QGroupBox("Transfer Progress & Chunks", tab_transfer);
    QVBoxLayout* prog_layout = new QVBoxLayout(progress_box);

    QLabel* lbl_chunks_guide = new QLabel(
        "<b>Bảng theo dõi truyền tin cậy (Reliable Transfer):</b> File được cắt thành nhiều mảnh nhỏ (Chunks) để gửi qua mạng và kiểm chứng từng mảnh:<br>"
        "• <b>Seq #:</b> Thứ tự mảnh dữ liệu (0, 1, 2...). &nbsp; "
        "• <b>State:</b> Trạng thái (<i>PENDING</i> = chờ gửi, <i>IN_FLIGHT</i> = đang đi, <i>ACKED</i> = đích đã nhận, <i>FAILED</i> = quá 3 lần thử).<br>"
        "• <b>Attempts:</b> Số lần đã gửi/gửi lại (tối đa 3 lần nếu mất gói hoặc timeout). &nbsp; "
        "• <b>Size &amp; Checksum:</b> Kích thước (byte) và mã kiểm lỗi để phát hiện dữ liệu bị hỏng.",
        progress_box);
    lbl_chunks_guide->setWordWrap(true);
    lbl_chunks_guide->setStyleSheet("padding: 8px; background-color: #0b0f19; border: 1px solid #1f293d; border-left: 3px solid #10b981; border-radius: 6px; color: #cbd5e1; font-size: 11px;");
    prog_layout->addWidget(lbl_chunks_guide);

    lbl_transfer_status_ = new QLabel("Transfer Status: IDLE", progress_box);
    lbl_transfer_status_->setStyleSheet("background-color: rgba(100, 116, 139, 0.2); color: #94a3b8; border: 1px solid #64748b; border-radius: 12px; padding: 4px 12px; font-weight: bold;");
    prog_layout->addWidget(lbl_transfer_status_);

    progress_transfer_ = new QProgressBar(progress_box);
    progress_transfer_->setRange(0, 100);
    progress_transfer_->setValue(0);
    prog_layout->addWidget(progress_transfer_);

    table_chunks_ = new QTableWidget(progress_box);
    table_chunks_->setColumnCount(5);
    table_chunks_->setMinimumHeight(140);
    table_chunks_->setHorizontalHeaderLabels({"Seq #", "State", "Attempts", "Size", "Checksum"});
    table_chunks_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    prog_layout->addWidget(table_chunks_);

    transfer_layout->addWidget(progress_box);
    transfer_scroll->setWidget(tab_transfer);
    tabs->addTab(transfer_scroll, "Transfer");

    // ==========================================
    // TAB 3: Inspector & Fault Injection
    // ==========================================
    QWidget* tab_faults = new QWidget(tabs);
    QVBoxLayout* faults_layout = new QVBoxLayout(tab_faults);

    QGroupBox* inspector_box = new QGroupBox("Selected Node / Link Inspector", tab_faults);
    QVBoxLayout* insp_layout = new QVBoxLayout(inspector_box);

    lbl_selected_info_ = new QLabel("Click vào bất kỳ <b>Node (Client / Router)</b> hoặc <b>Link (đường nối truyền Packet)</b> trên sơ đồ bên trái để chọn và kiểm tra.", inspector_box);
    lbl_selected_info_->setWordWrap(true);
    lbl_selected_info_->setStyleSheet("padding: 10px; background-color: #0b0f19; border: 1px solid #1f293d; border-left: 4px solid #38bdf8; border-radius: 6px; color: #f1f5f9; line-height: 1.5;");
    insp_layout->addWidget(lbl_selected_info_);

    QLabel* lbl_faults_guide = new QLabel(
        "<b>Hướng dẫn tương tác &amp; Tiêm lỗi (Fault Injection):</b><br>"
        "• <b>Chọn đối tượng:</b> Bạn có thể click chọn <b>Link</b> (đường nối mang packet) hoặc <b>Packet Node</b> (Client / Router) trực tiếp trên sơ đồ.<br>"
        "• <b>Toggle Selected Failure / Recovery:</b> Tắt (Offline/Failed) hoặc bật lại (Active) Node hoặc Link đang chọn để mô phỏng sự cố đứt cáp / sập router và xem Dijkstra tự đổi đường.<br>"
        "• <b>Force Drop Next Packet (chỉ dùng khi chọn Link):</b> Ép làm rơi gói tin kế tiếp đi qua Link này (mô phỏng nghẽn/rớt gói) để quan sát cơ chế Timeout &amp; gửi lại (Retry).<br>"
        "• <b>Force Corrupt Next Packet (chỉ dùng khi chọn Link):</b> Làm sai lệch dữ liệu gói tin kế tiếp đi qua Link này để kiểm tra cơ chế phát hiện lỗi Checksum &amp; yêu cầu gửi lại.",
        inspector_box);
    lbl_faults_guide->setWordWrap(true);
    lbl_faults_guide->setStyleSheet("padding: 8px; background-color: #0b0f19; border: 1px solid #1f293d; border-left: 3px solid #f59e0b; border-radius: 6px; color: #cbd5e1; font-size: 11px;");
    insp_layout->addWidget(lbl_faults_guide);

    btn_toggle_fail_ = new QPushButton("⚡ Toggle Selected Failure / Recovery", inspector_box);
    btn_toggle_fail_->setToolTip("Bật/Tắt trạng thái hoạt động của Node (Client/Router) hoặc Link đang chọn trên sơ đồ.");
    btn_toggle_fail_->setStyleSheet("background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ef4444, stop:1 #dc2626); color: white; font-weight: bold; border: none; border-radius: 6px; padding: 8px;");
    connect(btn_toggle_fail_, &QPushButton::clicked, this, &MainWindow::handle_toggle_failure_selected);
    insp_layout->addWidget(btn_toggle_fail_);

    btn_inject_loss_ = new QPushButton("💧 Force Drop Next Packet on Selected Link", inspector_box);
    btn_inject_loss_->setToolTip("Chọn 1 Link trên sơ đồ trước: ép rớt gói tin tiếp theo đi qua Link đó.");
    connect(btn_inject_loss_, &QPushButton::clicked, this, &MainWindow::handle_inject_loss_selected);
    insp_layout->addWidget(btn_inject_loss_);

    btn_inject_corruption_ = new QPushButton("☣ Force Corrupt Next Packet on Selected Link", inspector_box);
    btn_inject_corruption_->setToolTip("Chọn 1 Link trên sơ đồ trước: làm hỏng checksum gói tin tiếp theo đi qua Link đó.");
    connect(btn_inject_corruption_, &QPushButton::clicked, this, &MainWindow::handle_inject_corruption_selected);
    insp_layout->addWidget(btn_inject_corruption_);

    faults_layout->addWidget(inspector_box);
    faults_layout->addStretch();
    tabs->addTab(tab_faults, "Inspector & Faults");

    // ==========================================
    // TAB 4: Routing & Queues
    // ==========================================
    QScrollArea* routing_scroll = new QScrollArea(tabs);
    routing_scroll->setWidgetResizable(true);
    QWidget* tab_routing = new QWidget(routing_scroll);
    QVBoxLayout* routing_layout = new QVBoxLayout(tab_routing);

    QGroupBox* route_box = new QGroupBox("Current Lowest-Cost Shortest Path", tab_routing);
    QVBoxLayout* r_box_layout = new QVBoxLayout(route_box);
    lbl_active_route_ = new QLabel("No active route calculated.", route_box);
    lbl_active_route_->setStyleSheet("font-weight: bold; color: #34d399; font-size: 13px; padding: 8px; background-color: #0b0f19; border: 1px solid #1f293d; border-radius: 6px;");
    lbl_route_cost_ = new QLabel("Cost: N/A", route_box);
    lbl_route_cost_->setStyleSheet("color: #38bdf8; font-weight: bold; padding: 4px 8px;");
    lbl_route_breakdown_ = new QLabel("Công thức Dijkstra: Cost = Latency(ms) + (100 / Bandwidth KB/s) + (2 × Loss% × 100)", route_box);
    lbl_route_breakdown_->setWordWrap(true);
    lbl_route_breakdown_->setStyleSheet("padding: 8px; background-color: #0b0f19; border: 1px solid #1f293d; border-left: 3px solid #38bdf8; border-radius: 6px; color: #cbd5e1; font-size: 11px;");
    r_box_layout->addWidget(lbl_active_route_);
    r_box_layout->addWidget(lbl_route_cost_);
    r_box_layout->addWidget(lbl_route_breakdown_);
    routing_layout->addWidget(route_box);

    QGroupBox* queue_box = new QGroupBox("Link Queue Occupancy", tab_routing);
    QVBoxLayout* q_layout = new QVBoxLayout(queue_box);
    table_queues_ = new QTableWidget(queue_box);
    table_queues_->setColumnCount(5);
    table_queues_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table_queues_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table_queues_->setHorizontalHeaderLabels({"Link", "Dijkstra Cost", "Queue Depth", "Capacity", "Status"});
    table_queues_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    q_layout->addWidget(table_queues_);
    routing_layout->addWidget(queue_box);
    routing_layout->addStretch();
    routing_scroll->setWidget(tab_routing);
    tabs->addTab(routing_scroll, "Routing • Queues");

    // ==========================================
    // TAB 5: Coherent Demonstrations (SPEC.md Section 3)
    // ==========================================
    QScrollArea* demo_scroll = new QScrollArea(tabs);
    demo_scroll->setWidgetResizable(true);
    QWidget* tab_demo = new QWidget(demo_scroll);
    QVBoxLayout* demo_layout = new QVBoxLayout(tab_demo);

    QGroupBox* demo_box = new QGroupBox("End-to-End Demonstrations", tab_demo);
    QVBoxLayout* d_layout = new QVBoxLayout(demo_box);

    QPushButton* btn_probe = new QPushButton("📡 Send Hop-by-Hop Route Probe", demo_box);
    btn_probe->setStyleSheet("background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #8b5cf6, stop:1 #7c3aed); color: white; font-weight: bold; border: none; border-radius: 6px; padding: 8px;");
    connect(btn_probe, &QPushButton::clicked, this, &MainWindow::handle_send_route_probe);
    d_layout->addWidget(btn_probe);

    QPushButton* btn_demo1 = new QPushButton("⚡ Run Scenario 1: Dynamic Recovery & Rerouting", demo_box);
    btn_demo1->setStyleSheet("background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #3b82f6, stop:1 #1d4ed8); color: white; font-weight: bold; border: none; border-radius: 6px; padding: 10px; font-size: 12px;");
    connect(btn_demo1, &QPushButton::clicked, this, &MainWindow::start_animated_demo1);
    d_layout->addWidget(btn_demo1);

    QPushButton* btn_demo2 = new QPushButton("⛔ Run Scenario 2: Controlled Retry-Limit Failure", demo_box);
    btn_demo2->setStyleSheet("background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ef4444, stop:1 #b91c1c); color: white; font-weight: bold; border: none; border-radius: 6px; padding: 10px; font-size: 12px;");
    connect(btn_demo2, &QPushButton::clicked, this, &MainWindow::start_animated_demo2);
    d_layout->addWidget(btn_demo2);

    lbl_demo_live_step_ = new QLabel("Sẵn sàng chạy trình diễn từng bước. Nhấn Scenario 1 hoặc Scenario 2 để xem mô phỏng trực quan trên sơ đồ.", demo_box);
    lbl_demo_live_step_->setWordWrap(true);
    lbl_demo_live_step_->setStyleSheet("padding: 10px; background-color: #0f172a; border: 1px solid #38bdf8; border-left: 4px solid #38bdf8; border-radius: 6px; color: #e2e8f0; font-size: 12px; font-weight: bold;");
    d_layout->addWidget(lbl_demo_live_step_);

    QHBoxLayout* demo_nav_layout = new QHBoxLayout();
    btn_demo_prev_ = new QPushButton("◀ Quay lại", demo_box);
    btn_demo_prev_->setEnabled(false);
    btn_demo_prev_->setStyleSheet("background-color: #1e293b; color: #e2e8f0; border: 1px solid #475569; border-radius: 6px; padding: 7px 10px; font-weight: bold;");
    connect(btn_demo_prev_, &QPushButton::clicked, this, &MainWindow::on_demo_prev_step);

    btn_demo_next_ = new QPushButton("Tiếp tục ▶", demo_box);
    btn_demo_next_->setEnabled(false);
    btn_demo_next_->setStyleSheet("background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #10b981, stop:1 #059669); color: white; border: none; border-radius: 6px; padding: 7px 12px; font-weight: bold;");
    connect(btn_demo_next_, &QPushButton::clicked, this, &MainWindow::on_demo_next_step);

    btn_demo_auto_ = new QPushButton("⏯ Tự động", demo_box);
    btn_demo_auto_->setCheckable(true);
    btn_demo_auto_->setEnabled(false);
    btn_demo_auto_->setToolTip("Bật/tắt tự động chuyển bước sau mỗi 2.5 giây.");
    btn_demo_auto_->setStyleSheet(
        "QPushButton { background-color: #1e293b; color: #38bdf8; border: 1px solid #38bdf8; border-radius: 6px; padding: 7px 10px; font-weight: bold; }"
        "QPushButton:checked { background-color: #0284c7; color: #ffffff; border: 1px solid #7dd3fc; }");
    connect(btn_demo_auto_, &QPushButton::clicked, this, &MainWindow::on_demo_toggle_auto);

    demo_nav_layout->addWidget(btn_demo_prev_);
    demo_nav_layout->addWidget(btn_demo_next_);
    demo_nav_layout->addWidget(btn_demo_auto_);
    d_layout->addLayout(demo_nav_layout);

    QLabel* lbl_demo_desc = new QLabel(
        "<b>Scenario 1 (Khôi phục &amp; Đổi đường từng bước):</b><br>"
        "1. Client1 dò các router kề (nếu gặp router hỏng/ngắt kết nối sẽ hiện dấu ✖ đỏ, sau đó chuyển sang đường sống).<br>"
        "2. Tại mỗi router trung gian, hệ thống so sánh tốc độ (ms &amp; KB/s) giữa các nhánh để loại nhánh chậm và chọn đường tối ưu tới đích.<br>"
        "3. Giữa chừng 1 router bị ngắt đột ngột ➔ Dijkstra tính lại đường thay thế và truyền nốt các chunk còn thiếu.<br><br>"
        "<b>Scenario 2 (Tại sao ngắt 2 link nối tới Client đích?):</b><br>"
        "• Client đích chỉ nối với mạng qua <b>2 đường cuối (last-hop links)</b>. Nếu chỉ ngắt 1 link hoặc 1 router giữa mạng, Dijkstra vẫn tìm được đường vòng khác (giống Scenario 1).<br>"
        "• Do đó, để tái hiện kịch bản <b>mất kết nối hoàn toàn (Destination Unreachable)</b>, kịch bản ngắt đồng thời cả 2 link cuối vào Client đích ➔ Không còn bất kỳ đường đi nào ➔ Gói tin không nhận được ACK, tự động thử lại tối đa 3 lần rồi dừng an toàn.",
        demo_box);
    lbl_demo_desc->setWordWrap(true);
    lbl_demo_desc->setStyleSheet("padding: 10px; background-color: #0b0f19; border: 1px solid #1f293d; border-left: 4px solid #8b5cf6; border-radius: 6px; color: #cbd5e1; line-height: 1.5; font-size: 11px;");
    d_layout->addWidget(lbl_demo_desc);

    demo_layout->addWidget(demo_box);
    demo_layout->addStretch();
    demo_scroll->setWidget(tab_demo);
    tabs->addTab(demo_scroll, "Demonstrations");

    return tabs;
}

QWidget* MainWindow::create_bottom_panel() {
    QGroupBox* bottom_box = new QGroupBox("Packet Events & Statistics", this);
    QVBoxLayout* layout = new QVBoxLayout(bottom_box);
    layout->setContentsMargins(10, 6, 10, 6);

    QHBoxLayout* top_bar = new QHBoxLayout();
    lbl_stats_summary_ = new QLabel("Sent: 0 | Delivered: 0 | Dropped: 0 | Lost: 0 | Corrupted: 0 | Heartbeat Timeouts: 0 | Acks: 0 | Retries: 0", bottom_box);
    lbl_stats_summary_->setStyleSheet("font-weight: bold; color: #f1f5f9; padding: 4px; font-size: 11px;");
    top_bar->addWidget(lbl_stats_summary_);
    top_bar->addStretch();

    btn_clear_log_ = new QPushButton("🗑 Clear Log", bottom_box);
    btn_clear_log_->setStyleSheet("background-color: #1e293b; color: #94a3b8; border: 1px solid #334155; border-radius: 4px; padding: 4px 10px; font-size: 11px;");
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
    lbl_sim_status_->setStyleSheet("background-color: rgba(16, 185, 129, 0.2); color: #34d399; border: 1px solid #10b981; border-radius: 12px; padding: 4px 12px; font-weight: bold; font-size: 11px;");
    append_log_entry(simulation_service_.get_current_tick(), "Simulation", "Simulation started", "");
}

void MainWindow::handle_pause_simulation() {
    simulation_service_.pause();
    sim_timer_->stop();
    lbl_sim_status_->setText("Status: PAUSED");
    lbl_sim_status_->setStyleSheet("background-color: rgba(245, 158, 11, 0.2); color: #fbbf24; border: 1px solid #f59e0b; border-radius: 12px; padding: 4px 12px; font-weight: bold; font-size: 11px;");
    append_log_entry(simulation_service_.get_current_tick(), "Simulation", "Simulation paused", "");
}

void MainWindow::handle_resume_simulation() {
    simulation_service_.resume();
    sim_timer_->start(50);
    lbl_sim_status_->setText("Status: RUNNING");
    lbl_sim_status_->setStyleSheet("background-color: rgba(16, 185, 129, 0.2); color: #34d399; border: 1px solid #10b981; border-radius: 12px; padding: 4px 12px; font-weight: bold; font-size: 11px;");
    append_log_entry(simulation_service_.get_current_tick(), "Simulation", "Simulation resumed", "");
}

void MainWindow::handle_reset_simulation() {
    sim_timer_->stop();
    if (demo_timer_) demo_timer_->stop();
    if (xfer_timer_) xfer_timer_->stop();
    demo_mode_ = 0;
    demo_step_ = 0;
    xfer_step_active_ = false;
    xfer_step_ = 0;
    update_demo_nav_buttons();
    update_xfer_nav_buttons();
    simulation_service_.reset();
    active_source_id_ = -1;
    active_destination_id_ = -1;
    next_chunk_to_send_idx_ = 0;
    last_processed_delivered_idx_ = 0;
    discovered_probe_path_ = "";
    transfer_chunks_.clear();

    lbl_sim_status_->setText("Status: STOPPED");
    lbl_sim_status_->setStyleSheet("background-color: rgba(100, 116, 139, 0.2); color: #94a3b8; border: 1px solid #64748b; border-radius: 12px; padding: 4px 12px; font-weight: bold; font-size: 11px;");
    lbl_sim_tick_->setText("Tick: 0 ms");
    lbl_transfer_status_->setText("Transfer Status: IDLE");
    lbl_transfer_status_->setStyleSheet("background-color: rgba(100, 116, 139, 0.2); color: #94a3b8; border: 1px solid #64748b; border-radius: 12px; padding: 4px 12px; font-weight: bold; font-size: 11px;");
    progress_transfer_->setValue(0);
    table_chunks_->setRowCount(0);

    canvas_->clear_probe_visuals();
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

    canvas_->clear_probe_visuals();
    canvas_->reset_layout();
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
    string summary = report.is_valid
        ? "Topology is VALID & fully connected.\n\n"
          "Đã kiểm tra:\n"
          "- Toàn bộ các Client đều có ít nhất 1 đường đi thông suốt tới nhau.\n"
          "- Không có node bị cô lập hoàn toàn hoặc thiếu liên kết hoạt động."
        : "Topology WARNINGS detected:\n";
    for (int i = 0; i < report.warnings.size(); ++i) {
        summary += "- " + report.warnings[i] + "\n";
    }
    if (!report.is_valid) {
        summary += "\nGợi ý: Hãy nối thêm Link hoặc phục hồi các Node/Link đang bị Offline trong tab Inspector & Faults.";
    }
    lbl_validation_status_->setText(report.is_valid ? "Status: VALID" : "Status: WARNINGS");
    lbl_validation_status_->setStyleSheet(report.is_valid ? "color: #34d399; font-weight: bold;" : "color: #f87171; font-weight: bold;");
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
    lbl_transfer_status_->setStyleSheet("background-color: rgba(59, 130, 246, 0.2); color: #60a5fa; border: 1px solid #3b82f6; border-radius: 12px; padding: 4px 12px; font-weight: bold; font-size: 11px;");
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
            if (acked && transfer_service_.is_transfer_completed() && transfer_service_.is_reconstruction_ready()) {
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

    QString status_badge = n->is_active() ? "<span style='color:#34d399; font-weight:bold;'>● ACTIVE</span>"
                                          : "<span style='color:#f87171; font-weight:bold;'>● FAILED</span>";
    QString info = QString("<div style='line-height: 1.6;'>"
                           "<b style='color:#38bdf8; font-size:12px;'>Selected Node: ID %1</b><br>"
                           "<b>Name:</b> %2<br>"
                           "<b>Type:</b> %3<br>"
                           "<b>Status:</b> %4</div>")
                       .arg(node_id)
                       .arg(QString::fromStdString(n->get_name()))
                       .arg(QString::fromStdString(n->get_type()))
                       .arg(status_badge);
    lbl_selected_info_->setText(info);
}

void MainWindow::on_link_selected(int node_a, int node_b) {
    model::Link* l = graph_.get_link(node_a, node_b);
    if (l == nullptr) return;

    QString status_badge = l->is_failed() ? "<span style='color:#f87171; font-weight:bold;'>● FAILED</span>"
                                          : "<span style='color:#34d399; font-weight:bold;'>● ACTIVE</span>";
    QString info = QString("<div style='line-height: 1.6;'>"
                           "<b style='color:#38bdf8; font-size:12px;'>Selected Link: (%1 ⮂ %2)</b><br>"
                           "<b>Bandwidth:</b> %3 KB/s<br>"
                           "<b>Latency:</b> %4 ms<br>"
                           "<b>Loss Prob:</b> %5<br>"
                           "<b>Corruption Prob:</b> %6<br>"
                           "<b>Queue Occupancy:</b> %7 / %8<br>"
                           "<b>Status:</b> %9</div>")
                       .arg(node_a).arg(node_b)
                       .arg(l->get_bandwidth())
                       .arg(l->get_latency())
                       .arg(l->get_loss_probability())
                       .arg(l->get_corruption_probability())
                       .arg(l->queue_size())
                       .arg(l->queue_capacity())
                       .arg(status_badge);
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
        if (lbl_selected_info_) {
            lbl_selected_info_->setText(QString("<b>Đã kích hoạt Force Drop</b> trên Link <b>%1 ⮂ %2</b>.<br>Gói tin kế tiếp đi qua Link này sẽ bị đánh rơi để kiểm tra Retry/Timeout.")
                                        .arg(sel_link.first).arg(sel_link.second));
        }
    } else {
        QMessageBox::information(this, "Chưa chọn Link",
                                 "Chức năng Force Drop chỉ áp dụng cho đường nối (Link).\nHãy click vào một đoạn dây nối giữa 2 node trên sơ đồ rồi bấm lại.");
    }
}

void MainWindow::handle_inject_corruption_selected() {
    pair<int, int> sel_link = canvas_->get_selected_link();
    if (sel_link.first != -1) {
        simulation_service_.inject_link_corruption(sel_link.first, sel_link.second);
        append_log_entry(simulation_service_.get_current_tick(), "Fault",
                         "Arming manual packet corruption on Link (" + to_string(sel_link.first) + " - " + to_string(sel_link.second) + ")", "Next packet will corrupt");
        if (lbl_selected_info_) {
            lbl_selected_info_->setText(QString("<b>Đã kích hoạt Force Corrupt</b> trên Link <b>%1 ⮂ %2</b>.<br>Gói tin kế tiếp đi qua Link này sẽ bị làm hỏng dữ liệu để kiểm tra Checksum.")
                                        .arg(sel_link.first).arg(sel_link.second));
        }
    } else {
        QMessageBox::information(this, "Chưa chọn Link",
                                 "Chức năng Force Corrupt chỉ áp dụng cho đường nối (Link).\nHãy click vào một đoạn dây nối giữa 2 node trên sơ đồ rồi bấm lại.");
    }
}

void MainWindow::update_route_display() {
    if (active_source_id_ == -1 || active_destination_id_ == -1) {
        lbl_active_route_->setText("Select Source & Destination clients to compute route.");
        lbl_route_cost_->setText("Cost: N/A");
        if (lbl_route_breakdown_) {
            lbl_route_breakdown_->setText("Công thức Dijkstra: Cost = Latency(ms) + (100 / Bandwidth KB/s) + (2 × Loss% × 100)");
        }
        return;
    }

    services::RoutingService::Route r = routing_service_.find_route(graph_, active_source_id_, active_destination_id_);
    canvas_->set_active_route(r.path);

    if (!r.found || r.path.size() < 2) {
        lbl_active_route_->setText("<span style='color:#f87171; font-weight:bold;'>NO ROUTE AVAILABLE (Destination Unreachable)</span>");
        lbl_route_cost_->setText("Cost: INFINITY");
        if (lbl_route_breakdown_) {
            lbl_route_breakdown_->setText("Không có đường đi khả dụng giữa nguồn và đích (tất cả nhánh có thể đã bị Offline/Failed).");
        }
    } else {
        QString path_str = "";
        QString breakdown_str = "<b>Chi tiết chi phí từng chặng (Dijkstra Cost):</b><br>"
                                "<i>Công thức mỗi chặng: Latency(ms) + 100 / Bandwidth(KB/s) + 2 × Loss(%)</i><br>";
        double sum_check = 0.0;
        for (int i = 0; i < r.path.size(); ++i) {
            model::Node* n = graph_.get_node(r.path[i]);
            QString name = n ? QString::fromStdString(n->get_name()) : QString::number(r.path[i]);
            path_str += QString("<span style='color:#38bdf8; font-weight:bold;'>%1</span>").arg(name);
            if (i < r.path.size() - 1) {
                path_str += " <span style='color:#10b981;'>➔</span> ";
                model::Node* next_n = graph_.get_node(r.path[i + 1]);
                QString next_name = next_n ? QString::fromStdString(next_n->get_name()) : QString::number(r.path[i + 1]);
                const model::Link* lnk = graph_.get_link(r.path[i], r.path[i + 1]);
                if (lnk) {
                    double hop_cost = routing_service_.calculate_link_cost(*lnk);
                    sum_check += hop_cost;
                    double bw_term = (lnk->get_bandwidth() > 0) ? (routing_service_.get_reference_bandwidth() / lnk->get_bandwidth()) : 0.0;
                    double loss_term = routing_service_.get_loss_weight() * (lnk->get_loss_probability() * 100.0);
                    breakdown_str += QString("• <b>%1 ➔ %2</b>: %3 ms + (100/%4) [%5] + loss [%6] = <b style='color:#34d399;'>%7</b><br>")
                        .arg(name).arg(next_name)
                        .arg(lnk->get_latency())
                        .arg(lnk->get_bandwidth())
                        .arg(bw_term, 0, 'f', 2)
                        .arg(loss_term, 0, 'f', 2)
                        .arg(hop_cost, 0, 'f', 2);
                }
            }
        }
        breakdown_str += QString("<b>Tổng cộng: %1</b>").arg(sum_check, 0, 'f', 2);
        lbl_active_route_->setText(path_str);
        lbl_route_cost_->setText(QString("Dijkstra Metric Cost: <b style='color:#38bdf8;'>%1</b>").arg(r.total_cost, 0, 'f', 2));
        if (lbl_route_breakdown_) {
            lbl_route_breakdown_->setText(breakdown_str);
        }
    }
}

void MainWindow::update_queue_table() {
    const dsa::DynamicArray<model::Link*>& links = graph_.get_all_links();
    table_queues_->setRowCount(links.size());

    for (int i = 0; i < links.size(); ++i) {
        model::Link* l = links[i];
        if (l == nullptr) continue;
        QString name = QString("%1 ⮂ %2").arg(l->get_first_node_id()).arg(l->get_second_node_id());
        double cost = routing_service_.calculate_link_cost(*l);
        table_queues_->setItem(i, 0, new QTableWidgetItem(name));
        table_queues_->setItem(i, 1, new QTableWidgetItem(QString::number(cost, 'f', 2)));
        table_queues_->setItem(i, 2, new QTableWidgetItem(QString::number(l->queue_size())));
        table_queues_->setItem(i, 3, new QTableWidgetItem(QString::number(l->queue_capacity())));

        QString status_str = l->is_failed() ? "FAILED" : (l->queue_full() ? "FULL" : "NORMAL");
        QColor status_col = l->is_failed() ? QColor(248, 113, 113) : (l->queue_full() ? QColor(251, 191, 36) : QColor(52, 211, 153));
        QTableWidgetItem* item_status = new QTableWidgetItem(status_str);
        item_status->setForeground(status_col);
        QFont f = item_status->font();
        f.setBold(true);
        item_status->setFont(f);
        table_queues_->setItem(i, 4, item_status);
    }

    table_queues_->resizeRowsToContents();
    int total_h = table_queues_->horizontalHeader()->height() + 6;
    for (int r = 0; r < table_queues_->rowCount(); ++r) {
        total_h += table_queues_->rowHeight(r);
    }
    table_queues_->setFixedHeight(max(120, total_h));
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
        lbl_transfer_status_->setStyleSheet("background-color: rgba(16, 185, 129, 0.2); color: #34d399; border: 1px solid #10b981; border-radius: 12px; padding: 4px 12px; font-weight: bold; font-size: 11px;");
    } else if (transfer_service_.is_transfer_failed()) {
        lbl_transfer_status_->setText("Transfer Status: FAILED (Retry Limit Exceeded)");
        lbl_transfer_status_->setStyleSheet("background-color: rgba(239, 68, 68, 0.2); color: #f87171; border: 1px solid #ef4444; border-radius: 12px; padding: 4px 12px; font-weight: bold; font-size: 11px;");
    }

    for (int i = 0; i < transfer_chunks_.size(); ++i) {
        services::TransferService::ChunkProgress prog = transfer_service_.get_chunk_progress(i);
        QString state_str = "UNSENT";
        QColor state_color = QColor(148, 163, 184);

        if (prog.state == services::TransferService::ChunkState::IN_FLIGHT) {
            state_str = "IN_FLIGHT";
            state_color = QColor(56, 189, 248);
        } else if (prog.state == services::TransferService::ChunkState::ACKNOWLEDGED) {
            state_str = "ACKNOWLEDGED";
            state_color = QColor(52, 211, 153);
        } else if (prog.state == services::TransferService::ChunkState::TIMED_OUT) {
            state_str = "TIMED_OUT";
            state_color = QColor(251, 191, 36);
        } else if (prog.state == services::TransferService::ChunkState::FAILED) {
            state_str = "FAILED";
            state_color = QColor(248, 113, 113);
        }

        QTableWidgetItem* item_state = new QTableWidgetItem(state_str);
        item_state->setForeground(state_color);
        QFont f = item_state->font();
        f.setBold(true);
        item_state->setFont(f);
        table_chunks_->setItem(i, 1, item_state);

        QTableWidgetItem* item_attempts = new QTableWidgetItem(QString::number(prog.attempts));
        item_attempts->setForeground(QColor(241, 245, 249));
        table_chunks_->setItem(i, 2, item_attempts);
    }
}

void MainWindow::update_statistics_display() {
    services::SimulationService::Statistics sim_stats = simulation_service_.get_statistics();
    services::TransferService::TransferStatistics tr_stats = transfer_service_.get_statistics();

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

void MainWindow::append_log_entry(int tick, const string& source, const string& desc, const string& details) {
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

void MainWindow::setup_initial_startup_topology() {
    handle_reset_simulation();

    graph_ = model::NetworkGraph();
    canvas_->set_network_graph(&graph_);

    // Startup layout: Client1 (ID 1) at left/start, Router2..5 (ID 2..5) in middle, Client6 (ID 6) at right/end
    graph_.add_client(1, "Client1");
    graph_.add_router(2, "Router2");
    graph_.add_router(3, "Router3");
    graph_.add_router(4, "Router4");
    graph_.add_router(5, "Router5");
    graph_.add_client(6, "Client6");

    // Left end: Client1 -> Router2, Router3
    graph_.add_link(1, 2, 120, 5, 0.0, 0.0, 10);
    graph_.add_link(1, 3, 80, 15, 0.0, 0.0, 10);

    // Middle routers: structured non-overlapping mesh
    graph_.add_link(2, 4, 150, 10, 0.0, 0.0, 10);
    graph_.add_link(2, 5, 100, 15, 0.0, 0.0, 10);
    graph_.add_link(3, 4, 100, 15, 0.0, 0.0, 10);
    graph_.add_link(3, 5, 200, 10, 0.0, 0.0, 10);
    graph_.add_link(2, 3, 150, 10, 0.0, 0.0, 10);
    graph_.add_link(4, 5, 150, 10, 0.0, 0.0, 10);

    // Right end: Router4, Router5 -> Client6
    graph_.add_link(4, 6, 120, 5, 0.0, 0.0, 10);
    graph_.add_link(5, 6, 80, 15, 0.0, 0.0, 10);

    canvas_->reset_layout();
    refresh_client_combos();
    active_source_id_ = 1;
    active_destination_id_ = 6;
    if (combo_src_client_) {
        int idx1 = combo_src_client_->findData(1);
        if (idx1 >= 0) combo_src_client_->setCurrentIndex(idx1);
    }
    if (combo_dst_client_) {
        int idx6 = combo_dst_client_->findData(6);
        if (idx6 >= 0) combo_dst_client_->setCurrentIndex(idx6);
    }
    canvas_->update();
    update_route_display();
    update_queue_table();
    handle_validate_topology();
    append_log_entry(0, "Topology", "Initialized clean startup topology (Client1 -> Routers 2..5 -> Client6)", "6 nodes, 10 links");
}

void MainWindow::start_animated_demo1() {
    if (demo_timer_) demo_timer_->stop();
    if (btn_demo_auto_) btn_demo_auto_->setChecked(false);
    demo_mode_ = 1;
    demo_step_ = 0;
    apply_demo_step(0);
}

void MainWindow::start_animated_demo2() {
    if (demo_timer_) demo_timer_->stop();
    if (btn_demo_auto_) btn_demo_auto_->setChecked(false);
    demo_mode_ = 2;
    demo_step_ = 0;
    apply_demo_step(0);
}

void MainWindow::update_demo_nav_buttons() {
    if (!btn_demo_prev_ || !btn_demo_next_ || !btn_demo_auto_) return;
    if (demo_mode_ == 0) {
        btn_demo_prev_->setEnabled(false);
        btn_demo_next_->setEnabled(false);
        btn_demo_auto_->setEnabled(false);
        btn_demo_auto_->setChecked(false);
        btn_demo_auto_->setText("⏯ Tự động");
        return;
    }
    int max_step = (demo_mode_ == 1) ? 5 : 3;
    btn_demo_prev_->setEnabled(demo_step_ > 0);
    btn_demo_next_->setEnabled(demo_step_ < max_step);
    btn_demo_auto_->setEnabled(demo_step_ < max_step);
    if (demo_step_ >= max_step && demo_timer_) {
        demo_timer_->stop();
        btn_demo_auto_->setChecked(false);
    }
    btn_demo_auto_->setText(btn_demo_auto_->isChecked() ? "⏸ Dừng tự động" : "⏯ Tự động");
}

void MainWindow::on_demo_prev_step() {
    if (demo_mode_ == 0 || demo_step_ <= 0) return;
    if (demo_timer_) demo_timer_->stop();
    if (btn_demo_auto_) btn_demo_auto_->setChecked(false);
    apply_demo_step(demo_step_ - 1);
}

void MainWindow::on_demo_next_step() {
    if (demo_mode_ == 0) return;
    int max_step = (demo_mode_ == 1) ? 5 : 3;
    if (demo_step_ >= max_step) return;
    apply_demo_step(demo_step_ + 1);
}

void MainWindow::on_demo_toggle_auto() {
    if (!demo_timer_ || !btn_demo_auto_) return;
    if (btn_demo_auto_->isChecked() && demo_mode_ != 0) {
        btn_demo_auto_->setText("⏸ Dừng tự động");
        int max_step = (demo_mode_ == 1) ? 5 : 3;
        if (demo_step_ < max_step) {
            apply_demo_step(demo_step_ + 1);
        }
        if (btn_demo_auto_->isChecked() && demo_step_ < max_step) {
            demo_timer_->start(2500);
        }
    } else {
        demo_timer_->stop();
        btn_demo_auto_->setText("⏯ Tự động");
    }
}

void MainWindow::on_demo_timer_tick() {
    if (demo_mode_ == 0) {
        if (demo_timer_) demo_timer_->stop();
        return;
    }
    int max_step = (demo_mode_ == 1) ? 5 : 3;
    if (demo_step_ < max_step) {
        apply_demo_step(demo_step_ + 1);
    } else {
        if (demo_timer_) demo_timer_->stop();
        if (btn_demo_auto_) btn_demo_auto_->setChecked(false);
        update_demo_nav_buttons();
    }
}

void MainWindow::apply_demo_step(int step) {
    int saved_mode = demo_mode_;
    bool was_auto = (btn_demo_auto_ && btn_demo_auto_->isChecked());
    if (saved_mode == 1) {
        int target_step = max(0, min(step, 5));
        // Rebuild clean state for deterministic forward/backward stepping
        if (demo_timer_) demo_timer_->stop();
        handle_generate_random_topology(6, 2, 0.45, 12345);
        demo_mode_ = 1;
        demo_step_ = target_step;
        if (btn_demo_auto_) btn_demo_auto_->setChecked(was_auto);
        handle_validate_topology();
        active_source_id_ = 1;
        active_destination_id_ = 2;
        demo_failed_router_ = 4;
        simulation_service_.fail_node_manually(demo_failed_router_);
        update_route_display();

        services::RoutingService::Route r = routing_service_.find_route(graph_, 1, 2);
        demo_alt_route_ = r.path;

        if (demo_step_ == 0) {
            QVector<TopologyCanvas::ProbeEdge> probes;
            probes.push_back({1, 4, 0, "Dò đường..."});
            probes.push_back({1, 3, 0, "Dò đường..."});
            canvas_->set_probe_visuals(probes);
            if (lbl_demo_live_step_) {
                lbl_demo_live_step_->setText("Bước 1/6: Client1 bắt đầu nối thử tới cả 2 router kề là Router4 và Router3...\n(Nhấn 'Tiếp tục ▶' để xem kết quả dò)");
            }
        } else if (demo_step_ == 1) {
            QVector<TopologyCanvas::ProbeEdge> probes;
            probes.push_back({1, 4, 1, "✖ Ngắt kết nối"});
            probes.push_back({1, 3, 2, "✔ 5ms • 100KB/s"});
            canvas_->set_probe_visuals(probes);
            if (lbl_demo_live_step_) {
                lbl_demo_live_step_->setText("Bước 2/6: Router4 bị hỏng nên hiện dấu ✖ đỏ (thất bại). Client1 chuyển sang nối qua Router3 thành công!");
            }
        } else if (demo_step_ == 2) {
            QVector<TopologyCanvas::ProbeEdge> probes;
            probes.push_back({1, 3, 2, "✔ Đã chọn"});
            const dsa::DynamicArray<int>& nbrs = graph_.get_neighbors(3);
            for (int i = 0; i < nbrs.size(); ++i) {
                int nb = nbrs[i];
                if (nb == 1 || nb == 4) continue;
                probes.push_back({3, nb, 0, "So sánh ms/KB/s..."});
            }
            canvas_->set_probe_visuals(probes);
            if (lbl_demo_live_step_) {
                lbl_demo_live_step_->setText("Bước 3/6: Tại Router3, hệ thống kiểm tra các đường đi tiếp để so sánh độ trễ (ms) và băng thông (KB/s)...");
            }
        } else if (demo_step_ == 3) {
            QVector<TopologyCanvas::ProbeEdge> probes;
            int chosen_next = (demo_alt_route_.size() >= 3) ? demo_alt_route_[2] : 5;
            probes.push_back({1, 3, 2, "✔"});
            const dsa::DynamicArray<int>& nbrs = graph_.get_neighbors(3);
            for (int i = 0; i < nbrs.size(); ++i) {
                int nb = nbrs[i];
                if (nb == 1 || nb == 4) continue;
                if (nb == chosen_next) {
                    probes.push_back({3, nb, 2, "✔ Nhanh hơn"});
                } else {
                    probes.push_back({3, nb, 3, "Chậm hơn (bỏ)"});
                }
            }
            for (int i = 2; i < demo_alt_route_.size() - 1; ++i) {
                probes.push_back({demo_alt_route_[i], demo_alt_route_[i + 1], 2, "✔ Tối ưu"});
            }
            canvas_->set_probe_visuals(probes);
            if (lbl_demo_live_step_) {
                lbl_demo_live_step_->setText("Bước 4/6: Ngắt nhánh chậm hơn và giữ đường có tốc độ ms & KB/s tối ưu nối tới Client2!");
            }
        } else if (demo_step_ == 4) {
            canvas_->clear_probe_visuals();
            string payload = "Demonstration Payload for PBL2 Simulator: Verifying seamless retransmission and Dijkstra dynamic rerouting upon network failure!";
            handle_start_file_transfer(1, 2, payload, 32);
            handle_advance_ticks(20);
            if (lbl_demo_live_step_) {
                lbl_demo_live_step_->setText("Bước 5/6: Bắt đầu truyền file tin cậy qua tuyến đường mới đã chọn (các gói đang di chuyển trên mạng)...");
            }
        } else {
            canvas_->clear_probe_visuals();
            string payload = "Demonstration Payload for PBL2 Simulator: Verifying seamless retransmission and Dijkstra dynamic rerouting upon network failure!";
            handle_start_file_transfer(1, 2, payload, 32);
            handle_advance_ticks(370);
            if (transfer_service_.is_transfer_completed()) {
                append_log_entry(simulation_service_.get_current_tick(), "Demo 1",
                                 "DEMO 1 SUCCESS: File reconstructed with 100% integrity despite failure!",
                                 "Saved to data/experiments/demonstration_recovery.json");
                handle_export_experiment_results("data/experiments/demonstration_recovery.json");
            }
            if (lbl_demo_live_step_) {
                lbl_demo_live_step_->setText("Bước 6/6 (Hoàn tất): Truyền file thành công 100% qua đường thay thế dù Router4 bị ngắt!\n(Có thể bấm '◀ Quay lại' để xem lại các bước trước)");
            }
        }
        if (was_auto && demo_step_ < 5 && demo_timer_) demo_timer_->start(2500);
    } else if (saved_mode == 2) {
        int target_step = max(0, min(step, 3));
        if (demo_timer_) demo_timer_->stop();
        handle_generate_random_topology(6, 2, 0.45, 12345);
        demo_mode_ = 2;
        demo_step_ = target_step;
        if (btn_demo_auto_) btn_demo_auto_->setChecked(was_auto);
        active_source_id_ = 1;
        active_destination_id_ = 2;
        string payload = "Controlled Retry Limit Failure Test Payload";
        handle_start_file_transfer(1, 2, payload, 64);
        handle_advance_ticks(5);

        const dsa::DynamicArray<int>& neighbors = graph_.get_neighbors(2);
        if (demo_step_ == 0) {
            QVector<TopologyCanvas::ProbeEdge> probes;
            for (int i = 0; i < neighbors.size(); ++i) {
                probes.push_back({2, neighbors[i], 0, "Link cuối vào C2"});
            }
            canvas_->set_probe_visuals(probes);
            if (lbl_demo_live_step_) {
                lbl_demo_live_step_->setText("Bước 1/4: Đang truyền file tới Client2. Client2 chỉ nối với mạng qua 2 đường cuối này.\n(Nhấn 'Tiếp tục ▶' để xem quá trình ngắt kết nối)");
            }
        } else if (demo_step_ == 1) {
            QVector<TopologyCanvas::ProbeEdge> probes;
            for (int i = 0; i < neighbors.size(); ++i) {
                simulation_service_.fail_link_manually(2, neighbors[i]);
                probes.push_back({2, neighbors[i], 1, "✖ Đứt link cuối"});
            }
            canvas_->set_probe_visuals(probes);
            update_route_display();
            if (lbl_demo_live_step_) {
                lbl_demo_live_step_->setText("Bước 2/4: Ngắt cả 2 link cuối nối vào Client2 ➔ Đích bị cô lập hoàn toàn, không còn đường vòng nào.");
            }
        } else if (demo_step_ == 2) {
            for (int i = 0; i < neighbors.size(); ++i) {
                simulation_service_.fail_link_manually(2, neighbors[i]);
            }
            canvas_->clear_probe_visuals();
            update_route_display();
            handle_advance_ticks(120);
            if (lbl_demo_live_step_) {
                lbl_demo_live_step_->setText("Bước 3/4: Gói tin gửi đi không nhận được ACK ➔ Timeout và tự động gửi lại (Retry)...");
            }
        } else {
            for (int i = 0; i < neighbors.size(); ++i) {
                simulation_service_.fail_link_manually(2, neighbors[i]);
            }
            canvas_->clear_probe_visuals();
            update_route_display();
            handle_advance_ticks(360);
            if (transfer_service_.is_transfer_failed()) {
                append_log_entry(simulation_service_.get_current_tick(), "Demo 2",
                                 "DEMO 2 SUCCESS: Transfer correctly aborted after exceeding maximum retry limit (3 attempts)!",
                                 "Evidence preserved in statistics");
                handle_export_experiment_results("data/experiments/demonstration_failure.json");
            }
            if (lbl_demo_live_step_) {
                lbl_demo_live_step_->setText("Bước 4/4 (Hoàn tất): Đã thử lại tối đa 3 lần thất bại ➔ Phiên truyền tự hủy an toàn.\n(Có thể bấm '◀ Quay lại' để xem lại các bước trước)");
            }
        }
        if (was_auto && demo_step_ < 3 && demo_timer_) demo_timer_->start(2500);
    }
    update_demo_nav_buttons();
}

void MainWindow::start_guided_transfer(int src_id, int dst_id, const string& payload, int chunk_size) {
    if (xfer_timer_) xfer_timer_->stop();
    if (btn_xfer_auto_) btn_xfer_auto_->setChecked(false);
    xfer_src_id_ = src_id;
    xfer_dst_id_ = dst_id;
    xfer_payload_ = payload;
    xfer_chunk_size_ = chunk_size;
    xfer_step_active_ = true;
    xfer_step_ = 0;
    apply_xfer_step(0);
}

void MainWindow::update_xfer_nav_buttons() {
    if (!btn_xfer_prev_ || !btn_xfer_next_ || !btn_xfer_auto_) return;
    if (!xfer_step_active_) {
        btn_xfer_prev_->setEnabled(false);
        btn_xfer_next_->setEnabled(false);
        btn_xfer_auto_->setEnabled(false);
        btn_xfer_auto_->setChecked(false);
        btn_xfer_auto_->setText("⏯ Tự động");
        return;
    }
    btn_xfer_prev_->setEnabled(xfer_step_ > 0);
    btn_xfer_next_->setEnabled(xfer_step_ < 4);
    btn_xfer_auto_->setEnabled(xfer_step_ < 4);
    if (xfer_step_ >= 4 && xfer_timer_) {
        xfer_timer_->stop();
        btn_xfer_auto_->setChecked(false);
    }
    btn_xfer_auto_->setText(btn_xfer_auto_->isChecked() ? "⏸ Dừng tự động" : "⏯ Tự động");
}

void MainWindow::on_xfer_prev_step() {
    if (!xfer_step_active_ || xfer_step_ <= 0) return;
    if (xfer_timer_) xfer_timer_->stop();
    if (btn_xfer_auto_) btn_xfer_auto_->setChecked(false);
    apply_xfer_step(xfer_step_ - 1);
}

void MainWindow::on_xfer_next_step() {
    if (!xfer_step_active_ || xfer_step_ >= 4) return;
    apply_xfer_step(xfer_step_ + 1);
}

void MainWindow::on_xfer_toggle_auto() {
    if (!xfer_timer_ || !btn_xfer_auto_) return;
    if (btn_xfer_auto_->isChecked() && xfer_step_active_) {
        btn_xfer_auto_->setText("⏸ Dừng tự động");
        if (xfer_step_ < 4) {
            apply_xfer_step(xfer_step_ + 1);
        }
        if (btn_xfer_auto_->isChecked() && xfer_step_ < 4) {
            xfer_timer_->start(2500);
        }
    } else {
        xfer_timer_->stop();
        btn_xfer_auto_->setText("⏯ Tự động");
    }
}

void MainWindow::on_xfer_timer_tick() {
    if (!xfer_step_active_) {
        if (xfer_timer_) xfer_timer_->stop();
        return;
    }
    if (xfer_step_ < 4) {
        apply_xfer_step(xfer_step_ + 1);
    } else {
        if (xfer_timer_) xfer_timer_->stop();
        if (btn_xfer_auto_) btn_xfer_auto_->setChecked(false);
        update_xfer_nav_buttons();
    }
}

void MainWindow::apply_xfer_step(int step) {
    int target_step = max(0, min(step, 4));
    bool was_auto = (btn_xfer_auto_ && btn_xfer_auto_->isChecked());
    if (xfer_timer_) xfer_timer_->stop();

    // Reset simulation clock/packets cleanly while keeping current graph topology intact
    handle_reset_simulation();
    xfer_step_active_ = true;
    xfer_step_ = target_step;
    if (btn_xfer_auto_) btn_xfer_auto_->setChecked(was_auto);

    handle_start_file_transfer(xfer_src_id_, xfer_dst_id_, xfer_payload_, xfer_chunk_size_);
    services::RoutingService::Route r = routing_service_.find_route(graph_, xfer_src_id_, xfer_dst_id_);

    if (xfer_step_ == 0) {
        update_transfer_display();
        if (lbl_transfer_live_step_) {
            lbl_transfer_live_step_->setText(
                QString("Bước 1/5 (Cắt Chunk & Checksum): Chia dữ liệu thành %1 mảnh. Quan sát bảng bên dưới: State = PENDING, kèm kích thước & mã Checksum.")
                .arg(transfer_chunks_.size()));
        }
    } else if (xfer_step_ == 1) {
        QVector<TopologyCanvas::ProbeEdge> probes;
        QString route_names;
        for (int i = 0; i < r.path.size(); ++i) {
            model::Node* n = graph_.get_node(r.path[i]);
            route_names += n ? QString::fromStdString(n->get_name()) : QString::number(r.path[i]);
            if (i < r.path.size() - 1) {
                route_names += " ➔ ";
                probes.push_back({r.path[i], r.path[i + 1], 2, "✔ Tuyến tối ưu"});
            }
        }
        canvas_->set_probe_visuals(probes);
        update_transfer_display();
        if (lbl_transfer_live_step_) {
            if (r.found) {
                lbl_transfer_live_step_->setText(
                    QString("Bước 2/5 (Định tuyến Dijkstra): Chọn đường chi phí thấp nhất: %1 (Cost = %2).")
                    .arg(route_names).arg(r.total_cost, 0, 'f', 2));
            } else {
                lbl_transfer_live_step_->setText("Bước 2/5 (Định tuyến Dijkstra): Không tìm thấy đường đi tới đích (Destination Unreachable).");
            }
        }
    } else if (xfer_step_ == 2) {
        canvas_->clear_probe_visuals();
        handle_advance_ticks(8);
        if (lbl_transfer_live_step_) {
            lbl_transfer_live_step_->setText(
                "Bước 3/5 (Đang gửi DATA): Nguồn đẩy các Chunk lên mạng. Quan sát gói tin di chuyển trên sơ đồ và bảng chuyển sang IN_FLIGHT.");
        }
    } else if (xfer_step_ == 3) {
        canvas_->clear_probe_visuals();
        for (int t = 0; t < 80; ++t) {
            handle_advance_ticks(1);
            if (transfer_service_.get_statistics().acknowledged_count > 0 ||
                transfer_service_.is_transfer_completed() ||
                transfer_service_.is_transfer_failed()) {
                break;
            }
        }
        if (lbl_transfer_live_step_) {
            lbl_transfer_live_step_->setText(
                "Bước 4/5 (Kiểm tra Checksum & Gửi ACK): Client đích kiểm chứng dữ liệu từng Chunk, gửi ACK phản hồi về nguồn.");
        }
    } else {
        canvas_->clear_probe_visuals();
        for (int t = 0; t < 400; ++t) {
            if (transfer_service_.is_transfer_completed() || transfer_service_.is_transfer_failed()) {
                break;
            }
            handle_advance_ticks(2);
        }
        if (lbl_transfer_live_step_) {
            if (transfer_service_.is_transfer_completed()) {
                lbl_transfer_live_step_->setText(
                    "Bước 5/5 (Hoàn tất 100%): Đã nhận đủ ACK cho mọi Chunk, file được ghép lại toàn vẹn tại đích! (Bấm '◀ Quay lại' để xem lại)");
            } else {
                lbl_transfer_live_step_->setText(
                    "Bước 5/5 (Kết thúc): Quá trình truyền dừng do mất kết nối hoặc vượt giới hạn 3 lần thử lại.");
            }
        }
    }

    if (was_auto && xfer_step_ < 4 && xfer_timer_) {
        xfer_timer_->start(2500);
    }
    update_xfer_nav_buttons();
}

}
}
