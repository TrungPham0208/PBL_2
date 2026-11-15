#include "ui/TopologyCanvas.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <QBrush>
#include <QColor>
#include <QContextMenuEvent>
#include <QCursor>
#include <QFont>
#include <QFontMetrics>
#include <QLinearGradient>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPushButton>
#include <QRadialGradient>
#include <QWheelEvent>

using namespace std;

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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
    user_dragged_ = false;
    zoom_factor_ = 1.0;
    pan_offset_ = QPointF(0.0, 0.0);
    is_panning_ = false;
    pan_did_move_ = false;

    hovered_node_id_ = -1;
    hovered_link_a_ = -1;
    hovered_link_b_ = -1;

    anim_frame_ = 0;

    setMinimumSize(450, 400);
    setMouseTracking(true);

    btn_fit_view_ = new QPushButton("Fit / Reset View", this);
    btn_fit_view_->setCursor(Qt::PointingHandCursor);
    btn_fit_view_->setToolTip("Reset zoom (1.0x), pan, and auto-arrange nodes");
    btn_fit_view_->setStyleSheet(
        "QPushButton { background-color: rgba(15, 23, 42, 210); color: #38bdf8; "
        "border: 1px solid #334155; border-radius: 6px; padding: 5px 10px; font-size: 11px; font-weight: bold; }"
        "QPushButton:hover { background-color: #1e293b; border-color: #38bdf8; }");
    connect(btn_fit_view_, &QPushButton::clicked, this, &TopologyCanvas::reset_view);
    position_overlay_controls();

    // Smooth animation timer (~30 FPS) for pulsing glow, active route flow, and moving packets
    anim_timer_ = new QTimer(this);
    connect(anim_timer_, &QTimer::timeout, this, [this]() {
        anim_frame_++;
        update();
    });
    anim_timer_->start(33);
}

void TopologyCanvas::set_network_graph(model::NetworkGraph* graph) {
    graph_ = graph;
    node_positions_.clear();
    probe_edges_.clear();
    user_dragged_ = false;
    update_layout();
    update();
}

void TopologyCanvas::reset_layout() {
    node_positions_.clear();
    user_dragged_ = false;
    update_layout();
    update();
}

void TopologyCanvas::set_probe_visuals(const QVector<ProbeEdge>& edges) {
    probe_edges_ = edges;
    update();
}

