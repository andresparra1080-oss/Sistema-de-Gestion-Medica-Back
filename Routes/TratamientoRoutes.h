#pragma once

#include "crow.h"
#include "crow/middlewares/cors.h"
#include "Services/TratamientoService.h"

class TratamientoRoutes {
public:
    explicit TratamientoRoutes(TratamientoService& tratamientoService);
    void registrarRutas(crow::App<crow::CORSHandler>& app);

private:
    TratamientoService& tratamientoService_;
};