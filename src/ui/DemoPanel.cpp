#include "ui/DemoPanel.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QVBoxLayout>

namespace pbl {
namespace ui {

DemoPanel::DemoPanel(QWidget* parent) : QWidget(parent) {
    QVBoxLayout* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    QScrollArea* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    QWidget* content = new QWidget(scroll);
    QVBoxLayout* layout = new QVBoxLayout(content);

    QGroupBox* demo_box = new QGroupBox("End-to-End Demonstrations", content);
    QVBoxLayout* d_layout = new QVBoxLayout(demo_box);

    QPushButton* btn_probe = new QPushButton("Send Hop-by-Hop Route Probe", demo_box);
    btn_probe->setStyleSheet("background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #8b5cf6, stop:1 #7c3aed); color: white; font-weight: bold; border: none; border-radius: 6px; padding: 8px;");
    connect(btn_probe, &QPushButton::clicked, this, &DemoPanel::probeRequested);
    d_layout->addWidget(btn_probe);

    QPushButton* btn_demo1 = new QPushButton("Run Preset Scenario 1: Dynamic Recovery & Rerouting", demo_box);
    btn_demo1->setToolTip("Preset 6-node scenario demonstrating neighbor probing, failure detection (X), and Dijkstra rerouting.");
    btn_demo1->setStyleSheet("background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #3b82f6, stop:1 #1d4ed8); color: white; font-weight: bold; border: none; border-radius: 6px; padding: 10px; font-size: 12px;");
    connect(btn_demo1, &QPushButton::clicked, this, &DemoPanel::scenario1Requested);
    d_layout->addWidget(btn_demo1);

    QPushButton* btn_demo2 = new QPushButton("Run Preset Scenario 2: Controlled Retry-Limit Failure", demo_box);
    btn_demo2->setToolTip("Preset 6-node scenario severing both last-hop links to Client2 to prove 3-retry abort.");
    btn_demo2->setStyleSheet("background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ef4444, stop:1 #b91c1c); color: white; font-weight: bold; border: none; border-radius: 6px; padding: 10px; font-size: 12px;");
    connect(btn_demo2, &QPushButton::clicked, this, &DemoPanel::scenario2Requested);
    d_layout->addWidget(btn_demo2);

    lbl_demo_live_step_ = new QLabel(
        "Ready for step-by-step demonstration. Click Preset Scenario 1 or Scenario 2 to begin.",
        demo_box);
    lbl_demo_live_step_->setWordWrap(true);
    lbl_demo_live_step_->setStyleSheet("padding: 10px; background-color: #0f172a; border: 1px solid #38bdf8; border-left: 4px solid #38bdf8; border-radius: 6px; color: #e2e8f0; font-size: 12px; font-weight: bold;");
    d_layout->addWidget(lbl_demo_live_step_);

    QHBoxLayout* nav_layout = new QHBoxLayout();
    btn_demo_prev_ = new QPushButton("< Prev", demo_box);
    btn_demo_prev_->setEnabled(false);
    btn_demo_prev_->setStyleSheet("background-color: #1e293b; color: #e2e8f0; border: 1px solid #475569; border-radius: 6px; padding: 7px 10px; font-weight: bold;");
    connect(btn_demo_prev_, &QPushButton::clicked, this, &DemoPanel::prevStepRequested);

    btn_demo_next_ = new QPushButton("Next >", demo_box);
    btn_demo_next_->setEnabled(false);
    btn_demo_next_->setStyleSheet("background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #10b981, stop:1 #059669); color: white; border: none; border-radius: 6px; padding: 7px 12px; font-weight: bold;");
    connect(btn_demo_next_, &QPushButton::clicked, this, &DemoPanel::nextStepRequested);

    btn_demo_auto_ = new QPushButton("Auto Play", demo_box);
    btn_demo_auto_->setCheckable(true);
    btn_demo_auto_->setEnabled(false);
    btn_demo_auto_->setToolTip("Toggle automatic step progression every 2.5 seconds.");
    btn_demo_auto_->setStyleSheet(
        "QPushButton { background-color: #1e293b; color: #38bdf8; border: 1px solid #38bdf8; border-radius: 6px; padding: 7px 10px; font-weight: bold; }"
        "QPushButton:checked { background-color: #0284c7; color: #ffffff; border: 1px solid #7dd3fc; }");
    connect(btn_demo_auto_, &QPushButton::clicked, this, &DemoPanel::toggleAutoRequested);

    nav_layout->addWidget(btn_demo_prev_);
    nav_layout->addWidget(btn_demo_next_);
    nav_layout->addWidget(btn_demo_auto_);
    d_layout->addLayout(nav_layout);

    QLabel* lbl_demo_desc = new QLabel(
        "<b>Preset Scenario 1 (Step-by-Step Recovery &amp; Rerouting):</b><br>"
        "1. Client1 probes adjacent routers (Router4 is OFFLINE and marked with a red ✖, so traffic shifts to Router3).<br>"
        "2. At Router3, outgoing links are compared by latency (ms) and bandwidth (KB/s); slower alternatives are discarded and the lowest-cost path to Client2 is chosen.<br>"
        "3. Reliable multi-chunk file transfer completes with 100% integrity over the alternate path.<br><br>"
        "<b>Preset Scenario 2 (Why Sever Both Last-Hop Links to Client2?):</b><br>"
        "• Client2 connects to the mesh through <b>two last-hop links</b>. Failing only one link or an intermediate router still leaves an alternate Dijkstra path.<br>"
        "• Severing both last-hop links isolates Client2 completely (Destination Unreachable), causing ACK timeouts until the max retry limit (3 attempts) aborts the transfer cleanly.",
        demo_box);
    lbl_demo_desc->setWordWrap(true);
    lbl_demo_desc->setStyleSheet("padding: 10px; background-color: #0b0f19; border: 1px solid #1f293d; border-left: 4px solid #8b5cf6; border-radius: 6px; color: #cbd5e1; line-height: 1.5; font-size: 11px;");
    d_layout->addWidget(lbl_demo_desc);

    layout->addWidget(demo_box);
    layout->addStretch();

    scroll->setWidget(content);
    outer->addWidget(scroll);
}

void DemoPanel::set_live_step_text(const QString& text) {
    lbl_demo_live_step_->setText(text);
}

void DemoPanel::update_nav_buttons(int demo_mode, int step, int max_step, bool auto_running) {
    if (demo_mode == 0) {
        btn_demo_prev_->setEnabled(false);
        btn_demo_next_->setEnabled(false);
        btn_demo_auto_->setEnabled(false);
        btn_demo_auto_->setChecked(false);
        btn_demo_auto_->setText("Auto Play");
        return;
    }
    btn_demo_prev_->setEnabled(step > 0);
    btn_demo_next_->setEnabled(step < max_step);
    btn_demo_auto_->setEnabled(step < max_step);
    if (step >= max_step) {
        btn_demo_auto_->setChecked(false);
    } else {
        btn_demo_auto_->setChecked(auto_running);
    }
    btn_demo_auto_->setText(btn_demo_auto_->isChecked() ? "Pause Auto" : "Auto Play");
}

void DemoPanel::set_auto_checked(bool checked) {
    btn_demo_auto_->setChecked(checked);
    btn_demo_auto_->setText(checked ? "Pause Auto" : "Auto Play");
}

bool DemoPanel::is_auto_checked() const {
    return btn_demo_auto_->isChecked();
}

}  // namespace ui
}  // namespace pbl
