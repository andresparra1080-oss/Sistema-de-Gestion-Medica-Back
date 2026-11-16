#include "Routes/LoteRoutes.h"

#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "Cross/ApiHttp.h"

namespace {

nlohmann::json aJson(const Lote& lote) {
    return {
        {"id_lote", lote.getIdLote()},
        {"id_medicamento", lote.getIdMedicamento()},
        {"num_lote", lote.getNumLote()},
        {"ubicacion", lote.getUbicacion()},
        {"fecha_vencimiento", lote.getFechaVencimiento()},
        {"cantidad", lote.getCantidad()}
    };
}

long long leerEntero(
    const nlohmann::json& cuerpo,
    const char* campo) {

    const auto& valor = cuerpo.at(campo);

    if (valor.is_number_unsigned()) {
        const auto numero =
            valor.get<unsigned long long>();

        if (numero > static_cast<unsigned long long>(
                std::numeric_limits<long long>::max())) {
            throw std::out_of_range("Entero fuera de rango");
        }

        return static_cast<long long>(numero);
    }

    if (!valor.is_number_integer()) {
        throw std::invalid_argument("Se esperaba un entero");
    }

    return valor.get<long long>();
}

}  // namespace

LoteRoutes::LoteRoutes(LoteService& loteService)
    : loteService_(loteService) {
}

void LoteRoutes::registrarRutas(
    crow::App<crow::CORSHandler>& app) {

    CROW_ROUTE(app, "/api/lotes")
        .methods(crow::HTTPMethod::Get)(
            [this](const crow::request& peticion) {

        std::string correo;
        std::string rol;

        if (!ApiHttp::peticionAutorizada(
                peticion, correo, rol)) {
            return ApiHttp::respuestaError(
                401, "Token ausente o invalido");
        }

        std::vector<Lote> lotes;
        std::string mensaje;

        const ResultadoOperacion resultado =
            loteService_.listar(correo, lotes, mensaje);

        if (resultado != ResultadoOperacion::Exito) {
            return ApiHttp::respuestaError(
                ApiHttp::codigoHttpDe(resultado), mensaje);
        }

        nlohmann::json lista = nlohmann::json::array();

        for (const Lote& lote : lotes) {
            lista.push_back(aJson(lote));
        }

        nlohmann::json respuesta;
        respuesta["success"] = true;
        respuesta["total"] = lista.size();
        respuesta["lotes"] = lista;

        return ApiHttp::respuestaJson(
            200, respuesta.dump());
    });

    CROW_ROUTE(app, "/api/lotes")
        .methods(crow::HTTPMethod::Post)(
            [this](const crow::request& peticion) {

        std::string correo;
        std::string rol;

        if (!ApiHttp::peticionAutorizada(
                peticion, correo, rol)) {
            return ApiHttp::respuestaError(
                401, "Token ausente o invalido");
        }

        Lote datos;

        try {
            const auto cuerpo =
                nlohmann::json::parse(peticion.body);

            if (!cuerpo.is_object()) {
                return ApiHttp::respuestaError(
                    400, "Se esperaba un objeto JSON");
            }

            datos.setIdMedicamento(
                leerEntero(cuerpo, "id_medicamento"));

            datos.setNumLote(
                cuerpo.at("num_lote").get<std::string>());

            datos.setUbicacion(
                cuerpo.at("ubicacion").get<std::string>());

            datos.setFechaVencimiento(
                cuerpo.at("fecha_vencimiento")
                    .get<std::string>());

            if (cuerpo.contains("cantidad")) {
                datos.setCantidad(
                    leerEntero(cuerpo, "cantidad"));
            }
        } catch (const std::exception&) {
            return ApiHttp::respuestaError(
                400,
                "JSON invalido, campos obligatorios ausentes "
                "o tipos de datos incorrectos");
        }

        Lote creado;
        std::string mensaje;

        const ResultadoOperacion resultado =
            loteService_.crear(
                datos, correo, creado, mensaje);

        if (resultado != ResultadoOperacion::Exito) {
            return ApiHttp::respuestaError(
                ApiHttp::codigoHttpDe(resultado), mensaje);
        }

        nlohmann::json respuesta;
        respuesta["success"] = true;
        respuesta["message"] = mensaje;
        respuesta["lote"] = aJson(creado);

        return ApiHttp::respuestaJson(
            201, respuesta.dump());
    });
}