void TopologyCanvas::clear_probe_visuals() {
    probe_edges_.clear();
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

    double w = width() > 200 ? (double)width() : 740.0;
    double h = height() > 200 ? (double)height() : 520.0;

    // Separate into Clients (placed at left & right ends) and Routers (spread in middle)
    vector<int> clients;
    vector<int> routers;
    for (int i = 0; i < count; ++i) {
        int id = node_ids[i];
        model::Node* n = graph_->get_node(id);
        if (n != nullptr && n->get_type() == "Client") {
            clients.push_back(id);
        } else {
            routers.push_back(id);
        }
    }
    sort(clients.begin(), clients.end());
    sort(routers.begin(), routers.end());

    double left_x = max(70.0, w * 0.11);
    double right_x = min(w - 70.0, w * 0.89);

    // 1. Position Clients on Left and Right ends
    if (clients.size() == 1) {
        if (node_positions_.find(clients[0]) == node_positions_.end()) {
            node_positions_[clients[0]] = QPointF(left_x, h * 0.50);
        }
    } else if (clients.size() == 2) {
        if (node_positions_.find(clients[0]) == node_positions_.end()) {
            node_positions_[clients[0]] = QPointF(left_x, h * 0.50);
        }
        if (node_positions_.find(clients[1]) == node_positions_.end()) {
            node_positions_[clients[1]] = QPointF(right_x, h * 0.50);
        }
    } else if (clients.size() > 2) {
        vector<int> left_c, right_c;
        for (size_t i = 0; i < clients.size(); ++i) {
            if (i % 2 == 0) left_c.push_back(clients[i]);
            else right_c.push_back(clients[i]);
        }
        for (size_t i = 0; i < left_c.size(); ++i) {
            if (node_positions_.find(left_c[i]) == node_positions_.end()) {
                double y = h * ((double)(i + 1) / (double)(left_c.size() + 1));
                node_positions_[left_c[i]] = QPointF(left_x, y);
            }
        }
        for (size_t i = 0; i < right_c.size(); ++i) {
            if (node_positions_.find(right_c[i]) == node_positions_.end()) {
                double y = h * ((double)(i + 1) / (double)(right_c.size() + 1));
                node_positions_[right_c[i]] = QPointF(right_x, y);
            }
        }
    }

    // 2. Position Routers in a spacious staggered middle grid
    int r_count = (int)routers.size();
    if (r_count == 1) {
        if (node_positions_.find(routers[0]) == node_positions_.end()) {
            node_positions_[routers[0]] = QPointF(w * 0.50, h * 0.50);
        }
    } else if (r_count == 2) {
        double ys[2] = {h * 0.28, h * 0.72};
        for (int i = 0; i < 2; ++i) {
            if (node_positions_.find(routers[i]) == node_positions_.end()) {
                node_positions_[routers[i]] = QPointF(w * 0.50, ys[i]);
            }
        }
    } else if (r_count == 3) {
        QPointF pts[3] = {
            QPointF(w * 0.38, h * 0.26),
            QPointF(w * 0.38, h * 0.74),
            QPointF(w * 0.62, h * 0.50)
        };
        for (int i = 0; i < 3; ++i) {
            if (node_positions_.find(routers[i]) == node_positions_.end()) {
                node_positions_[routers[i]] = pts[i];
            }
        }
    } else if (r_count == 4) {
        // Staggered 2x2 diamond-grid so crossing link labels never overlap at exact center
        QPointF pts[4] = {
            QPointF(w * 0.36, h * 0.23),
            QPointF(w * 0.39, h * 0.77),
            QPointF(w * 0.61, h * 0.21),
            QPointF(w * 0.64, h * 0.75)
        };
        for (int i = 0; i < 4; ++i) {
            if (node_positions_.find(routers[i]) == node_positions_.end()) {
                node_positions_[routers[i]] = pts[i];
            }
        }
    } else if (r_count > 4) {
        int cols = max(2, (int)ceil(sqrt((double)r_count)));
        int rows = (int)ceil((double)r_count / cols);
        double mid_left = w * 0.30;
        double mid_right = w * 0.70;
        double top_y = max(70.0, h * 0.16);
        double bot_y = min(h - 70.0, h * 0.84);

        for (int i = 0; i < r_count; ++i) {
            if (node_positions_.find(routers[i]) != node_positions_.end()) continue;
            int col = i % cols;
            int row = i / cols;
            double x = (cols > 1) ? mid_left + (mid_right - mid_left) * ((double)col / (cols - 1)) : w * 0.50;
            double y_stagger = (col % 2 == 1) ? 18.0 : -18.0;
            double y = (rows > 1) ? top_y + (bot_y - top_y) * ((double)row / (rows - 1)) + y_stagger : h * 0.50;
            y = max(50.0, min(h - 50.0, y));
            node_positions_[routers[i]] = QPointF(x, y);
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

pair<int, int> TopologyCanvas::find_link_at(const QPointF& pos, double threshold) const {
    if (graph_ == nullptr) return make_pair(-1, -1);

    const dsa::DynamicArray<model::Link*>& links = graph_->get_all_links();
    for (int i = 0; i < links.size(); ++i) {
        model::Link* l = links[i];
        if (l == nullptr) continue;
        int u = l->get_first_node_id();
        int v = l->get_second_node_id();
        if (node_positions_.find(u) == node_positions_.end() || node_positions_.find(v) == node_positions_.end()) {
            continue;
        }
        QPointF p1 = node_positions_.at(u);
        QPointF p2 = node_positions_.at(v);

        double line_len = sqrt(pow(p2.x() - p1.x(), 2) + pow(p2.y() - p1.y(), 2));
        if (line_len < 1e-4) continue;

        double u_param = ((pos.x() - p1.x()) * (p2.x() - p1.x()) + (pos.y() - p1.y()) * (p2.y() - p1.y())) / (line_len * line_len);
        if (u_param >= 0.0 && u_param <= 1.0) {
            QPointF proj = p1 + u_param * (p2 - p1);
            double dist = sqrt(pow(pos.x() - proj.x(), 2) + pow(pos.y() - proj.y(), 2));
            if (dist <= threshold) {
                return make_pair(u, v);
            }
        }
    }
    return make_pair(-1, -1);
}

void TopologyCanvas::draw_grid(QPainter& painter) {
    // Futuristic Deep Slate Background
    painter.fillRect(rect(), QColor(13, 17, 28));

    // Subtle radial center ambient glow
    double cx = width() / 2.0;
    double cy = height() / 2.0;
    double max_dim = max(cx, cy) * 1.2;
    QRadialGradient ambient(cx, cy, max_dim);
    ambient.setColorAt(0.0, QColor(26, 38, 64, 130));
    ambient.setColorAt(0.7, QColor(16, 24, 40, 60));
    ambient.setColorAt(1.0, QColor(13, 17, 28, 0));
    painter.fillRect(rect(), ambient);

    // Subtle CAD Grid dots (anchored to pan_offset_ for visual movement feedback)
    painter.setPen(QColor(51, 65, 85, 90));
    const int grid_step = 28;
    int ox = static_cast<int>(std::fmod(pan_offset_.x(), static_cast<double>(grid_step)));
    int oy = static_cast<int>(std::fmod(pan_offset_.y(), static_cast<double>(grid_step)));
    if (ox < 0) ox += grid_step;
    if (oy < 0) oy += grid_step;
    for (int x = ox; x < width(); x += grid_step) {
        for (int y = oy; y < height(); y += grid_step) {
            painter.drawPoint(x, y);
        }
    }
}

void TopologyCanvas::draw_links(QPainter& painter) {
    if (graph_ == nullptr) return;

    const dsa::DynamicArray<model::Link*>& links = graph_->get_all_links();

    // Pass 1: Draw lines & neon glowing auras
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
        bool is_hovered = ((u == hovered_link_a_ && v == hovered_link_b_) ||
                           (u == hovered_link_b_ && v == hovered_link_a_));
        bool on_route = is_link_on_active_route(u, v);
        bool is_failed = l->is_failed();

        // 1. Glowing outer aura
        if (is_failed) {
            // Pulsing red glow
            double pulse = (sin(anim_frame_ * 0.15) + 1.0) * 0.5;
            QPen glow_pen(QColor(239, 68, 68, (int)(40 + 50 * pulse)), 8, Qt::SolidLine, Qt::RoundCap);
            painter.setPen(glow_pen);
            painter.drawLine(p1, p2);
        } else if (on_route) {
            // Neon Emerald glow
            double pulse = (sin(anim_frame_ * 0.12) + 1.0) * 0.5;
            QPen glow_pen(QColor(16, 185, 129, (int)(70 + 60 * pulse)), 10, Qt::SolidLine, Qt::RoundCap);
            painter.setPen(glow_pen);
            painter.drawLine(p1, p2);
        } else if (is_selected) {
            // Amber Gold glow
            QPen glow_pen(QColor(245, 158, 11, 100), 8, Qt::SolidLine, Qt::RoundCap);
            painter.setPen(glow_pen);
            painter.drawLine(p1, p2);
        } else if (is_hovered) {
            // Soft Cyan hover aura
            QPen glow_pen(QColor(56, 189, 248, 80), 6, Qt::SolidLine, Qt::RoundCap);
            painter.setPen(glow_pen);
            painter.drawLine(p1, p2);
        }

        // 2. Core cable line
        QPen core_pen;
        if (is_failed) {
            core_pen.setColor(QColor(239, 68, 68));
            core_pen.setWidth(2);
            core_pen.setStyle(Qt::DashLine);
        } else if (on_route) {
            core_pen.setColor(QColor(52, 211, 153)); // Bright Emerald Neon
            core_pen.setWidth(3);
            core_pen.setStyle(Qt::SolidLine);
            core_pen.setCapStyle(Qt::RoundCap);
        } else if (is_selected) {
            core_pen.setColor(QColor(251, 191, 36));
            core_pen.setWidth(2.5);
            core_pen.setStyle(Qt::SolidLine);
        } else if (is_hovered) {
            core_pen.setColor(QColor(56, 189, 248));
            core_pen.setWidth(2.5);
            core_pen.setStyle(Qt::SolidLine);
        } else {
            // Slate blue cable with subtle bandwidth scaling
            int w = 2;
            if (l->get_bandwidth() >= 200) w = 3;
            core_pen.setColor(QColor(71, 85, 105));
            core_pen.setWidth(w);
            core_pen.setStyle(Qt::SolidLine);
        }

        painter.setPen(core_pen);
        painter.drawLine(p1, p2);

        // 3. Dynamic energy flow animation along active route
        if (on_route && !is_failed) {
            QPen flow_pen(QColor(255, 255, 255, 220), 2.5);
            QList<qreal> dashes;
            dashes << 3 << 8;
            flow_pen.setDashPattern(dashes);
            flow_pen.setDashOffset(-anim_frame_ * 1.2);
            painter.setPen(flow_pen);
            painter.drawLine(p1, p2);
        }
    }

    // Pass 2: Draw floating metadata badges on links
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
        // Slight stagger along link to prevent midpoint badge overlap on crossing links
        double t_pos = ((u + v) % 2 == 0) ? 0.44 : 0.56;
        QPointF mid = p1 + t_pos * (p2 - p1);

        bool on_route = is_link_on_active_route(u, v);
        bool is_failed = l->is_failed();

        QString label = QString("%1ms  •  %2KB/s").arg(l->get_latency()).arg(l->get_bandwidth());
        if (is_failed) {
            label = "⚠ LINK OFFLINE";
        } else if (l->queue_size() > 0) {
            label += QString(" [Q:%1/%2]").arg(l->queue_size()).arg(l->queue_capacity());
        }

        painter.setFont(QFont("Segoe UI", 8, QFont::Bold));
        QFontMetrics fm = painter.fontMetrics();
        int tw = fm.horizontalAdvance(label);
        int th = fm.height();
        QRectF bg_rect(mid.x() - tw / 2.0 - 7, mid.y() - th / 2.0 - 4, tw + 14, th + 8);

        // Badge pill styling
        QColor badge_bg = is_failed ? QColor(45, 15, 20, 235)
                                    : (on_route ? QColor(6, 44, 30, 235) : QColor(15, 23, 42, 235));
        QColor badge_border = is_failed ? QColor(239, 68, 68)
                                        : (on_route ? QColor(16, 185, 129) : QColor(51, 65, 85));

        painter.setBrush(badge_bg);
        painter.setPen(QPen(badge_border, 1.2));
        painter.drawRoundedRect(bg_rect, 5.0, 5.0);

        // Queue occupancy mini indicator
        if (!is_failed && l->queue_size() > 0) {
            double q_ratio = (double)l->queue_size() / (double)max(1, l->queue_capacity());
            QRectF q_bar(bg_rect.left() + 2, bg_rect.bottom() - 2.5, (bg_rect.width() - 4) * min(1.0, q_ratio), 2);
            painter.fillRect(q_bar, q_ratio >= 0.8 ? QColor(239, 68, 68) : QColor(245, 158, 11));
        }

        // Text
        painter.setPen(is_failed ? QColor(248, 113, 113)
                                 : (on_route ? QColor(52, 211, 153) : QColor(226, 232, 240)));
        painter.drawText(bg_rect, Qt::AlignCenter, label);
    }
}

void TopologyCanvas::draw_packets(QPainter& painter) {
    if (simulation_ == nullptr) return;

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
            QPointF dir = p_to - p_from;
            double len = sqrt(dir.x() * dir.x() + dir.y() * dir.y());
            QPointF unit_dir = (len > 1e-4) ? (dir / len) : QPointF(0, 0);

            QColor pkt_color = QColor(14, 165, 233); // Vivid Sky Blue for DATA
            QString type_str = "D";
            if (ifp.packet.get_type() == model::Packet::Type::ACK) {
                pkt_color = QColor(16, 185, 129); // Emerald for ACK
                type_str = "ACK";
            } else if (ifp.packet.get_type() == model::Packet::Type::ROUTE_PROBE) {
                pkt_color = QColor(168, 85, 247); // Vibrant Purple for Probe
                type_str = "PRB";
            } else if (ifp.packet.get_type() == model::Packet::Type::HEARTBEAT) {
                pkt_color = QColor(249, 115, 22); // Orange for Heartbeat
                type_str = "HB";
            }

            // 1. Motion Trail (Comet tail behind packet)
            for (int t = 1; t <= 3; ++t) {
                QPointF trail_pos = pkt_pos - unit_dir * (t * 6.0);
                int alpha = 140 / (t * 2);
                QColor trail_col = pkt_color;
                trail_col.setAlpha(alpha);
                painter.setBrush(trail_col);
                painter.setPen(Qt::NoPen);
                painter.drawEllipse(trail_pos, 5.0 - t, 5.0 - t);
            }

            // 2. Glowing Halo
            QRadialGradient pkt_glow(pkt_pos, 14.0);
            QColor glow_col = pkt_color;
            glow_col.setAlpha(180);
            pkt_glow.setColorAt(0.0, glow_col);
            glow_col.setAlpha(0);
            pkt_glow.setColorAt(1.0, glow_col);
            painter.setBrush(pkt_glow);
            painter.setPen(Qt::NoPen);
            painter.drawEllipse(pkt_pos, 14.0, 14.0);

            // 3. Solid Particle Core
            painter.setBrush(pkt_color);
            painter.setPen(QPen(Qt::white, 1.5));
            painter.drawEllipse(pkt_pos, 8.5, 8.5);

            // 4. Packet Type Tag
            painter.setFont(QFont("Segoe UI", 6, QFont::Bold));
            painter.setPen(Qt::white);
            painter.drawText(QRectF(pkt_pos.x() - 12, pkt_pos.y() - 6, 24, 12), Qt::AlignCenter, type_str);
        }
    }
}

