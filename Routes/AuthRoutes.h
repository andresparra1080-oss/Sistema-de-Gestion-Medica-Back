#pragma once

#include "crow.h"
#include "Cross/CorsMiddleware.h"
#include "Services/AuthService.h"

class AuthRoutes {
public:
    explicit AuthRoutes(AuthService& authService);
    void registrarRutas(crow::App<CorsMiddleware>& app);

private:
    AuthService& authService_;
};
