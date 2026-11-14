#include "Routes/AlertaRoutes.h"

#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

#include "Cross/ApiHttp.h"
#include "Models/Alerta.h"

namespace {

nlohmann::json enteroONulo(const std::optional<long long>& valor) {
    return valor.has_value() ? nlohmann::json(*valor) : nlohmann::json(nullptr);
}

nlohmann::json aJson(const Alerta& alerta) {
    nlohmann::json json;
    json["id_alerta"] = alerta.idAlerta;
    json["id_medicamento"] = enteroONulo(alerta.idMedicamento);
    json["id_lote"] = enteroONulo(alerta.idLote);
    json["id_tratamiento"] = enteroONulo(alerta.idTratamiento);
    json["id_emergencia"] = enteroONulo(alerta.idEmergencia);
    json["tipo_alerta"] = alerta.tipoAlerta;
    json["mensaje"] = alerta.mensaje;
    json["prioridad"] = alerta.prioridad;
    json["notificada"] = alerta.notificada;
    json["resuelta"] = alerta.resuelta;
    // Alias "leida" por compatibilidad directa con el front
    json["leida"] = alerta.resuelta;
    json["fecha_generacion"] = alerta.fechaGeneracion;
    json["fecha_resolucion"] =
        alerta.fechaResolucion.has_value() ? nlohmann::json(*alerta.fechaResolucion) : nlohmann::json(nullptr);
    return json;
}

}  // namespace

AlertaRoutes::AlertaRoutes(AlertaService& alertaService)
    : alertaService_(alertaService) {
}

void AlertaRoutes::registrarRutas(crow::App<crow::CORSHandler>& app) {
    // GET /api/alertas?resuelta=0&tipo=STOCK_BAJO
    CROW_ROUTE(app, "/api/alertas")
        .methods(crow::HTTPMethod::Get)([this](const crow::request& peticion) {
            std::string correo;
            std::string rol;
            if (!ApiHttp::peticionAutorizada(peticion, correo, rol)) {
                return ApiHttp::respuestaError(401, "Token ausente o inválido");
            }

            std::optional<int> resueltaFiltro;
            const char* paramResuelta = peticion.url_params.get("resuelta");
            if (paramResuelta != nullptr && paramResuelta[0] != '\0') {
                const std::string valor(paramResuelta);
                if (valor == "0" || valor == "false") {
                    resueltaFiltro = 0;
                } else if (valor == "1" || valor == "true") {
                    resueltaFiltro = 1;
                } else {
                    return ApiHttp::respuestaError(400, "El parámetro resuelta debe ser 0 o 1");
                }
            }

            std::string tipoFiltro;
            const char* paramTipo = peticion.url_params.get("tipo");
            if (paramTipo != nullptr) {
                tipoFiltro = paramTipo;
            }

            std::vector<Alerta> alertas;
            std::string mensaje;
            const ResultadoOperacion resultado =
                alertaService_.listar(resueltaFiltro, tipoFiltro, correo, alertas, mensaje);

            if (resultado != ResultadoOperacion::Exito) {
                return ApiHttp::respuestaError(ApiHttp::codigoHttpDe(resultado), mensaje);
            }

            nlohmann::json lista = nlohmann::json::array();
            for (const Alerta& alerta : alertas) {
                lista.push_back(aJson(alerta));
            }

            nlohmann::json cuerpo;
            cuerpo["success"] = true;
            cuerpo["total"] = lista.size();
            cuerpo["alertas"] = lista;
            return ApiHttp::respuestaJson(200, cuerpo.dump());
        });

    // PUT /api/alertas/{id}/leida
    CROW_ROUTE(app, "/api/alertas/<int>/leida")
        .methods(crow::HTTPMethod::Put)([this](const crow::request& peticion, int idAlerta) {
            std::string correo;
            std::string rol;
            if (!ApiHttp::peticionAutorizada(peticion, correo, rol)) {
                return ApiHttp::respuestaError(401, "Token ausente o inválido");
            }

            Alerta actualizada;
            std::string mensaje;
            const ResultadoOperacion resultado =
                alertaService_.marcarLeida(idAlerta, correo, actualizada, mensaje);

            if (resultado != ResultadoOperacion::Exito) {
                return ApiHttp::respuestaError(ApiHttp::codigoHttpDe(resultado), mensaje);
            }

            nlohmann::json cuerpo;
            cuerpo["success"] = true;
            cuerpo["message"] = mensaje;
            cuerpo["alerta"] = aJson(actualizada);
            return ApiHttp::respuestaJson(200, cuerpo.dump());
        });

    // PUT /api/alertas/leidas
    CROW_ROUTE(app, "/api/alertas/leidas")
        .methods(crow::HTTPMethod::Put)([this](const crow::request& peticion) {
            std::string correo;
            std::string rol;
            if (!ApiHttp::peticionAutorizada(peticion, correo, rol)) {
                return ApiHttp::respuestaError(401, "Token ausente o inválido");
            }

            int cantidadActualizada = 0;
            std::string mensaje;
            const ResultadoOperacion resultado =
                alertaService_.marcarTodasLeidas(correo, cantidadActualizada, mensaje);

            if (resultado != ResultadoOperacion::Exito) {
                return ApiHttp::respuestaError(ApiHttp::codigoHttpDe(resultado), mensaje);
            }

            nlohmann::json cuerpo;
            cuerpo["success"] = true;
            cuerpo["message"] = mensaje;
            cuerpo["actualizadas"] = cantidadActualizada;
            return ApiHttp::respuestaJson(200, cuerpo.dump());
        });
}