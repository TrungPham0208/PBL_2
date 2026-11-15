#include "ui/TransferPanel.h"

#include <QColor>
#include <QFont>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QScrollArea>
#include <QVBoxLayout>

namespace pbl {
namespace ui {

TransferPanel::TransferPanel(QWidget* parent) : QWidget(parent) {
    QVBoxLayout* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    QScrollArea* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    QWidget* content = new QWidget(scroll);
    QVBoxLayout* layout = new QVBoxLayout(content);

    // Setup Box
    QGroupBox* setup_box = new QGroupBox("Reliable File Transfer Setup", content);
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
        emit startTransferRequested(src_id, dst_id, edit_payload_->toPlainText().toStdString(), spin_chunk_size_->value());
    });
    setup_grid->addWidget(btn_start_transfer_, 4, 0, 1, 2);

    lbl_transfer_live_step_ = new QLabel(
        "Click 'Start Reliable File Transfer' to step through Chunking, Dijkstra Routing, DATA Transmission, and ACK Verification.",
        setup_box);
    lbl_transfer_live_step_->setWordWrap(true);
    lbl_transfer_live_step_->setStyleSheet("padding: 8px; background-color: #0f172a; border: 1px solid #38bdf8; border-left: 4px solid #38bdf8; border-radius: 6px; color: #e2e8f0; font-size: 11px; font-weight: bold;");
    setup_grid->addWidget(lbl_transfer_live_step_, 5, 0, 1, 2);

    QHBoxLayout* nav_layout = new QHBoxLayout();
    btn_xfer_prev_ = new QPushButton("< Prev", setup_box);
    btn_xfer_prev_->setEnabled(false);
    btn_xfer_prev_->setStyleSheet("background-color: #1e293b; color: #e2e8f0; border: 1px solid #475569; border-radius: 6px; padding: 6px 10px; font-weight: bold;");
    connect(btn_xfer_prev_, &QPushButton::clicked, this, &TransferPanel::prevStepRequested);

    btn_xfer_next_ = new QPushButton("Next >", setup_box);
    btn_xfer_next_->setEnabled(false);
    btn_xfer_next_->setStyleSheet("background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #10b981, stop:1 #059669); color: white; border: none; border-radius: 6px; padding: 6px 12px; font-weight: bold;");
    connect(btn_xfer_next_, &QPushButton::clicked, this, &TransferPanel::nextStepRequested);

    btn_xfer_auto_ = new QPushButton("Auto Play", setup_box);
    btn_xfer_auto_->setCheckable(true);
    btn_xfer_auto_->setEnabled(false);
    btn_xfer_auto_->setToolTip("Toggle automatic step progression every 2.5 seconds.");
    btn_xfer_auto_->setStyleSheet(
        "QPushButton { background-color: #1e293b; color: #38bdf8; border: 1px solid #38bdf8; border-radius: 6px; padding: 6px 10px; font-weight: bold; }"
        "QPushButton:checked { background-color: #0284c7; color: #ffffff; border: 1px solid #7dd3fc; }");
    connect(btn_xfer_auto_, &QPushButton::clicked, this, &TransferPanel::toggleAutoRequested);

    nav_layout->addWidget(btn_xfer_prev_);
    nav_layout->addWidget(btn_xfer_next_);
    nav_layout->addWidget(btn_xfer_auto_);
    setup_grid->addLayout(nav_layout, 6, 0, 1, 2);

    layout->addWidget(setup_box);

    // Progress Box
    QGroupBox* progress_box = new QGroupBox("Transfer Progress • Chunks", content);
    QVBoxLayout* prog_layout = new QVBoxLayout(progress_box);