void TopologyCanvas::draw_nodes(QPainter& painter) {
    if (graph_ == nullptr) return;

    const dsa::DynamicArray<int>& node_ids = graph_->get_node_ids();
    double node_radius = 24.0;

    for (int i = 0; i < node_ids.size(); ++i) {
        int id = node_ids[i];
        if (node_positions_.find(id) == node_positions_.end()) continue;

        model::Node* node = graph_->get_node(id);
        if (node == nullptr) continue;

        QPointF p = node_positions_[id];
        bool is_selected = (id == selected_node_id_);
        bool is_hovered = (id == hovered_node_id_);
        bool on_route = is_node_on_active_route(id);
        bool is_client = (node->get_type() == "Client");
        bool is_active = node->is_active();

        // 1. Glowing outer halo
        if (!is_active) {
            // Pulsing Red Warning Halo
            double pulse = (sin(anim_frame_ * 0.18) + 1.0) * 0.5;
            painter.setPen(QPen(QColor(239, 68, 68, (int)(70 + 70 * pulse)), 6));
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(p, node_radius + 5, node_radius + 5);
        } else if (is_selected) {
            // Amber Target Ring
            painter.setPen(QPen(QColor(245, 158, 11, 200), 5));
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(p, node_radius + 5, node_radius + 5);
        } else if (on_route) {
            // Active Route Emerald Halo
            double pulse = (sin(anim_frame_ * 0.12) + 1.0) * 0.5;
            painter.setPen(QPen(QColor(16, 185, 129, (int)(80 + 60 * pulse)), 4));
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(p, node_radius + 4, node_radius + 4);
        } else if (is_hovered) {
            // Cyan Hover Glow
            painter.setPen(QPen(QColor(56, 189, 248, 150), 3.5));
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(p, node_radius + 3, node_radius + 3);
        }

        // 2. Node Body Gradient Fill & Border
        QLinearGradient grad(p.x() - node_radius, p.y() - node_radius, p.x() + node_radius, p.y() + node_radius);
        QColor border_color;

        if (!is_active) {
            grad.setColorAt(0.0, QColor(185, 28, 28));
            grad.setColorAt(1.0, QColor(127, 29, 29));
            border_color = QColor(248, 113, 113);
        } else if (is_client) {
            // Emerald to Teal gradient for Clients
            grad.setColorAt(0.0, QColor(16, 185, 129));
            grad.setColorAt(1.0, QColor(15, 118, 110));
            border_color = QColor(94, 234, 212);
        } else {
            // Royal Blue to Indigo gradient for Routers
            grad.setColorAt(0.0, QColor(59, 130, 246));
            grad.setColorAt(1.0, QColor(29, 78, 216));
            border_color = QColor(147, 197, 253);
        }

        painter.setBrush(grad);
        painter.setPen(QPen(border_color, 2.2));
        painter.drawEllipse(p, node_radius, node_radius);

        // 3. Subtle Inner Specular Ring
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(255, 255, 255, 60), 1.0));
        painter.drawEllipse(p, node_radius - 2.5, node_radius - 2.5);

        // 4. Node Glyph / Symbol
        painter.setFont(QFont("Segoe UI", 9, QFont::Bold));
        painter.setPen(Qt::white);
        QString short_label = (is_client ? "💻 C" : "⮂ R") + QString::number(id);
        painter.drawText(QRectF(p.x() - node_radius, p.y() - node_radius, node_radius * 2, node_radius * 2),
                         Qt::AlignCenter, short_label);

        // 5. Node Name Pill Badge below circle
        QString full_name = QString::fromStdString(node->get_name());
        painter.setFont(QFont("Segoe UI", 8, QFont::Bold));
        QFontMetrics fm = painter.fontMetrics();
        int nw = fm.horizontalAdvance(full_name);
        QRectF name_bg(p.x() - nw / 2.0 - 6, p.y() + node_radius + 4, nw + 12, 17);

        painter.setBrush(QColor(15, 23, 42, 230));
        painter.setPen(QPen(QColor(51, 65, 85), 1.0));
        painter.drawRoundedRect(name_bg, 4.0, 4.0);

        painter.setPen(QColor(241, 245, 249));
        painter.drawText(name_bg, Qt::AlignCenter, full_name);

        // 6. Status Warning if Failed
        if (!is_active) {
            QRectF fail_bg(p.x() - 32, p.y() + node_radius + 23, 64, 15);
            painter.setBrush(QColor(239, 68, 68));
            painter.setPen(Qt::NoPen);
            painter.drawRoundedRect(fail_bg, 3.0, 3.0);

            painter.setFont(QFont("Segoe UI", 7, QFont::Bold));
            painter.setPen(Qt::white);
            painter.drawText(fail_bg, Qt::AlignCenter, "OFFLINE");
        }
    }
}

