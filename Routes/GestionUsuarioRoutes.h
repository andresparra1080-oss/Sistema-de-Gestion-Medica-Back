#pragma once

#include "Services/GestionUsuarioService.h"
#include "crow.h"
#include "crow/middlewares/cors.h"

class GestionUsuarioRoutes {
public:
    explicit GestionUsuarioRoutes(GestionUsuarioService& gestionUsuarioService);
    void registrarRutas(crow::App<crow::CORSHandler>& app);

private:
    GestionUsuarioService& gestionUsuarioService_;
};
