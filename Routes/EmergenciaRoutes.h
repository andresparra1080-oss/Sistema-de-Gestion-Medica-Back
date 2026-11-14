#pragma once

#include "crow.h"
#include "crow/middlewares/cors.h"
#include "Services/EmergenciaService.h"

// Rutas del módulo 4 (emergencias): POST /api/emergencias
class EmergenciaRoutes {
public:
    explicit EmergenciaRoutes(EmergenciaService& emergenciaService);
    void registrarRutas(crow::App<crow::CORSHandler>& app);

private:
    EmergenciaService& emergenciaService_;
};