void TopologyCanvas::draw_hud(QPainter& painter) {
    if (graph_ == nullptr) return;

    // Top-left minimal HUD Badge
    int n_count = graph_->get_node_ids().size();
    int l_count = graph_->get_all_links().size();
    int p_count = (simulation_ != nullptr) ? simulation_->get_in_flight_packets().size() : 0;

    QString hud_text = QString("TOPOLOGY  •  %1 Nodes  |  %2 Links  |  %3 In-Flight")
                           .arg(n_count).arg(l_count).arg(p_count);

    painter.setFont(QFont("Segoe UI", 8, QFont::Bold));
    QFontMetrics fm = painter.fontMetrics();
    int hw = fm.horizontalAdvance(hud_text);
    QRectF hud_rect(12, 12, hw + 16, 24);

    painter.setBrush(QColor(15, 23, 42, 210));
    painter.setPen(QPen(QColor(51, 65, 85, 180), 1.0));
    painter.drawRoundedRect(hud_rect, 6.0, 6.0);

    painter.setPen(QColor(148, 163, 184));
    painter.drawText(hud_rect, Qt::AlignCenter, hud_text);

    // Bottom-right Legend
    QString legend_client = "● Client";
    QString legend_router = "● Router";
    QString legend_route = "● Active Path";
    QString legend_failed = "● Failed";

    QRectF legend_rect(width() - 280, height() - 30, 268, 22);
    painter.setBrush(QColor(15, 23, 42, 210));
    painter.setPen(QPen(QColor(51, 65, 85, 180), 1.0));
    painter.drawRoundedRect(legend_rect, 5.0, 5.0);

    painter.setFont(QFont("Segoe UI", 7, QFont::Bold));
    painter.setPen(QColor(52, 211, 153));
    painter.drawText(QRectF(width() - 275, height() - 30, 60, 22), Qt::AlignCenter, legend_client);

    painter.setPen(QColor(96, 165, 250));
    painter.drawText(QRectF(width() - 215, height() - 30, 60, 22), Qt::AlignCenter, legend_router);

    painter.setPen(QColor(52, 211, 153));
    painter.drawText(QRectF(width() - 155, height() - 30, 75, 22), Qt::AlignCenter, legend_route);

    painter.setPen(QColor(248, 113, 113));
    painter.drawText(QRectF(width() - 75, height() - 30, 60, 22), Qt::AlignCenter, legend_failed);
}

