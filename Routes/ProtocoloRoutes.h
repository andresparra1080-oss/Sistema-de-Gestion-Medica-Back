#pragma once

#include "crow.h"
#include "crow/middlewares/cors.h"
#include "Services/ProtocoloService.h"

// Rutas de protocolos de emergencia (KAN-50):
//   GET /api/protocolos          todos los protocolos con sus pasos
//   GET /api/protocolos/{tipo}   el protocolo de un tipo (TRAUMATISMO, HEMORRAGIA,
//                                INTOXICACION, QUEMADURA, OTRO)
class ProtocoloRoutes {
public:
    explicit ProtocoloRoutes(ProtocoloService& protocoloService);
    void registrarRutas(crow::App<crow::CORSHandler>& app);

private:
    ProtocoloService& protocoloService_;
};