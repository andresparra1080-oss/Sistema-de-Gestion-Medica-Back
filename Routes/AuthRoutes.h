#pragma once

#include "crow.h"
#include "crow/middlewares/cors.h"
#include "Services/AuthService.h"

class AuthRoutes {
public:
    explicit AuthRoutes(AuthService& authService);
    void registrarRutas(crow::App<crow::CORSHandler>& app);

private:
    AuthService& authService_;
};
