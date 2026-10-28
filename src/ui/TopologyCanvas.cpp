#include <bits/stdc++.h>

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QFont>
#include <QPen>
#include <QBrush>
#include <QColor>

#include "ui/TopologyCanvas.h"

using namespace std;

namespace pbl {
namespace ui {

// Milestone 11: Supporting/boilerplate — Qt dashboard and topology controls
TopologyCanvas::TopologyCanvas(QWidget* parent) : QWidget(parent) {
    graph_ = nullptr;
    routing_ = nullptr;
    simulation_ = nullptr;

    selected_node_id_ = -1;
    selected_link_a_ = -1;
    selected_link_b_ = -1;
    dragging_node_id_ = -1;

    setMinimumSize(400, 350);
    setMouseTracking(true);
}

void TopologyCanvas::set_network_graph(model::NetworkGraph* graph) {
    graph_ = graph;
    update_layout();
    update();
}

void TopologyCanvas::set_routing_service(services::RoutingService* routing) {
    routing_ = routing;
    update();
}

void TopologyCanvas::set_simulation_service(services::SimulationService* simulation) {
    simulation_ = simulation;
    update();
}

void TopologyCanvas::set_active_route(const dsa::DynamicArray<int>& route) {
    active_route_.clear();
    for (int i = 0; i < route.size(); ++i) {
        active_route_.push_back(route[i]);
    }
    update();
}

void TopologyCanvas::set_selected_node(int node_id) {
    selected_node_id_ = node_id;
    selected_link_a_ = -1;
    selected_link_b_ = -1;
    update();
}

void TopologyCanvas::set_selected_link(int node_a, int node_b) {
    selected_node_id_ = -1;
    selected_link_a_ = node_a;
    selected_link_b_ = node_b;
    update();
}

int TopologyCanvas::get_selected_node() const {
    return selected_node_id_;
}

pair<int, int> TopologyCanvas::get_selected_link() const {
    return make_pair(selected_link_a_, selected_link_b_);
}

void TopologyCanvas::set_node_position(int node_id, double x, double y) {
    node_positions_[node_id] = QPointF(x, y);
    update();
}

void TopologyCanvas::update_layout() {
    if (graph_ == nullptr) {
        return;
    }

    const dsa::DynamicArray<int>& node_ids = graph_->get_node_ids();
    int count = node_ids.size();
    if (count == 0) {
        return;
    }

    double cx = width() > 100 ? width() / 2.0 : 250.0;
    double cy = height() > 100 ? height() / 2.0 : 200.0;
    double radius = min(cx, cy) * 0.70;
    if (radius < 80.0) {
        radius = 120.0;
    }

    double angle_step = (2.0 * M_PI) / max(1, count);
    for (int i = 0; i < count; ++i) {
        int id = node_ids[i];
        if (node_positions_.find(id) == node_positions_.end()) {
            double angle = i * angle_step - M_PI / 2.0;
            double x = cx + radius * cos(angle);
            double y = cy + radius * sin(angle);
            node_positions_[id] = QPointF(x, y);
        }
    }
}

bool TopologyCanvas::is_node_on_active_route(int node_id) const {
    for (int i = 0; i < active_route_.size(); ++i) {
        if (active_route_[i] == node_id) {
            return true;
        }
    }
    return false;
}

bool TopologyCanvas::is_link_on_active_route(int node_a, int node_b) const {
    if (active_route_.size() < 2) {
        return false;
    }
    for (int i = 0; i < active_route_.size() - 1; ++i) {
        int u = active_route_[i];
        int v = active_route_[i + 1];
        if ((u == node_a && v == node_b) || (u == node_b && v == node_a)) {
            return true;
        }
    }
    return false;
}

int TopologyCanvas::find_node_at(const QPointF& pos, double radius) const {
    for (auto it = node_positions_.begin(); it != node_positions_.end(); ++it) {
        int id = it->first;
        QPointF p = it->second;
        double dx = pos.x() - p.x();
        double dy = pos.y() - p.y();
        if (sqrt(dx * dx + dy * dy) <= radius) {
            return id;
        }
    }
    return -1;
}

void TopologyCanvas::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    // Canvas background
    painter.fillRect(rect(), QColor(248, 249, 250));

    if (graph_ == nullptr) {
        painter.setPen(QColor(140, 145, 155));
        painter.setFont(QFont("Segoe UI", 11));
        painter.drawText(rect(), Qt::AlignCenter, "No Network Topology Loaded\nUse 'Generate Topology' or 'Add Node' to begin.");
        return;
    }

    // Ensure positions exist
    update_layout();

