#pragma once

#include <string>

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QWidget>

namespace pbl {
namespace ui {

/**
 * @brief Tab 1 panel: Reproducible topology generator, validation, and manual node/link editing.
 */
class TopologyPanel : public QWidget {
    Q_OBJECT

public:
    explicit TopologyPanel(QWidget* parent = nullptr);

    int node_count() const;
    int client_count() const;
    double density() const;
    int seed() const;
    void set_validation_status(bool is_valid, const std::string& message = "");
    void set_undo_enabled(bool enabled);

signals:
    void generateRequested(int node_count, int client_count, double density, int seed);
    void validateRequested();
    void addNodeRequested(const std::string& name, const std::string& type);
    void addLinkRequested(int u, int v, int bandwidth_kbps, int latency_ms, double loss_prob);
    void deleteSelectedRequested();
    void undoRequested();

private:
    QSpinBox* spin_node_count_;
    QSpinBox* spin_client_count_;
    QDoubleSpinBox* spin_density_;
    QSpinBox* spin_seed_;
    QPushButton* btn_generate_;
    QPushButton* btn_validate_;
    QLabel* lbl_validation_status_;

    QLineEdit* edit_node_name_;
    QComboBox* combo_node_type_;
    QPushButton* btn_add_node_;
    QSpinBox* spin_link_u_;
    QSpinBox* spin_link_v_;
    QSpinBox* spin_link_bw_;
    QSpinBox* spin_link_lat_;
    QDoubleSpinBox* spin_link_loss_;
    QPushButton* btn_add_link_;
    QPushButton* btn_delete_selected_;
    QPushButton* btn_undo_;
};

}  // namespace ui
}  // namespace pbl
