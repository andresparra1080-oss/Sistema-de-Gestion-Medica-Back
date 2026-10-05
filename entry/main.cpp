#include "crow.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

#include "Cross/Config.h"
#include "Database/SQLite.h"
#include "Routes/AuthRoutes.h"
#include "Services/AuthService.h"

int main() {
    const std::filesystem::path rutaArchivoEnv = ".env";
    Configuracion::instancia().cargarDesdeArchivo(rutaArchivoEnv.string());

    const std::string rutaBaseDatos = Configuracion::instancia().obtenerCadena("DB_PATH", "./heartmed_db.sqlite3");
    BaseDatosSqlite::instancia().abrir(rutaBaseDatos);

    const char* puertoEntorno = std::getenv("PORT");
    int puerto = Configuracion::instancia().obtenerEntero("PORT", 18080);

    if (puertoEntorno != nullptr && puertoEntorno[0] != '\0') {
        try {
            puerto = std::stoi(puertoEntorno);
        } catch (const std::exception&) {
            std::cerr << "La variable PORT no es válida. Se usará el puerto " << puerto << std::endl;
        }
    }

    crow::SimpleApp aplicacion;

    AuthService servicioAutenticacion;
    AuthRoutes rutasAutenticacion(servicioAutenticacion);
    rutasAutenticacion.registrarRutas(aplicacion);

    CROW_ROUTE(aplicacion, "/health")([]() {
        nlohmann::json respuesta;
        respuesta["status"] = "ok";
        respuesta["service"] = "heartmed-backend";
        return crow::response(respuesta.dump());
    });

    CROW_ROUTE(aplicacion, "/api/health")([]() {
        nlohmann::json respuesta;
        respuesta["status"] = "ok";
        respuesta["message"] = "Backend de HeartMed funcionando";
        respuesta["database"] = Configuracion::instancia().obtenerCadena("DB_NAME", "heartmed_db");
        return crow::response(respuesta.dump());
    });

    std::cout << "Configuración cargada desde .env" << std::endl;
    std::cout << "Base de datos: " << rutaBaseDatos << std::endl;
    std::cout << "Iniciando servidor Crow en puerto " << puerto << std::endl;
    aplicacion.port(puerto).multithreaded().run();

    return 0;
}