    // 1. Draw Links
    const dsa::DynamicArray<model::Link*>& links = graph_->get_all_links();
    for (int i = 0; i < links.size(); ++i) {
        model::Link* l = links[i];
        if (l == nullptr) continue;

        int u = l->get_first_node_id();
        int v = l->get_second_node_id();
        if (node_positions_.find(u) == node_positions_.end() || node_positions_.find(v) == node_positions_.end()) {
            continue;
        }

        QPointF p1 = node_positions_[u];
        QPointF p2 = node_positions_[v];

        bool is_selected = ((u == selected_link_a_ && v == selected_link_b_) ||
                            (u == selected_link_b_ && v == selected_link_a_));
        bool on_route = is_link_on_active_route(u, v);

        QPen pen;
        if (l->is_failed()) {
            pen.setColor(QColor(220, 53, 69)); // Red for failed
            pen.setWidth(2);
            pen.setStyle(Qt::DashLine);
        } else if (on_route) {
            pen.setColor(QColor(40, 167, 69)); // Bright green for active route
            pen.setWidth(4);
            pen.setStyle(Qt::SolidLine);
        } else if (is_selected) {
            pen.setColor(QColor(255, 193, 7)); // Amber gold for selected
            pen.setWidth(3);
            pen.setStyle(Qt::SolidLine);
        } else {
            pen.setColor(QColor(180, 190, 205)); // Soft slate blue
            pen.setWidth(2);
            pen.setStyle(Qt::SolidLine);
        }

        painter.setPen(pen);
        painter.drawLine(p1, p2);

        // Draw link label (bandwidth, latency, queue) at midpoint
        QPointF mid = (p1 + p2) / 2.0;
        QString label = QString("%1ms | %2KB/s").arg(l->get_latency()).arg(l->get_bandwidth());
        if (l->queue_size() > 0) {
            label += QString(" [Q:%1/%2]").arg(l->queue_size()).arg(l->queue_capacity());
        }

        painter.setFont(QFont("Segoe UI", 8));
        QFontMetrics fm = painter.fontMetrics();
        int tw = fm.horizontalAdvance(label);
        int th = fm.height();
        QRectF bg_rect(mid.x() - tw / 2.0 - 3, mid.y() - th / 2.0 - 2, tw + 6, th + 4);

        painter.fillRect(bg_rect, QColor(255, 255, 255, 210));
        painter.setPen(QColor(100, 105, 115));
        painter.drawRect(bg_rect);
        painter.setPen(l->is_failed() ? QColor(200, 30, 45) : QColor(50, 55, 65));
        painter.drawText(bg_rect, Qt::AlignCenter, label);
    }

    // 2. Draw In-Flight Packets traversing links
    if (simulation_ != nullptr) {
        const dsa::DynamicArray<services::SimulationService::InFlightPacket>& in_flight =
            simulation_->get_in_flight_packets();
        int current_tick = simulation_->get_current_tick();

        for (int i = 0; i < in_flight.size(); ++i) {
            const services::SimulationService::InFlightPacket& ifp = in_flight[i];
            int from_id = ifp.from_node_id;
            int to_id = ifp.to_node_id;

            if (node_positions_.find(from_id) != node_positions_.end() &&
                node_positions_.find(to_id) != node_positions_.end()) {
                QPointF p_from = node_positions_[from_id];
                QPointF p_to = node_positions_[to_id];

                double progress = 0.0;
                int duration = ifp.arrival_tick - ifp.dispatch_tick;
                if (duration > 0) {
                    progress = (double)(current_tick - ifp.dispatch_tick) / (double)duration;
                }
                progress = max(0.0, min(1.0, progress));

                QPointF pkt_pos = p_from + progress * (p_to - p_from);

                QColor pkt_color = QColor(13, 110, 253); // Default blue for DATA
                QString type_str = "D";
                if (ifp.packet.get_type() == model::Packet::Type::ACK) {
                    pkt_color = QColor(25, 135, 84); // Green for ACK
                    type_str = "ACK";
                } else if (ifp.packet.get_type() == model::Packet::Type::ROUTE_PROBE) {
                    pkt_color = QColor(111, 66, 193); // Purple for Probe
                    type_str = "PRB";
                } else if (ifp.packet.get_type() == model::Packet::Type::HEARTBEAT) {
                    pkt_color = QColor(253, 126, 20); // Orange for Heartbeat
                    type_str = "HB";
                }

                painter.setBrush(pkt_color);
                painter.setPen(QPen(Qt::white, 1));
                painter.drawEllipse(pkt_pos, 7.0, 7.0);

                painter.setFont(QFont("Segoe UI", 6, QFont::Bold));
                painter.setPen(Qt::white);
                painter.drawText(QRectF(pkt_pos.x() - 10, pkt_pos.y() - 5, 20, 10), Qt::AlignCenter, type_str);
            }
        }
    }

    // 3. Draw Nodes
    const dsa::DynamicArray<int>& node_ids = graph_->get_node_ids();
    double node_radius = 22.0;

