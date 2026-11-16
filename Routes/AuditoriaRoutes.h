#pragma once

#include "crow.h"
#include "crow/middlewares/cors.h"
#include "Services/AuditoriaService.h"

// GET /api/pacientes/{id}/auditoria
class AuditoriaRoutes {
public:
    explicit AuditoriaRoutes(AuditoriaService& auditoriaService);
    void registrarRutas(crow::App<crow::CORSHandler>& app);

private:
    AuditoriaService& auditoriaService_;
};