void TopologyCanvas::draw_probe_visuals(QPainter& painter) {
    if (probe_edges_.isEmpty()) return;

    for (int i = 0; i < probe_edges_.size(); ++i) {
        const ProbeEdge& pe = probe_edges_[i];
        if (node_positions_.find(pe.u) == node_positions_.end() ||
            node_positions_.find(pe.v) == node_positions_.end()) {
            continue;
        }

        QPointF p1 = node_positions_[pe.u];
        QPointF p2 = node_positions_[pe.v];
        QPointF mid = (p1 + p2) / 2.0;

        if (pe.status == 0) {
            // Probing / Comparing: animated cyan/amber dashed beam
            double pulse = (sin(anim_frame_ * 0.25) + 1.0) * 0.5;
            QPen glow_pen(QColor(56, 189, 248, (int)(90 + 70 * pulse)), 10, Qt::SolidLine, Qt::RoundCap);
            painter.setPen(glow_pen);
            painter.drawLine(p1, p2);

            QPen beam_pen(QColor(251, 191, 36), 3.5);
            QList<qreal> dashes;
            dashes << 6 << 5;
            beam_pen.setDashPattern(dashes);
            beam_pen.setDashOffset(-anim_frame_ * 1.5);
            painter.setPen(beam_pen);
            painter.drawLine(p1, p2);
        } else if (pe.status == 1) {
            // Failed / Blocked (Red line + big X mark)
            double pulse = (sin(anim_frame_ * 0.22) + 1.0) * 0.5;
            QPen glow_pen(QColor(239, 68, 68, (int)(110 + 70 * pulse)), 11, Qt::SolidLine, Qt::RoundCap);
            painter.setPen(glow_pen);
            painter.drawLine(p1, p2);

            QPen red_pen(QColor(248, 113, 113), 3.5, Qt::DashLine);
            painter.setPen(red_pen);
            painter.drawLine(p1, p2);

            // Big X circle at midpoint
            painter.setBrush(QColor(220, 38, 38));
            painter.setPen(QPen(Qt::white, 2.0));
            painter.drawEllipse(mid, 13.0, 13.0);

            painter.setPen(QPen(Qt::white, 2.8, Qt::SolidLine, Qt::RoundCap));
            painter.drawLine(mid + QPointF(-5, -5), mid + QPointF(5, 5));
            painter.drawLine(mid + QPointF(5, -5), mid + QPointF(-5, 5));
        } else if (pe.status == 2) {
            // Selected / Confirmed hop (Bright emerald beam)
            QPen glow_pen(QColor(16, 185, 129, 130), 12, Qt::SolidLine, Qt::RoundCap);
            painter.setPen(glow_pen);
            painter.drawLine(p1, p2);

            QPen core_pen(QColor(52, 211, 153), 4.0, Qt::SolidLine, Qt::RoundCap);
            painter.setPen(core_pen);
            painter.drawLine(p1, p2);

            QPen flow_pen(QColor(255, 255, 255, 230), 2.5);
            QList<qreal> dashes;
            dashes << 4 << 7;
            flow_pen.setDashPattern(dashes);
            flow_pen.setDashOffset(-anim_frame_ * 1.5);
            painter.setPen(flow_pen);
            painter.drawLine(p1, p2);
        } else if (pe.status == 3) {
            // Slower / Dropped alternative
            QPen slow_pen(QColor(249, 115, 22, 180), 2.5, Qt::DotLine);
            painter.setPen(slow_pen);
            painter.drawLine(p1, p2);
        }

        // Optional status pill label offset slightly above midpoint
        if (!pe.label.isEmpty()) {
            painter.setFont(QFont("Segoe UI", 8, QFont::Bold));
            QFontMetrics fm = painter.fontMetrics();
            int tw = fm.horizontalAdvance(pe.label);
            int th = fm.height();
            QPointF label_pos = mid + QPointF(0, (pe.status == 1) ? -22.0 : -16.0);
            QRectF lrect(label_pos.x() - tw / 2.0 - 7, label_pos.y() - th / 2.0 - 3, tw + 14, th + 6);

            QColor bg = (pe.status == 1) ? QColor(127, 29, 29, 240)
                      : (pe.status == 2) ? QColor(6, 78, 59, 240)
                      : (pe.status == 3) ? QColor(67, 20, 7, 235)
                                         : QColor(30, 41, 59, 240);
            QColor bd = (pe.status == 1) ? QColor(248, 113, 113)
                      : (pe.status == 2) ? QColor(52, 211, 153)
                      : (pe.status == 3) ? QColor(251, 146, 60)
                                         : QColor(251, 191, 36);
            painter.setBrush(bg);
            painter.setPen(QPen(bd, 1.3));
            painter.drawRoundedRect(lrect, 5.0, 5.0);
            painter.setPen(Qt::white);
            painter.drawText(lrect, Qt::AlignCenter, pe.label);
        }
    }
}

