#pragma once

#include "crow.h"
#include "crow/middlewares/cors.h"

#include "Services/LoteService.h"

class LoteRoutes {
public:
    explicit LoteRoutes(LoteService& loteService);

    void registrarRutas(crow::App<crow::CORSHandler>& app);

private:
    LoteService& loteService_;
};