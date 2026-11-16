#pragma once

#include <string>
#include <utility>
#include <vector>

#ifdef emit
#pragma push_macro("emit")
#undef emit
#define PBL_RESTORE_EMIT_TP
#endif
#include "pbl/dsa/DynamicArray.h"
#include "services/TransferService.h"
#ifdef PBL_RESTORE_EMIT_TP
#pragma pop_macro("emit")
#undef PBL_RESTORE_EMIT_TP
#endif

#include <QComboBox>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTextEdit>
#include <QWidget>

namespace pbl {
namespace ui {

/**
 * @brief Tab 2 panel: Reliable file transfer setup, guided stepper, and chunk progress tracking.
 */
class TransferPanel : public QWidget {
    Q_OBJECT

public:
    explicit TransferPanel(QWidget* parent = nullptr);

    void set_clients(const std::vector<std::pair<int, std::string>>& clients, int preferred_src_id = -1, int preferred_dst_id = -1);
    int chunk_size() const;
    int selected_src_id() const;
    int selected_dst_id() const;
    std::string payload_text() const;

    void reset_display();
    void set_transfer_status_in_progress();
    void populate_chunks(const dsa::DynamicArray<services::TransferService::Chunk>& chunks);
    void update_progress(const services::TransferService& transfer_service,
                         const dsa::DynamicArray<services::TransferService::Chunk>& chunks);

    void set_guided_step_text(const QString& text);
    void update_nav_buttons(bool active, int step, int max_step, bool auto_running);
    void set_auto_checked(bool checked);
    bool is_auto_checked() const;

signals:
    void startTransferRequested(int src_id, int dst_id, const std::string& payload, int chunk_size);
    void prevStepRequested();
    void nextStepRequested();
    void toggleAutoRequested();

private:
    QComboBox* combo_src_client_;
    QComboBox* combo_dst_client_;
    QSpinBox* spin_chunk_size_;
    QTextEdit* edit_payload_;
    QPushButton* btn_start_transfer_;

    QLabel* lbl_transfer_live_step_;
    QPushButton* btn_xfer_prev_;
    QPushButton* btn_xfer_next_;
    QPushButton* btn_xfer_auto_;

    QLabel* lbl_transfer_status_;
    QProgressBar* progress_transfer_;
    QTableWidget* table_chunks_;
};

}  // namespace ui
}  // namespace pbl