void TopologyCanvas::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    // 1. Grid & Backdrop
    draw_grid(painter);

    if (graph_ == nullptr || graph_->node_count() == 0) {
        painter.setPen(QColor(148, 163, 184));
        painter.setFont(QFont("Segoe UI", 12, QFont::Bold));
        painter.drawText(rect(), Qt::AlignCenter, "No Network Topology Loaded\nUse 'Generate Topology' or 'Add Node' to begin.");
        return;
    }

    // Ensure node positions exist
    update_layout();

    painter.save();
    painter.translate(pan_offset_);
    painter.scale(zoom_factor_, zoom_factor_);

    // 2. Links
    draw_links(painter);

    // 2b. Step-by-step route probe/exploration visuals
    draw_probe_visuals(painter);

    // 3. In-Flight Packets
    draw_packets(painter);

    // 4. Nodes
    draw_nodes(painter);

    painter.restore();

    // 5. Canvas HUD & Legend (screen space)
    draw_hud(painter);
}

QPointF TopologyCanvas::screen_to_world(const QPointF& screen_pos) const {
    return (screen_pos - pan_offset_) / zoom_factor_;
}

void TopologyCanvas::position_overlay_controls() {
    if (btn_fit_view_) {
        btn_fit_view_->adjustSize();
        btn_fit_view_->move(max(12, width() - btn_fit_view_->width() - 12), 12);
    }
}

