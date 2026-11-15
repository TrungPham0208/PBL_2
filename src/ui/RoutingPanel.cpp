#include "ui/RoutingPanel.h"

#include <algorithm>
#include <limits>
#include <map>
#include <queue>
#include <utility>
#include <vector>

#include <QColor>
#include <QFont>
#include <QGroupBox>
#include <QHeaderView>
#include <QScrollArea>
#include <QVBoxLayout>

namespace pbl {
namespace ui {

RoutingPanel::RoutingPanel(QWidget* parent) : QWidget(parent) {
    QVBoxLayout* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    QScrollArea* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    QWidget* content = new QWidget(scroll);
    QVBoxLayout* layout = new QVBoxLayout(content);

    // 1. Shortest Path & Per-Hop Cost Breakdown
    QGroupBox* route_box = new QGroupBox("Current Lowest-Cost Shortest Path", content);
    QVBoxLayout* r_box_layout = new QVBoxLayout(route_box);

    lbl_active_route_ = new QLabel("Select Source & Destination clients to compute route.", route_box);
    lbl_active_route_->setWordWrap(true);
    lbl_active_route_->setStyleSheet("font-weight: bold; color: #34d399; font-size: 13px; padding: 8px; background-color: #0b0f19; border: 1px solid #1f293d; border-radius: 6px;");

    lbl_route_cost_ = new QLabel("Cost: N/A", route_box);
    lbl_route_cost_->setStyleSheet("color: #38bdf8; font-weight: bold; padding: 4px 8px;");

    lbl_route_breakdown_ = new QLabel(
        "Dijkstra Formula: Cost = Latency(ms) + (100 / Bandwidth KB/s) + (2 × Loss% × 100)",
        route_box);
    lbl_route_breakdown_->setWordWrap(true);
    lbl_route_breakdown_->setStyleSheet("padding: 8px; background-color: #0b0f19; border: 1px solid #1f293d; border-left: 3px solid #38bdf8; border-radius: 6px; color: #cbd5e1; font-size: 11px;");

    r_box_layout->addWidget(lbl_active_route_);
    r_box_layout->addWidget(lbl_route_cost_);
    r_box_layout->addWidget(lbl_route_breakdown_);
    layout->addWidget(route_box);

    // 2. Dijkstra State Table (dist[] & prev[] from Source)
    QGroupBox* dijkstra_box = new QGroupBox("Dijkstra Algorithm State (dist[] & prev[] from Source)", content);
    QVBoxLayout* d_layout = new QVBoxLayout(dijkstra_box);
    table_dijkstra_state_ = new QTableWidget(dijkstra_box);
    table_dijkstra_state_->setColumnCount(4);
    table_dijkstra_state_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table_dijkstra_state_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table_dijkstra_state_->setHorizontalHeaderLabels({"Node", "Status", "dist[] (Cost)", "prev[]"});
    table_dijkstra_state_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    d_layout->addWidget(table_dijkstra_state_);
    layout->addWidget(dijkstra_box);

    // 3. Link Queue Occupancy & Cost Table
    QGroupBox* queue_box = new QGroupBox("Link Queue Occupancy", content);
    QVBoxLayout* q_layout = new QVBoxLayout(queue_box);
    table_queues_ = new QTableWidget(queue_box);
    table_queues_->setColumnCount(5);
    table_queues_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table_queues_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table_queues_->setHorizontalHeaderLabels({"Link", "Dijkstra Cost", "Queue Depth", "Capacity", "Status"});
    table_queues_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    q_layout->addWidget(table_queues_);
    layout->addWidget(queue_box);

    layout->addStretch();
    scroll->setWidget(content);
    outer->addWidget(scroll);
}

void RoutingPanel::fit_table_height(QTableWidget* table, int min_height) {
    table->resizeRowsToContents();
    int total_h = table->horizontalHeader()->height() + 6;
    for (int r = 0; r < table->rowCount(); ++r) {
        total_h += table->rowHeight(r);
    }
    table->setFixedHeight(std::max(min_height, total_h));
}

void RoutingPanel::update_route_display(const model::NetworkGraph& graph,
                                        const services::RoutingService& routing_service,
                                        int source_id,
                                        int destination_id,
                                        const services::RoutingService::Route& r) {
    if (source_id == -1 || destination_id == -1) {
        lbl_active_route_->setText("Select Source & Destination clients to compute route.");
        lbl_route_cost_->setText("Cost: N/A");
        lbl_route_breakdown_->setText("Dijkstra Formula: Cost = Latency(ms) + (100 / Bandwidth KB/s) + (2 × Loss% × 100)");
        table_dijkstra_state_->setRowCount(0);
        fit_table_height(table_dijkstra_state_, 80);
        return;
    }

    if (!r.found || r.path.size() < 2) {
        lbl_active_route_->setText("<span style='color:#f87171; font-weight:bold;'>NO ROUTE AVAILABLE (Destination Unreachable)</span>");
        lbl_route_cost_->setText("Cost: INFINITY");
        lbl_route_breakdown_->setText("No valid active path exists between Source and Destination (intermediate nodes or links are OFFLINE).");
    } else {
        QString path_str;
        QString breakdown_str =
            "<b>Per-Hop Dijkstra Cost Breakdown:</b><br>"
            "<i>Hop Cost = Latency(ms) + (100 / Bandwidth KB/s) + (2 × Loss% × 100)</i><br>";
        double sum_check = 0.0;
        for (int i = 0; i < r.path.size(); ++i) {
            const model::Node* n = graph.get_node(r.path[i]);
            QString name = n ? QString::fromStdString(n->get_name()) : QString::number(r.path[i]);
            path_str += QString("<span style='color:#38bdf8; font-weight:bold;'>%1</span>").arg(name);
            if (i < r.path.size() - 1) {
                path_str += " <span style='color:#10b981;'>➔</span> ";
                const model::Node* next_n = graph.get_node(r.path[i + 1]);
                QString next_name = next_n ? QString::fromStdString(next_n->get_name()) : QString::number(r.path[i + 1]);
                const model::Link* lnk = graph.get_link(r.path[i], r.path[i + 1]);
                if (lnk) {
                    double hop_cost = routing_service.calculate_link_cost(*lnk);
                    sum_check += hop_cost;
                    double bw_term = (lnk->get_bandwidth() > 0)
                                         ? (routing_service.get_reference_bandwidth() / lnk->get_bandwidth())
                                         : 0.0;
                    double loss_term = routing_service.get_loss_weight() * (lnk->get_loss_probability() * 100.0);
                    breakdown_str += QString("• <b>%1 ➔ %2</b>: %3 ms + (100/%4) [%5] + loss [%6] = <b style='color:#34d399;'>%7</b><br>")
                                         .arg(name, next_name)
                                         .arg(lnk->get_latency())
                                         .arg(lnk->get_bandwidth())
                                         .arg(bw_term, 0, 'f', 2)
                                         .arg(loss_term, 0, 'f', 2)
                                         .arg(hop_cost, 0, 'f', 2);
                }
            }
        }
        breakdown_str += QString("<b>Total Path Cost: %1</b>").arg(sum_check, 0, 'f', 2);
        lbl_active_route_->setText(path_str);
        lbl_route_cost_->setText(QString("Dijkstra Metric Cost: <b style='color:#38bdf8;'>%1</b>").arg(r.total_cost, 0, 'f', 2));
        lbl_route_breakdown_->setText(breakdown_str);
    }

    update_dijkstra_state_table(graph, routing_service, source_id);
}

void RoutingPanel::update_dijkstra_state_table(const model::NetworkGraph& graph,
                                               const services::RoutingService& routing_service,
                                               int source_id) {
    const dsa::DynamicArray<int>& node_ids = graph.get_node_ids();
    std::vector<int> sorted_ids;
    sorted_ids.reserve(node_ids.size());
    for (int i = 0; i < node_ids.size(); ++i) {
        sorted_ids.push_back(node_ids[i]);
    }
    std::sort(sorted_ids.begin(), sorted_ids.end());

    const double kInf = std::numeric_limits<double>::infinity();
    std::map<int, double> dist;
    std::map<int, int> prev;
    std::map<int, bool> visited;
    for (int id : sorted_ids) {
        dist[id] = kInf;
        prev[id] = -1;
        visited[id] = false;
    }

    const model::Node* src_node = graph.get_node(source_id);
    if (src_node != nullptr && src_node->is_active() && dist.count(source_id)) {
        dist[source_id] = 0.0;
        using QueueEntry = std::pair<double, int>;
        std::priority_queue<QueueEntry, std::vector<QueueEntry>, std::greater<QueueEntry>> pq;
        pq.push({0.0, source_id});

        while (!pq.empty()) {
            auto [d, u] = pq.top();
            pq.pop();
            if (visited[u]) continue;
            visited[u] = true;

            const dsa::DynamicArray<int>& neighbors = graph.get_neighbors(u);
            for (int i = 0; i < neighbors.size(); ++i) {
                int v = neighbors[i];
                const model::Node* vn = graph.get_node(v);
                const model::Link* lnk = graph.get_link(u, v);
                if (!vn || !vn->is_active() || !lnk || lnk->is_failed()) continue;
                double alt = d + routing_service.calculate_link_cost(*lnk);
                if (alt < dist[v]) {
                    dist[v] = alt;
                    prev[v] = u;
                    pq.push({alt, v});
                }
            }
        }
    }

    table_dijkstra_state_->setRowCount(static_cast<int>(sorted_ids.size()));
    for (int row = 0; row < static_cast<int>(sorted_ids.size()); ++row) {
        int id = sorted_ids[row];
        const model::Node* n = graph.get_node(id);
        QString node_label = n ? QString::fromStdString(n->get_name()) : QString::number(id);

        bool is_active = (n && n->is_active());
        bool is_vis = visited[id];
        QString status_str = !is_active ? "OFFLINE" : (is_vis ? "VISITED" : "UNREACHABLE");
        QColor status_col = !is_active ? QColor(248, 113, 113) : (is_vis ? QColor(52, 211, 153) : QColor(251, 191, 36));
        QString dist_str = std::isfinite(dist[id]) ? QString::number(dist[id], 'f', 2) : "INF";
        QString prev_str = (id == source_id) ? "SOURCE"
                         : (prev[id] != -1 ? QString::number(prev[id]) : "-");

        auto set_cell = [&](int col, const QString& text, const QColor& color, bool bold = false) {
            QTableWidgetItem* item = table_dijkstra_state_->item(row, col);
            if (!item) {
                item = new QTableWidgetItem();
                if (bold) {
                    QFont f = item->font();
                    f.setBold(true);
                    item->setFont(f);
                }
                table_dijkstra_state_->setItem(row, col, item);
            }
            item->setText(text);
            item->setForeground(color);
        };

        set_cell(0, node_label, QColor(241, 245, 249));
        set_cell(1, status_str, status_col, true);
        set_cell(2, dist_str, QColor(56, 189, 248));
        set_cell(3, prev_str, QColor(203, 213, 225));
    }

    fit_table_height(table_dijkstra_state_, 100);
}

void RoutingPanel::update_queue_table(const model::NetworkGraph& graph,
                                      const services::RoutingService& routing_service) {
    const dsa::DynamicArray<model::Link*>& links = graph.get_all_links();
    if (table_queues_->rowCount() != links.size()) {
        table_queues_->setRowCount(links.size());
    }

    for (int i = 0; i < links.size(); ++i) {
        model::Link* l = links[i];
        if (l == nullptr) continue;
        QString name = QString("%1 <-> %2").arg(l->get_first_node_id()).arg(l->get_second_node_id());
        double cost = routing_service.calculate_link_cost(*l);

        QString status_str = l->is_failed() ? "FAILED" : (l->queue_full() ? "FULL" : "NORMAL");
        QColor status_col = l->is_failed() ? QColor(248, 113, 113)
                                           : (l->queue_full() ? QColor(251, 191, 36) : QColor(52, 211, 153));

        auto set_cell = [&](int col, const QString& text, const QColor& color, bool bold = false) {
            QTableWidgetItem* item = table_queues_->item(i, col);
            if (!item) {
                item = new QTableWidgetItem();
                if (bold) {
                    QFont f = item->font();
                    f.setBold(true);
                    item->setFont(f);
                }
                table_queues_->setItem(i, col, item);
            }
            item->setText(text);
            item->setForeground(color);
        };

        set_cell(0, name, QColor(241, 245, 249));
        set_cell(1, QString::number(cost, 'f', 2), QColor(56, 189, 248));
        set_cell(2, QString::number(l->queue_size()), QColor(241, 245, 249));
        set_cell(3, QString::number(l->queue_capacity()), QColor(241, 245, 249));
        set_cell(4, status_str, status_col, true);
    }

    fit_table_height(table_queues_, 120);
}

}  // namespace ui
}  // namespace pbl
