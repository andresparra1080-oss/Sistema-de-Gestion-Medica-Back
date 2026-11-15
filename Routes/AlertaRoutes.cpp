#include "Routes/AlertaRoutes.h"

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "Cross/ApiHttp.h"
#include "Models/AlertaSistema.h"

namespace {

template <typename T>
nlohmann::json enteroONulo(const std::optional<T>& valor) {
    return valor.has_value() ? nlohmann::json(*valor) : nlohmann::json(nullptr);
}

nlohmann::json aJson(const AlertaSistema& alerta) {
    nlohmann::json json;
    json["id_alerta"] = alerta.idAlerta;
    json["id_medicamento"] = enteroONulo(alerta.idMedicamento);
    json["id_lote"] = enteroONulo(alerta.idLote);
    json["id_tratamiento"] = enteroONulo(alerta.idTratamiento);
    json["id_emergencia"] = enteroONulo(alerta.idEmergencia);
    json["tipo_alerta"] = alerta.tipoAlerta;
    json["mensaje"] = alerta.mensaje;
    json["prioridad"] = alerta.prioridad;
    // 0 / 1, como están en la tabla: el front los compara como número.
    json["notificada"] = alerta.notificada ? 1 : 0;
    json["resuelta"] = alerta.resuelta ? 1 : 0;
    json["fecha_generacion"] = alerta.fechaGeneracion;
    json["fecha_resolucion"] = enteroONulo(alerta.fechaResolucion);
    json["nombre_medicamento"] = alerta.nombreMedicamento.empty() ? nlohmann::json(nullptr)
                                                                  : nlohmann::json(alerta.nombreMedicamento);
    json["num_lote"] = alerta.numLote.empty() ? nlohmann::json(nullptr) : nlohmann::json(alerta.numLote);
    return json;
}

}  // namespace

AlertaRoutes::AlertaRoutes(AlertaService& alertaService)
    : alertaService_(alertaService) {
}

void AlertaRoutes::registrarRutas(crow::App<crow::CORSHandler>& app) {
    // Lista de alertas. ?resuelta=0 (o false) devuelve solo las sin resolver.
    CROW_ROUTE(app, "/api/alertas")
        .methods(crow::HTTPMethod::Get)([this](const crow::request& peticion) {
            std::string correo;
            std::string rol;
            if (!ApiHttp::peticionAutorizada(peticion, correo, rol)) {
                return ApiHttp::respuestaError(401, "Token ausente o inválido");
            }

            const char* parametroResuelta = peticion.url_params.get("resuelta");
            const bool soloSinResolver =
                parametroResuelta != nullptr &&
                (std::string(parametroResuelta) == "0" || std::string(parametroResuelta) == "false");

            std::vector<AlertaSistema> alertas;
            std::string mensaje;
            const ResultadoOperacion resultado =
                alertaService_.listar(soloSinResolver, correo, alertas, mensaje);

            if (resultado != ResultadoOperacion::Exito) {
                return ApiHttp::respuestaError(ApiHttp::codigoHttpDe(resultado), mensaje);
            }

            nlohmann::json lista = nlohmann::json::array();
            for (const AlertaSistema& alerta : alertas) {
                lista.push_back(aJson(alerta));
            }

            nlohmann::json cuerpo;
            cuerpo["success"] = true;
            cuerpo["total"] = lista.size();
            cuerpo["alertas"] = lista;
            return ApiHttp::respuestaJson(200, cuerpo.dump());
        });

    // Marcar todas como leídas. Va antes de la ruta con id por claridad; no
    // chocan porque esta no tiene el segmento numérico.
    CROW_ROUTE(app, "/api/alertas/leidas")
        .methods(crow::HTTPMethod::Put)([this](const crow::request& peticion) {
            std::string correo;
            std::string rol;
            if (!ApiHttp::peticionAutorizada(peticion, correo, rol)) {
                return ApiHttp::respuestaError(401, "Token ausente o inválido");
            }

            int cantidad = 0;
            std::string mensaje;
            const ResultadoOperacion resultado = alertaService_.marcarTodasLeidas(correo, cantidad, mensaje);

            if (resultado != ResultadoOperacion::Exito) {
                return ApiHttp::respuestaError(ApiHttp::codigoHttpDe(resultado), mensaje);
            }

            nlohmann::json cuerpo;
            cuerpo["success"] = true;
            cuerpo["message"] = mensaje;
            cuerpo["marcadas"] = cantidad;
            return ApiHttp::respuestaJson(200, cuerpo.dump());
        });

    CROW_ROUTE(app, "/api/alertas/<int>/leida")
        .methods(crow::HTTPMethod::Put)([this](const crow::request& peticion, int idAlerta) {
            std::string correo;
            std::string rol;
            if (!ApiHttp::peticionAutorizada(peticion, correo, rol)) {
                return ApiHttp::respuestaError(401, "Token ausente o inválido");
            }

            std::string mensaje;
            const ResultadoOperacion resultado = alertaService_.marcarLeida(idAlerta, correo, mensaje);

            if (resultado != ResultadoOperacion::Exito) {
                return ApiHttp::respuestaError(ApiHttp::codigoHttpDe(resultado), mensaje);
            }

            nlohmann::json cuerpo;
            cuerpo["success"] = true;
            cuerpo["message"] = mensaje;
            return ApiHttp::respuestaJson(200, cuerpo.dump());
        });
}