void TopologyCanvas::reset_view() {
    zoom_factor_ = 1.0;
    pan_offset_ = QPointF(0.0, 0.0);
    reset_layout();
}

void TopologyCanvas::mousePressEvent(QMouseEvent* event) {
    QPointF world_pos = screen_to_world(event->position());

    if (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton) {
        is_panning_ = true;
        pan_did_move_ = false;
        pan_start_pos_ = event->position();
        pan_last_pos_ = event->position();
        if (event->button() == Qt::MiddleButton) {
            setCursor(Qt::ClosedHandCursor);
        }
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton) {
        int clicked_node = find_node_at(world_pos);

        if (clicked_node != -1) {
            selected_node_id_ = clicked_node;
            selected_link_a_ = -1;
            selected_link_b_ = -1;
            dragging_node_id_ = clicked_node;
            drag_offset_ = node_positions_[clicked_node] - world_pos;
            emit node_selected(clicked_node);
            update();
            return;
        }

        // Check if a link was clicked
        pair<int, int> clicked_link = find_link_at(world_pos, 10.0 / zoom_factor_);
        if (clicked_link.first != -1) {
            selected_node_id_ = -1;
            selected_link_a_ = clicked_link.first;
            selected_link_b_ = clicked_link.second;
            emit link_selected(clicked_link.first, clicked_link.second);
            update();
            return;
        }

        // Clicked on empty canvas: deselect and allow left-drag panning
        selected_node_id_ = -1;
        selected_link_a_ = -1;
        selected_link_b_ = -1;
        is_panning_ = true;
        pan_did_move_ = false;
        pan_start_pos_ = event->position();
        pan_last_pos_ = event->position();
        emit canvas_clicked();
        update();
        event->accept();
    }
}

void TopologyCanvas::mouseMoveEvent(QMouseEvent* event) {
    if (is_panning_ && (event->buttons() & (Qt::LeftButton | Qt::MiddleButton | Qt::RightButton))) {
        if (!pan_did_move_ && (event->position() - pan_start_pos_).manhattanLength() >= 4.0) {
            pan_did_move_ = true;
        }
        QPointF delta = event->position() - pan_last_pos_;
        pan_offset_ += delta;
        pan_last_pos_ = event->position();
        setCursor(Qt::ClosedHandCursor);
        update();
        event->accept();
        return;
    }

    QPointF world_pos = screen_to_world(event->position());

    if (dragging_node_id_ != -1 && (event->buttons() & Qt::LeftButton)) {
        setCursor(Qt::ClosedHandCursor);
        user_dragged_ = true;
        QPointF new_pos = world_pos + drag_offset_;
        node_positions_[dragging_node_id_] = new_pos;
        update();
        return;
    }

    // Interactive Hover Tracking
    int hovered_node = find_node_at(world_pos);
    pair<int, int> hovered_link = find_link_at(world_pos, 8.0 / zoom_factor_);

    bool changed = false;
    if (hovered_node != hovered_node_id_) {
        hovered_node_id_ = hovered_node;
        changed = true;
    }
    if (hovered_link.first != hovered_link_a_ || hovered_link.second != hovered_link_b_) {
        hovered_link_a_ = hovered_link.first;
        hovered_link_b_ = hovered_link.second;
        changed = true;
    }

    if (hovered_node != -1 || hovered_link.first != -1) {
        setCursor(Qt::PointingHandCursor);
    } else {
        setCursor(Qt::ArrowCursor);
    }

    if (changed) {
        update();
    }
}

