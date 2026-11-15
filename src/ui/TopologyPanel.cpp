#include "ui/TopologyPanel.h"

#include <QGridLayout>
#include <QGroupBox>
#include <QScrollArea>
#include <QVBoxLayout>

namespace pbl {
namespace ui {

TopologyPanel::TopologyPanel(QWidget* parent) : QWidget(parent) {
    QVBoxLayout* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    QScrollArea* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    QWidget* content = new QWidget(scroll);
    QVBoxLayout* layout = new QVBoxLayout(content);

    // Random Topology Generator
    QGroupBox* gen_box = new QGroupBox("Random Topology Generator", content);
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
    spin_density_->setToolTip("Probability of adding redundant mesh links (0.10 = sparse, 1.00 = dense alternate paths).");
    gen_grid->addWidget(spin_density_, 2, 1);

    gen_grid->addWidget(new QLabel("Random Seed:"), 3, 0);
    spin_seed_ = new QSpinBox(gen_box);
    spin_seed_->setRange(1, 9999999);
    spin_seed_->setValue(12345);
    spin_seed_->setToolTip("Deterministic PRNG seed: identical parameters + seed always reproduce the exact same topology.");
    gen_grid->addWidget(spin_seed_, 3, 1);

    QLabel* lbl_gen_hint = new QLabel(
        "<b>Quick Reference:</b><br>"
        "• <b>Connection Density (0.10–1.00):</b> Controls redundant links. Higher density provides more backup paths for Dijkstra rerouting.<br>"
        "• <b>Random Seed:</b> Fixes the random generator so experiments can be reproduced identically.<br>"
        "• <b>Validate Topology:</b> Checks graph connectivity across all clients and detects isolated nodes.",
        gen_box);
    lbl_gen_hint->setWordWrap(true);
    lbl_gen_hint->setStyleSheet("padding: 8px; background-color: #0b0f19; border: 1px solid #1f293d; border-left: 3px solid #38bdf8; border-radius: 6px; color: #cbd5e1; font-size: 11px;");
    gen_grid->addWidget(lbl_gen_hint, 4, 0, 1, 2);

    btn_generate_ = new QPushButton("Generate Reproducible Topology", gen_box);
    btn_generate_->setStyleSheet("background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #3b82f6, stop:1 #2563eb); color: white; font-weight: bold; border: none; border-radius: 6px; padding: 8px;");
    connect(btn_generate_, &QPushButton::clicked, this, [this]() {
        emit generateRequested(spin_node_count_->value(),
                               spin_client_count_->value(),
                               spin_density_->value(),
                               spin_seed_->value());
    });
    gen_grid->addWidget(btn_generate_, 5, 0, 1, 2);

    btn_validate_ = new QPushButton("Validate Topology", gen_box);
    connect(btn_validate_, &QPushButton::clicked, this, &TopologyPanel::validateRequested);
    gen_grid->addWidget(btn_validate_, 6, 0, 1, 2);

    lbl_validation_status_ = new QLabel("Status: Ready", gen_box);
    lbl_validation_status_->setStyleSheet("font-weight: bold; color: #94a3b8; padding: 4px;");
    gen_grid->addWidget(lbl_validation_status_, 7, 0, 1, 2);

    layout->addWidget(gen_box);

    // Manual Node / Link Creation
    QGroupBox* manual_box = new QGroupBox("Manual Node / Link Creation", content);
    QGridLayout* manual_grid = new QGridLayout(manual_box);

    manual_grid->addWidget(new QLabel("Node Name:"), 0, 0);
    edit_node_name_ = new QLineEdit(manual_box);
    edit_node_name_->setPlaceholderText("e.g. Router7");
    manual_grid->addWidget(edit_node_name_, 0, 1);

    manual_grid->addWidget(new QLabel("Node Type:"), 1, 0);
    combo_node_type_ = new QComboBox(manual_box);
    combo_node_type_->addItem("Client");
    combo_node_type_->addItem("Router");
    manual_grid->addWidget(combo_node_type_, 1, 1);

    btn_add_node_ = new QPushButton("+ Add Node", manual_box);
    connect(btn_add_node_, &QPushButton::clicked, this, [this]() {
        std::string name = edit_node_name_->text().trimmed().toStdString();
        std::string type = combo_node_type_->currentText().toStdString();
        edit_node_name_->clear();
        emit addNodeRequested(name, type);
    });
    manual_grid->addWidget(btn_add_node_, 2, 0, 1, 2);

    manual_grid->addWidget(new QLabel("Link Node A ID:"), 3, 0);
    spin_link_u_ = new QSpinBox(manual_box);
    spin_link_u_->setRange(1, 1000);
    spin_link_u_->setValue(1);
    manual_grid->addWidget(spin_link_u_, 3, 1);

    manual_grid->addWidget(new QLabel("Link Node B ID:"), 4, 0);
    spin_link_v_ = new QSpinBox(manual_box);
    spin_link_v_->setRange(1, 1000);
    spin_link_v_->setValue(2);
    manual_grid->addWidget(spin_link_v_, 4, 1);

    manual_grid->addWidget(new QLabel("Bandwidth (KB/s):"), 5, 0);
    spin_link_bw_ = new QSpinBox(manual_box);
    spin_link_bw_->setRange(10, 10000);
    spin_link_bw_->setValue(100);
    manual_grid->addWidget(spin_link_bw_, 5, 1);

    manual_grid->addWidget(new QLabel("Latency (ms):"), 6, 0);
    spin_link_lat_ = new QSpinBox(manual_box);
    spin_link_lat_->setRange(1, 1000);
    spin_link_lat_->setValue(10);
    manual_grid->addWidget(spin_link_lat_, 6, 1);

    manual_grid->addWidget(new QLabel("Loss Prob (0.0-1.0):"), 7, 0);
    spin_link_loss_ = new QDoubleSpinBox(manual_box);
    spin_link_loss_->setRange(0.0, 1.0);
    spin_link_loss_->setSingleStep(0.05);
    spin_link_loss_->setValue(0.0);
    manual_grid->addWidget(spin_link_loss_, 7, 1);

    btn_add_link_ = new QPushButton("+ Add Bidirectional Link", manual_box);
    connect(btn_add_link_, &QPushButton::clicked, this, [this]() {
        emit addLinkRequested(spin_link_u_->value(),
                              spin_link_v_->value(),
                              spin_link_bw_->value(),
                              spin_link_lat_->value(),
                              spin_link_loss_->value());
    });
    manual_grid->addWidget(btn_add_link_, 8, 0, 1, 2);

    btn_delete_selected_ = new QPushButton("Delete Selected Node / Link", manual_box);
    btn_delete_selected_->setStyleSheet("background-color: rgba(239, 68, 68, 0.15); color: #f87171; border: 1px solid #ef4444; border-radius: 6px; padding: 6px; font-weight: bold;");
    connect(btn_delete_selected_, &QPushButton::clicked, this, &TopologyPanel::deleteSelectedRequested);
    manual_grid->addWidget(btn_delete_selected_, 9, 0, 1, 2);

    layout->addWidget(manual_box);
    layout->addStretch();

    scroll->setWidget(content);
    outer->addWidget(scroll);
}

int TopologyPanel::node_count() const {
    return spin_node_count_->value();
}

int TopologyPanel::client_count() const {
    return spin_client_count_->value();
}

double TopologyPanel::density() const {
    return spin_density_->value();
}

int TopologyPanel::seed() const {
    return spin_seed_->value();
}

void TopologyPanel::set_validation_status(bool is_valid, const std::string& message) {
    Q_UNUSED(message);
    lbl_validation_status_->setText(is_valid ? "Status: VALID" : "Status: WARNINGS");
    lbl_validation_status_->setStyleSheet(is_valid ? "color: #34d399; font-weight: bold;" : "color: #f87171; font-weight: bold;");
}

}  // namespace ui
}  // namespace pbl
