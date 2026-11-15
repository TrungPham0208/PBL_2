#pragma once

#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QWidget>

namespace pbl {
namespace ui {

/**
 * @brief Tab 3 panel: Selected Node/Link inspector and manual fault injection controls.
 */
class FaultInspectorPanel : public QWidget {
    Q_OBJECT

public:
    explicit FaultInspectorPanel(QWidget* parent = nullptr);

    void set_selected_info_html(const QString& html);
    void reset_selection_prompt();

signals:
    void toggleFailureRequested();
    void injectLossRequested();
    void injectCorruptionRequested();

private:
    QLabel* lbl_selected_info_;
    QPushButton* btn_toggle_fail_;
    QPushButton* btn_inject_loss_;
    QPushButton* btn_inject_corruption_;
};

}  // namespace ui
}  // namespace pbl
