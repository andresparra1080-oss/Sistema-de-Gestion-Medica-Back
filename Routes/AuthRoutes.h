#pragma once

#include "crow.h"
#include "Services/AuthService.h"

class AuthRoutes {
public:
    explicit AuthRoutes(AuthService& authService);
    void registrarRutas(crow::SimpleApp& app);

private:
    AuthService& authService_;
};
