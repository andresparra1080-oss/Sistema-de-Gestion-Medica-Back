#pragma once

#include "crow.h"
#include "crow/middlewares/cors.h"
#include "Services/EmergenciaService.h"

// Rutas del módulo 4 (emergencias):
//   POST   /api/emergencias                  registrar
//   GET    /api/emergencias                  lista (?activas=true)
//   GET    /api/emergencias/{id}             detalle con pacientes
//   GET    /api/emergencias/{id}/historial   cambios (logs_auditoria)
//   POST   /api/emergencias/{id}/acciones    registrar acción
//   GET    /api/emergencias/{id}/acciones    historial de acciones
//   PUT    /api/emergencias/{id}/estado      ACTIVA -> EN_ATENCION
//   PUT    /api/emergencias/{id}/finalizar   -> FINALIZADA
//   DELETE /api/emergencias/{id}             eliminar (solo FINALIZADA)
class EmergenciaRoutes {
public:
    explicit EmergenciaRoutes(EmergenciaService& emergenciaService);
    void registrarRutas(crow::App<crow::CORSHandler>& app);

private:
    EmergenciaService& emergenciaService_;
};