    for (int i = 0; i < node_ids.size(); ++i) {
        int id = node_ids[i];
        if (node_positions_.find(id) == node_positions_.end()) continue;

        model::Node* node = graph_->get_node(id);
        if (node == nullptr) continue;

        QPointF p = node_positions_[id];
        bool is_selected = (id == selected_node_id_);
        bool on_route = is_node_on_active_route(id);
        bool is_client = (node->get_type() == "Client");

        // Outer glow/ring for selected or route nodes
        if (is_selected) {
            painter.setPen(QPen(QColor(255, 193, 7, 180), 6));
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(p, node_radius + 4, node_radius + 4);
        } else if (on_route) {
            painter.setPen(QPen(QColor(40, 167, 69, 140), 5));
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(p, node_radius + 3, node_radius + 3);
        }

        // Node circle fill & border
        QColor node_color;
        if (!node->is_active()) {
            node_color = QColor(220, 53, 69); // Red for failed node
        } else if (is_client) {
            node_color = QColor(32, 201, 151); // Teal / emerald for Client
        } else {
            node_color = QColor(13, 110, 253); // Royal blue for Router
        }

        painter.setBrush(node_color);
        painter.setPen(QPen(QColor(255, 255, 255), 2));
        painter.drawEllipse(p, node_radius, node_radius);

        // Node ID & Type symbol
        painter.setFont(QFont("Segoe UI", 9, QFont::Bold));
        painter.setPen(Qt::white);
        QString short_label = (is_client ? "C" : "R") + QString::number(id);
        painter.drawText(QRectF(p.x() - node_radius, p.y() - node_radius, node_radius * 2, node_radius * 2),
                         Qt::AlignCenter, short_label);

        // Node Name label below circle
        painter.setFont(QFont("Segoe UI", 8));
        painter.setPen(QColor(33, 37, 41));
        QString full_name = QString::fromStdString(node->get_name());
        painter.drawText(QRectF(p.x() - 50, p.y() + node_radius + 2, 100, 16),
                         Qt::AlignCenter, full_name);

        if (!node->is_active()) {
            painter.setFont(QFont("Segoe UI", 7, QFont::Bold));
            painter.setPen(QColor(220, 53, 69));
            painter.drawText(QRectF(p.x() - 50, p.y() + node_radius + 16, 100, 14),
                             Qt::AlignCenter, "FAILED");
        }
    }
}

void TopologyCanvas::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        QPointF pos = event->position();
        int clicked_node = find_node_at(pos);

        if (clicked_node != -1) {
            selected_node_id_ = clicked_node;
            selected_link_a_ = -1;
            selected_link_b_ = -1;
            dragging_node_id_ = clicked_node;
            drag_offset_ = node_positions_[clicked_node] - pos;
            emit node_selected(clicked_node);
            update();
            return;
        }

        // Check if a link was clicked
        if (graph_ != nullptr) {
            const dsa::DynamicArray<model::Link*>& links = graph_->get_all_links();
            for (int i = 0; i < links.size(); ++i) {
                model::Link* l = links[i];
                if (l == nullptr) continue;
                int u = l->get_first_node_id();
                int v = l->get_second_node_id();
                if (node_positions_.find(u) == node_positions_.end() || node_positions_.find(v) == node_positions_.end()) {
                    continue;
                }
                QPointF p1 = node_positions_[u];
                QPointF p2 = node_positions_[v];

                // Distance from point to segment
                double line_len = sqrt(pow(p2.x() - p1.x(), 2) + pow(p2.y() - p1.y(), 2));
                if (line_len < 1e-4) continue;

                double u_param = ((pos.x() - p1.x()) * (p2.x() - p1.x()) + (pos.y() - p1.y()) * (p2.y() - p1.y())) / (line_len * line_len);
                if (u_param >= 0.0 && u_param <= 1.0) {
                    QPointF proj = p1 + u_param * (p2 - p1);
                    double dist = sqrt(pow(pos.x() - proj.x(), 2) + pow(pos.y() - proj.y(), 2));
                    if (dist <= 8.0) {
                        selected_node_id_ = -1;
                        selected_link_a_ = u;
                        selected_link_b_ = v;
                        emit link_selected(u, v);
                        update();
                        return;
                    }
                }
            }
        }

        // Clicked on empty canvas
        selected_node_id_ = -1;
        selected_link_a_ = -1;
        selected_link_b_ = -1;
        emit canvas_clicked();
        update();
    }
}

void TopologyCanvas::mouseMoveEvent(QMouseEvent* event) {
    if (dragging_node_id_ != -1 && (event->buttons() & Qt::LeftButton)) {
        QPointF new_pos = event->position() + drag_offset_;
        // Keep within widget boundaries
        new_pos.setX(max(30.0, min((double)width() - 30.0, new_pos.x())));
        new_pos.setY(max(30.0, min((double)height() - 30.0, new_pos.y())));
        node_positions_[dragging_node_id_] = new_pos;
        update();
    }
}

void TopologyCanvas::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        dragging_node_id_ = -1;
    }
}

}
}
