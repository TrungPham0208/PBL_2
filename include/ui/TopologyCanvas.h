#pragma once

#include <bits/stdc++.h>
#include <QWidget>
#include <QPainter>
#include <QMouseEvent>
#include <QPointF>

#include "model/NetworkGraph.h"
#include "model/Node.h"
#include "model/Link.h"
#include "pbl/dsa/DynamicArray.h"
#include "services/RoutingService.h"
#include "services/SimulationService.h"

using namespace std;

namespace pbl {
namespace ui {

// Milestone 11: Supporting/boilerplate — Qt dashboard and topology controls
// TopologyCanvas provides visual, interactive presentation of the network graph.
// It renders nodes, links, queue occupancy, failure states, active routes, and in-flight packets.
// Invariant: Canvas is strictly a presentation and interaction view.
// It does NOT perform Dijkstra, transfer logic, checksums, or failure detection.
class TopologyCanvas : public QWidget {
    Q_OBJECT

public:
    TopologyCanvas(QWidget* parent = nullptr);
    // Qt parent-child hierarchy automatically reclaims QWidget memory when parent is destroyed.
    // TopologyCanvas does not own raw dynamically allocated unmanaged resources, so no custom destructor is needed.

    void set_network_graph(model::NetworkGraph* graph);
    void set_routing_service(services::RoutingService* routing);
    void set_simulation_service(services::SimulationService* simulation);

    void set_active_route(const dsa::DynamicArray<int>& route);
    void set_selected_node(int node_id);
    void set_selected_link(int node_a, int node_b);

    int get_selected_node() const;
    pair<int, int> get_selected_link() const;

    void update_layout();
    void set_node_position(int node_id, double x, double y);

signals:
    void node_selected(int node_id);
    void link_selected(int node_a, int node_b);
    void canvas_clicked();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    model::NetworkGraph* graph_;
    services::RoutingService* routing_;
    services::SimulationService* simulation_;

    dsa::DynamicArray<int> active_route_;
    int selected_node_id_;
    int selected_link_a_;
    int selected_link_b_;

    // Node canvas positions (node_id -> QPointF)
    map<int, QPointF> node_positions_;

    // Mouse drag tracking
    int dragging_node_id_;
    QPointF drag_offset_;

    bool is_node_on_active_route(int node_id) const;
    bool is_link_on_active_route(int node_a, int node_b) const;
    int find_node_at(const QPointF& pos, double radius = 24.0) const;
};

}
}
