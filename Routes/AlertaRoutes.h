#pragma once

#include "crow.h"
#include "crow/middlewares/cors.h"
#include "Services/AlertaService.h"

// Rutas de alertas (KAN-57):
//   GET /api/alertas
//   PUT /api/alertas/<int>/leida
//   PUT /api/alertas/leidas
class AlertaRoutes {
public:
    explicit AlertaRoutes(AlertaService& alertaService);
    void registrarRutas(crow::App<crow::CORSHandler>& app);

private:
    AlertaService& alertaService_;
};