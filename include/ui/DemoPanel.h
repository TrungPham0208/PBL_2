#pragma once

#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QWidget>

namespace pbl {
namespace ui {

/**
 * @brief Tab 5 panel: Route Probe and Preset End-to-End Demonstrations with step-by-step controls.
 */
class DemoPanel : public QWidget {
    Q_OBJECT

public:
    explicit DemoPanel(QWidget* parent = nullptr);

    void set_live_step_text(const QString& text);
    void update_nav_buttons(int demo_mode, int step, int max_step, bool auto_running);
    void set_auto_checked(bool checked);
    bool is_auto_checked() const;

signals:
    void probeRequested();
    void scenario1Requested();
    void scenario2Requested();
    void prevStepRequested();
    void nextStepRequested();
    void toggleAutoRequested();

private:
    QLabel* lbl_demo_live_step_;
    QPushButton* btn_demo_prev_;
    QPushButton* btn_demo_next_;
    QPushButton* btn_demo_auto_;
};

}  // namespace ui
}  // namespace pbl