void TopologyCanvas::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton || event->button() == Qt::MiddleButton || event->button() == Qt::RightButton) {
        is_panning_ = false;
    }
    if (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton) {
        setCursor(Qt::ArrowCursor);
    }
    if (event->button() == Qt::LeftButton) {
        dragging_node_id_ = -1;
        QPointF world_pos = screen_to_world(event->position());
        if (find_node_at(world_pos) != -1) {
            setCursor(Qt::PointingHandCursor);
        } else {
            setCursor(Qt::ArrowCursor);
        }
    }
}

void TopologyCanvas::wheelEvent(QWheelEvent* event) {
    const double scale_step = (event->angleDelta().y() > 0) ? 1.12 : (1.0 / 1.12);
    double new_zoom = max(0.4, min(2.5, zoom_factor_ * scale_step));
    if (abs(new_zoom - zoom_factor_) < 1e-4) return;

    QPointF mouse_screen = event->position();
    QPointF mouse_world_before = screen_to_world(mouse_screen);
    zoom_factor_ = new_zoom;
    pan_offset_ = mouse_screen - mouse_world_before * zoom_factor_;
    update();
    event->accept();
}

void TopologyCanvas::contextMenuEvent(QContextMenuEvent* event) {
    if (pan_did_move_) {
        pan_did_move_ = false;
        event->accept();
        return;
    }
    if (graph_ == nullptr) return;
    QPointF world_pos = screen_to_world(QPointF(event->pos()));
    int node_id = find_node_at(world_pos);
    if (node_id != -1) {
        selected_node_id_ = node_id;
        selected_link_a_ = -1;
        selected_link_b_ = -1;
        emit node_selected(node_id);
        update();

        model::Node* node = graph_->get_node(node_id);
        if (!node) return;
        QMenu menu(this);
        QString fail_label = node->is_active() ? "Fail Node (Go Offline)" : "Recover Node (Go Online)";
        QAction* act_toggle = menu.addAction(fail_label);
        connect(act_toggle, &QAction::triggered, this, [this, node_id]() {
            emit toggle_node_failure_requested(node_id);
        });

        QMenu* connect_menu = menu.addMenu("Connect Link to...");
        const dsa::DynamicArray<int>& all_ids = graph_->get_node_ids();
        bool has_targets = false;
        for (int i = 0; i < all_ids.size(); ++i) {
            int other_id = all_ids[i];
            if (other_id == node_id || graph_->get_link(node_id, other_id) != nullptr) continue;
            model::Node* other = graph_->get_node(other_id);
            QString target_name = other ? QString("%1 (ID %2)").arg(QString::fromStdString(other->get_name())).arg(other_id)
                                        : QString("Node %1").arg(other_id);
            QAction* act_conn = connect_menu->addAction(target_name);
            connect(act_conn, &QAction::triggered, this, [this, node_id, other_id]() {
                emit connect_link_requested(node_id, other_id);
            });
            has_targets = true;
        }
        if (!has_targets) {
            QAction* empty_act = connect_menu->addAction("(All nodes already connected)");
            empty_act->setEnabled(false);
        }

        menu.addSeparator();
        QAction* act_del = menu.addAction("Delete Node");
        connect(act_del, &QAction::triggered, this, [this, node_id]() {
            emit delete_node_requested(node_id);
        });
        menu.exec(event->globalPos());
        return;
    }

    pair<int, int> link = find_link_at(world_pos, 10.0 / zoom_factor_);
    if (link.first != -1) {
        selected_node_id_ = -1;
        selected_link_a_ = link.first;
        selected_link_b_ = link.second;
        emit link_selected(link.first, link.second);
        update();

        const model::Link* l = graph_->get_link(link.first, link.second);
        QMenu menu(this);
        QString fail_label = (l && l->is_failed()) ? "Recover Link" : "Fail Link";
        QAction* act_toggle = menu.addAction(fail_label);
        connect(act_toggle, &QAction::triggered, this, [this, link]() {
            emit toggle_link_failure_requested(link.first, link.second);
        });
        QAction* act_drop = menu.addAction("Force Drop Next Packet");
        connect(act_drop, &QAction::triggered, this, [this, link]() {
            emit inject_link_loss_requested(link.first, link.second);
        });
        QAction* act_corrupt = menu.addAction("Force Corrupt Next Packet");
        connect(act_corrupt, &QAction::triggered, this, [this, link]() {
            emit inject_link_corruption_requested(link.first, link.second);
        });
        menu.addSeparator();
        QAction* act_del = menu.addAction("Delete Link");
        connect(act_del, &QAction::triggered, this, [this, link]() {
            emit delete_link_requested(link.first, link.second);
        });
        menu.exec(event->globalPos());
        return;
    }

    QMenu menu(this);
    QAction* act_fit = menu.addAction("Fit / Reset View");
    connect(act_fit, &QAction::triggered, this, &TopologyCanvas::reset_view);
    menu.exec(event->globalPos());
}

void TopologyCanvas::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    position_overlay_controls();
    if (!user_dragged_) {
        node_positions_.clear();
        update_layout();
    }
}

}
}
