#pragma once

#include "crow.h"
#include "crow/middlewares/cors.h"
#include "Services/AlertaService.h"

// Rutas de alertas (KAN-58):
//   GET /api/alertas              lista (?resuelta=0 deja solo las sin resolver)
//   PUT /api/alertas/{id}/leida   marca una alerta como leída (notificada = 1)
//   PUT /api/alertas/leidas       marca como leídas todas las sin resolver
class AlertaRoutes {
public:
    explicit AlertaRoutes(AlertaService& alertaService);
    void registrarRutas(crow::App<crow::CORSHandler>& app);

private:
    AlertaService& alertaService_;
};