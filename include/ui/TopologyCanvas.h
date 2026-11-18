#pragma once

#include <map>
#include <string>
#include <utility>
#include <vector>

#ifdef emit
#pragma push_macro("emit")
#undef emit
#define PBL_RESTORE_EMIT_TC
#endif
#include "model/NetworkGraph.h"
#include "model/Node.h"
#include "model/Link.h"
#include "pbl/dsa/DynamicArray.h"
#include "services/RoutingService.h"
#include "services/SimulationService.h"
#ifdef PBL_RESTORE_EMIT_TC
#pragma pop_macro("emit")
#undef PBL_RESTORE_EMIT_TC
#endif

#include <QContextMenuEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPointF>
#include <QPushButton>
#include <QResizeEvent>
#include <QTimer>
#include <QVector>
#include <QWheelEvent>
#include <QWidget>

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
    std::pair<int, int> get_selected_link() const;

    void update_layout();
    void reset_layout();
    void reset_view();
    void set_node_position(int node_id, double x, double y);

    // Visual-only step-by-step route exploration overlay for Demonstrations
    struct ProbeEdge {
        int u;
        int v;
        int status; // 0: probing/comparing, 1: failed (X), 2: selected, 3: slower/rejected
        QString label;
    };
    void set_probe_visuals(const QVector<ProbeEdge>& edges);
    void clear_probe_visuals();

signals:
    void node_selected(int node_id);
    void link_selected(int node_a, int node_b);
    void canvas_clicked();
    void toggle_node_failure_requested(int node_id);
    void connect_link_requested(int node_a, int node_b);
    void delete_node_requested(int node_id);
    void toggle_link_failure_requested(int node_a, int node_b);
    void edit_link_params_requested(int node_a, int node_b);
    void inject_link_loss_requested(int node_a, int node_b);
    void inject_link_corruption_requested(int node_a, int node_b);
    void delete_link_requested(int node_a, int node_b);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    model::NetworkGraph* graph_;
    services::RoutingService* routing_;
    services::SimulationService* simulation_;

    dsa::DynamicArray<int> active_route_;
    int selected_node_id_;
    int selected_link_a_;
    int selected_link_b_;

    // Node canvas positions (node_id -> QPointF)
    std::map<int, QPointF> node_positions_;

    // Mouse drag & pan/zoom tracking
    int dragging_node_id_;
    QPointF drag_offset_;
    bool user_dragged_;
    double zoom_factor_;
    QPointF pan_offset_;
    bool is_panning_;
    bool pan_did_move_;
    QPointF pan_start_pos_;
    QPointF pan_last_pos_;
    QPushButton* btn_fit_view_;

    QPointF screen_to_world(const QPointF& screen_pos) const;
    void position_overlay_controls();

    // Visual-only probe exploration edges
    QVector<ProbeEdge> probe_edges_;

    // Interactive Hover Tracking
    int hovered_node_id_;
    int hovered_link_a_;
    int hovered_link_b_;

    // Animation timer for pulsing glow and packet motion
    QTimer* anim_timer_;
    int anim_frame_;

    bool is_node_on_active_route(int node_id) const;
    bool is_link_on_active_route(int node_a, int node_b) const;
    int find_node_at(const QPointF& pos, double radius = 24.0) const;
    pair<int, int> find_link_at(const QPointF& pos, double threshold = 10.0) const;

    void draw_grid(QPainter& painter);
    void draw_links(QPainter& painter);
    void draw_probe_visuals(QPainter& painter);
    void draw_packets(QPainter& painter);
    void draw_nodes(QPainter& painter);
    void draw_hud(QPainter& painter);
};

}
}
