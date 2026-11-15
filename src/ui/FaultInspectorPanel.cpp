#include "ui/FaultInspectorPanel.h"

#include <QGroupBox>
#include <QScrollArea>
#include <QVBoxLayout>

namespace pbl {
namespace ui {

FaultInspectorPanel::FaultInspectorPanel(QWidget* parent) : QWidget(parent) {
    QVBoxLayout* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    QScrollArea* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    QWidget* content = new QWidget(scroll);
    QVBoxLayout* layout = new QVBoxLayout(content);

    QGroupBox* inspector_box = new QGroupBox("Selected Node / Link Inspector", content);
    QVBoxLayout* insp_layout = new QVBoxLayout(inspector_box);

    lbl_selected_info_ = new QLabel(
        "Click any <b>Node (Client / Router)</b> or <b>Link</b> on the canvas to inspect or inject faults.",
        inspector_box);
    lbl_selected_info_->setWordWrap(true);
    lbl_selected_info_->setStyleSheet("padding: 10px; background-color: #0b0f19; border: 1px solid #1f293d; border-left: 4px solid #38bdf8; border-radius: 6px; color: #f1f5f9; line-height: 1.5;");
    insp_layout->addWidget(lbl_selected_info_);

    QLabel* lbl_faults_guide = new QLabel(
        "<b>Interaction &amp; Fault Injection Guide:</b><br>"
        "• <b>Select Target:</b> Left-click a <b>Node</b> (Client/Router) or <b>Link</b> on the canvas (or right-click for quick context actions).<br>"
        "• <b>Toggle Failure / Recovery:</b> Disables (OFFLINE/FAILED) or recovers (ACTIVE) the selected Node or Link so Dijkstra dynamically reroutes.<br>"
        "• <b>Force Drop Next Packet (Link only):</b> Drops the next packet traversing the selected Link to demonstrate ACK timeout &amp; retransmission.<br>"
        "• <b>Force Corrupt Next Packet (Link only):</b> Corrupts the next packet's payload on the selected Link to test XOR checksum rejection.",
        inspector_box);
    lbl_faults_guide->setWordWrap(true);
    lbl_faults_guide->setStyleSheet("padding: 8px; background-color: #0b0f19; border: 1px solid #1f293d; border-left: 3px solid #f59e0b; border-radius: 6px; color: #cbd5e1; font-size: 11px;");
    insp_layout->addWidget(lbl_faults_guide);

    btn_toggle_fail_ = new QPushButton("Toggle Selected Failure / Recovery", inspector_box);
    btn_toggle_fail_->setToolTip("Toggle Active/Failed state of the selected Node or Link.");
    btn_toggle_fail_->setStyleSheet("background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ef4444, stop:1 #dc2626); color: white; font-weight: bold; border: none; border-radius: 6px; padding: 8px;");
    connect(btn_toggle_fail_, &QPushButton::clicked, this, &FaultInspectorPanel::toggleFailureRequested);
    insp_layout->addWidget(btn_toggle_fail_);

    btn_inject_loss_ = new QPushButton("Force Drop Next Packet on Selected Link", inspector_box);
    btn_inject_loss_->setToolTip("Select a Link first: arms a one-shot packet drop on that Link.");
    connect(btn_inject_loss_, &QPushButton::clicked, this, &FaultInspectorPanel::injectLossRequested);
    insp_layout->addWidget(btn_inject_loss_);

    btn_inject_corruption_ = new QPushButton("Force Corrupt Next Packet on Selected Link", inspector_box);
    btn_inject_corruption_->setToolTip("Select a Link first: arms a one-shot payload corruption on that Link.");
    connect(btn_inject_corruption_, &QPushButton::clicked, this, &FaultInspectorPanel::injectCorruptionRequested);
    insp_layout->addWidget(btn_inject_corruption_);

    layout->addWidget(inspector_box);
    layout->addStretch();

    scroll->setWidget(content);
    outer->addWidget(scroll);
}

void FaultInspectorPanel::set_selected_info_html(const QString& html) {
    lbl_selected_info_->setText(html);
}

void FaultInspectorPanel::reset_selection_prompt() {
    lbl_selected_info_->setText("Click any <b>Node (Client / Router)</b> or <b>Link</b> on the canvas to inspect or inject faults.");
}

}  // namespace ui
}  // namespace pbl
