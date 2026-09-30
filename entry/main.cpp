#include "crow.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

int main() {
    const char* portEnv = std::getenv("PORT");
    int port = 18080;

    if (portEnv != nullptr && portEnv[0] != '\0') {
        try {
            port = std::stoi(portEnv);
        } catch (const std::exception&) {
            std::cerr << "Variable PORT no válida. Se usará el puerto 18080 por defecto." << std::endl;
            port = 18080;
        }
    }

    crow::SimpleApp app;

    CROW_ROUTE(app, "/health")([]() {
        nlohmann::json response;
        response["status"] = "ok";
        response["service"] = "heartmed-backend";
        return crow::response(response.dump());
    });

    CROW_ROUTE(app, "/api/health")([]() {
        nlohmann::json response;
        response["status"] = "ok";
        response["message"] = "Backend de HeartMed funcionando";
        return crow::response(response.dump());
    });

    std::cout << "Iniciando servidor Crow en puerto " << port << std::endl;
    app.port(port).multithreaded().run();

    return 0;
}
