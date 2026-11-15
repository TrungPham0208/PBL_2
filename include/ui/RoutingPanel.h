#pragma once

#ifdef emit
#pragma push_macro("emit")
#undef emit
#define PBL_RESTORE_EMIT_RP
#endif
#include "model/NetworkGraph.h"
#include "services/RoutingService.h"
#ifdef PBL_RESTORE_EMIT_RP
#pragma pop_macro("emit")
#undef PBL_RESTORE_EMIT_RP
#endif

#include <QLabel>
#include <QTableWidget>
#include <QWidget>

namespace pbl {
namespace ui {

/**
 * @brief Tab 4 panel: Shortest path breakdown, Dijkstra dist[]/prev[] table, and link queue occupancy.
 */
class RoutingPanel : public QWidget {
    Q_OBJECT

public:
    explicit RoutingPanel(QWidget* parent = nullptr);

    void update_route_display(const model::NetworkGraph& graph,
                              const services::RoutingService& routing_service,
                              int source_id,
                              int destination_id,
                              const services::RoutingService::Route& route);

    void update_queue_table(const model::NetworkGraph& graph,
                            const services::RoutingService& routing_service);

private:
    QLabel* lbl_active_route_;
    QLabel* lbl_route_cost_;
    QLabel* lbl_route_breakdown_;
    QTableWidget* table_dijkstra_state_;
    QTableWidget* table_queues_;

    void update_dijkstra_state_table(const model::NetworkGraph& graph,
                                     const services::RoutingService& routing_service,
                                     int source_id);
    static void fit_table_height(QTableWidget* table, int min_height = 100);
};

}  // namespace ui
}  // namespace pbl
