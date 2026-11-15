#include "Routes/ProtocoloRoutes.h"

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "Cross/ApiHttp.h"
#include "Models/Protocolo.h"

namespace {

nlohmann::json aJson(const Protocolo& protocolo) {
    nlohmann::json json;
    json["id_protocolo"] = protocolo.idProtocolo;
    json["tipo_emergencia"] = protocolo.tipoEmergencia;
    json["nombre"] = protocolo.nombre;

    nlohmann::json pasos = nlohmann::json::array();
    for (const PasoProtocolo& paso : protocolo.pasos) {
        nlohmann::json fila;
        fila["id_paso"] = paso.idPaso;
        fila["numero"] = paso.numero;
        fila["descripcion"] = paso.descripcion;
        pasos.push_back(fila);
    }
    json["pasos"] = pasos;
    return json;
}

}  // namespace

ProtocoloRoutes::ProtocoloRoutes(ProtocoloService& protocoloService)
    : protocoloService_(protocoloService) {
}

void ProtocoloRoutes::registrarRutas(crow::App<crow::CORSHandler>& app) {
    CROW_ROUTE(app, "/api/protocolos")
        .methods(crow::HTTPMethod::Get)([this](const crow::request& peticion) {
            std::string correo;
            std::string rol;
            if (!ApiHttp::peticionAutorizada(peticion, correo, rol)) {
                return ApiHttp::respuestaError(401, "Token ausente o inválido");
            }

            std::vector<Protocolo> protocolos;
            std::string mensaje;
            const ResultadoOperacion resultado = protocoloService_.listar(correo, protocolos, mensaje);

            if (resultado != ResultadoOperacion::Exito) {
                return ApiHttp::respuestaError(ApiHttp::codigoHttpDe(resultado), mensaje);
            }

            nlohmann::json lista = nlohmann::json::array();
            for (const Protocolo& protocolo : protocolos) {
                lista.push_back(aJson(protocolo));
            }

            nlohmann::json cuerpo;
            cuerpo["success"] = true;
            cuerpo["total"] = lista.size();
            cuerpo["protocolos"] = lista;
            return ApiHttp::respuestaJson(200, cuerpo.dump());
        });

    CROW_ROUTE(app, "/api/protocolos/<string>")
        .methods(crow::HTTPMethod::Get)([this](const crow::request& peticion, const std::string& tipo) {
            std::string correo;
            std::string rol;
            if (!ApiHttp::peticionAutorizada(peticion, correo, rol)) {
                return ApiHttp::respuestaError(401, "Token ausente o inválido");
            }

            Protocolo protocolo;
            std::string mensaje;
            const ResultadoOperacion resultado =
                protocoloService_.obtenerPorTipo(tipo, correo, protocolo, mensaje);

            if (resultado != ResultadoOperacion::Exito) {
                return ApiHttp::respuestaError(ApiHttp::codigoHttpDe(resultado), mensaje);
            }

            nlohmann::json cuerpo;
            cuerpo["success"] = true;
            cuerpo["protocolo"] = aJson(protocolo);
            return ApiHttp::respuestaJson(200, cuerpo.dump());
        });
}