    QLabel* lbl_chunks_guide = new QLabel(
        "<b>Reliable Transfer Chunks Table:</b> Payload is split into sequenced chunks and verified end-to-end:<br>"
        "• <b>Seq #:</b> Chunk sequence index (0, 1, 2...). &nbsp; "
        "• <b>State:</b> <i>UNSENT</i>, <i>IN_FLIGHT</i>, <i>ACKNOWLEDGED</i>, <i>TIMED_OUT</i>, or <i>FAILED</i>.<br>"
        "• <b>Attempts:</b> Transmission attempts used (max 3 before abort). &nbsp; "
        "• <b>Size &amp; Checksum:</b> Byte length and XOR checksum for corruption detection.",
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
    table_chunks_->setMinimumHeight(150);
    table_chunks_->setHorizontalHeaderLabels({"Seq #", "State", "Attempts", "Size", "Checksum"});
    table_chunks_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    prog_layout->addWidget(table_chunks_);

    layout->addWidget(progress_box);
    layout->addStretch();

    scroll->setWidget(content);
    outer->addWidget(scroll);
}

void TransferPanel::set_clients(const std::vector<std::pair<int, std::string>>& clients, int preferred_src_id, int preferred_dst_id) {
    combo_src_client_->clear();
    combo_dst_client_->clear();
    for (const auto& entry : clients) {
        QString label = QString("Client %1 (%2)").arg(entry.first).arg(QString::fromStdString(entry.second));
        combo_src_client_->addItem(label, entry.first);
        combo_dst_client_->addItem(label, entry.first);
    }
    if (preferred_src_id != -1) {
        int idx = combo_src_client_->findData(preferred_src_id);
        if (idx >= 0) combo_src_client_->setCurrentIndex(idx);
    }
    if (preferred_dst_id != -1) {
        int idx = combo_dst_client_->findData(preferred_dst_id);
        if (idx >= 0) combo_dst_client_->setCurrentIndex(idx);
    } else if (combo_dst_client_->count() > 1) {
        combo_dst_client_->setCurrentIndex(combo_dst_client_->count() - 1);
    }
}

int TransferPanel::chunk_size() const {
    return spin_chunk_size_->value();
}

int TransferPanel::selected_src_id() const {
    return combo_src_client_->count() > 0 ? combo_src_client_->currentData().toInt() : -1;
}

int TransferPanel::selected_dst_id() const {
    return combo_dst_client_->count() > 0 ? combo_dst_client_->currentData().toInt() : -1;
}

void TransferPanel::reset_display() {
    lbl_transfer_status_->setText("Transfer Status: IDLE");
    lbl_transfer_status_->setStyleSheet("background-color: rgba(100, 116, 139, 0.2); color: #94a3b8; border: 1px solid #64748b; border-radius: 12px; padding: 4px 12px; font-weight: bold; font-size: 11px;");
    progress_transfer_->setValue(0);
    table_chunks_->setRowCount(0);
}

void TransferPanel::set_transfer_status_in_progress() {
    lbl_transfer_status_->setText("Transfer Status: IN_PROGRESS");
    lbl_transfer_status_->setStyleSheet("background-color: rgba(59, 130, 246, 0.2); color: #60a5fa; border: 1px solid #3b82f6; border-radius: 12px; padding: 4px 12px; font-weight: bold; font-size: 11px;");
    progress_transfer_->setValue(0);
}

void TransferPanel::populate_chunks(const dsa::DynamicArray<services::TransferService::Chunk>& chunks) {
    table_chunks_->setRowCount(chunks.size());
    for (int i = 0; i < chunks.size(); ++i) {
        QTableWidgetItem* c0 = table_chunks_->item(i, 0);
        if (!c0) {
            c0 = new QTableWidgetItem();
            table_chunks_->setItem(i, 0, c0);
        }
        c0->setText(QString::number(chunks[i].sequence_number));

        QTableWidgetItem* c1 = table_chunks_->item(i, 1);
        if (!c1) {
            c1 = new QTableWidgetItem();
            table_chunks_->setItem(i, 1, c1);
        }
        c1->setText("UNSENT");
        c1->setForeground(QColor(148, 163, 184));

        QTableWidgetItem* c2 = table_chunks_->item(i, 2);
        if (!c2) {
            c2 = new QTableWidgetItem();
            table_chunks_->setItem(i, 2, c2);
        }
        c2->setText("0");
        c2->setForeground(QColor(241, 245, 249));

        QTableWidgetItem* c3 = table_chunks_->item(i, 3);
        if (!c3) {
            c3 = new QTableWidgetItem();
            table_chunks_->setItem(i, 3, c3);
        }
        c3->setText(QString::number(chunks[i].size) + " B");

        QTableWidgetItem* c4 = table_chunks_->item(i, 4);
        if (!c4) {
            c4 = new QTableWidgetItem();
            table_chunks_->setItem(i, 4, c4);
        }
        c4->setText(QString::number(chunks[i].checksum));
    }
}

void TransferPanel::update_progress(const services::TransferService& transfer_service,
                                    const dsa::DynamicArray<services::TransferService::Chunk>& chunks) {
    services::TransferService::TransferStatistics stats = transfer_service.get_statistics();
    int total = chunks.size();
    int acks = stats.acknowledged_count;

    if (total > 0) {
        int pct = (acks * 100) / total;
        progress_transfer_->setValue(pct);
    }

    if (transfer_service.is_transfer_completed()) {
        lbl_transfer_status_->setText("Transfer Status: COMPLETED");
        lbl_transfer_status_->setStyleSheet("background-color: rgba(16, 185, 129, 0.2); color: #34d399; border: 1px solid #10b981; border-radius: 12px; padding: 4px 12px; font-weight: bold; font-size: 11px;");
    } else if (transfer_service.is_transfer_failed()) {
        lbl_transfer_status_->setText("Transfer Status: FAILED (Retry Limit Exceeded)");
        lbl_transfer_status_->setStyleSheet("background-color: rgba(239, 68, 68, 0.2); color: #f87171; border: 1px solid #ef4444; border-radius: 12px; padding: 4px 12px; font-weight: bold; font-size: 11px;");
    }

    if (table_chunks_->rowCount() != chunks.size()) {
        populate_chunks(chunks);
    }

    for (int i = 0; i < chunks.size(); ++i) {
        services::TransferService::ChunkProgress prog = transfer_service.get_chunk_progress(i);
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

        QTableWidgetItem* item_state = table_chunks_->item(i, 1);
        if (!item_state) {
            item_state = new QTableWidgetItem();
            QFont f = item_state->font();
            f.setBold(true);
            item_state->setFont(f);
            table_chunks_->setItem(i, 1, item_state);
        }
        item_state->setText(state_str);
        item_state->setForeground(state_color);

        QTableWidgetItem* item_attempts = table_chunks_->item(i, 2);
        if (!item_attempts) {
            item_attempts = new QTableWidgetItem();
            item_attempts->setForeground(QColor(241, 245, 249));
            table_chunks_->setItem(i, 2, item_attempts);
        }
        item_attempts->setText(QString::number(prog.attempts));
    }
}

void TransferPanel::set_guided_step_text(const QString& text) {
    lbl_transfer_live_step_->setText(text);
}

void TransferPanel::update_nav_buttons(bool active, int step, int max_step, bool auto_running) {
    if (!active) {
        btn_xfer_prev_->setEnabled(false);
        btn_xfer_next_->setEnabled(false);
        btn_xfer_auto_->setEnabled(false);
        btn_xfer_auto_->setChecked(false);
        btn_xfer_auto_->setText("Auto Play");
        return;
    }
    btn_xfer_prev_->setEnabled(step > 0);
    btn_xfer_next_->setEnabled(step < max_step);
    btn_xfer_auto_->setEnabled(step < max_step);
    if (step >= max_step) {
        btn_xfer_auto_->setChecked(false);
    } else {
        btn_xfer_auto_->setChecked(auto_running);
    }
    btn_xfer_auto_->setText(btn_xfer_auto_->isChecked() ? "Pause Auto" : "Auto Play");
}

void TransferPanel::set_auto_checked(bool checked) {
    btn_xfer_auto_->setChecked(checked);
    btn_xfer_auto_->setText(checked ? "Pause Auto" : "Auto Play");
}

bool TransferPanel::is_auto_checked() const {
    return btn_xfer_auto_->isChecked();
}

}  // namespace ui
}  // namespace pbl
