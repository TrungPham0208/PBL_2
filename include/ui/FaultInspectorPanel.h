#pragma once

#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QString>
#include <QWidget>

namespace pbl {
namespace ui {

/**
 * @brief Tab 3 panel: Selected Node/Link inspector, link parameter editor, and manual fault injection controls.
 */
class FaultInspectorPanel : public QWidget {
    Q_OBJECT

public:
    explicit FaultInspectorPanel(QWidget* parent = nullptr);

    void set_selected_info_html(const QString& html);
    void set_link_params_editor(bool enabled, int latency_ms, int bandwidth_kbps);
    void reset_selection_prompt();

signals:
    void toggleFailureRequested();
    void injectLossRequested();
    void injectCorruptionRequested();
    void linkParamsUpdateRequested(int latency_ms, int bandwidth_kbps);

private:
    QLabel* lbl_selected_info_;
    QSpinBox* spin_link_latency_;
    QSpinBox* spin_link_bandwidth_;
    QPushButton* btn_apply_link_params_;
    QPushButton* btn_toggle_fail_;
    QPushButton* btn_inject_loss_;
    QPushButton* btn_inject_corruption_;
};

}  // namespace ui
}  // namespace pbl
