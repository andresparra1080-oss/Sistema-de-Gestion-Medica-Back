#include "Routes/AuditoriaRoutes.h"

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "Cross/ApiHttp.h"
#include "Models/RegistroAuditoria.h"

namespace {

nlohmann::json aJson(const RegistroAuditoria& registro) {
    nlohmann::json json;
    json["id_log"] = registro.idLog;
    json["id_usuario"] = registro.idUsuario;
    json["nombre_usuario"] = registro.nombreUsuario;
    json["accion"] = registro.accion;
    json["tabla_afectada"] = registro.tablaAfectada;
    json["id_registro"] = registro.idRegistro.has_value() ? nlohmann::json(*registro.idRegistro)
                                                           : nlohmann::json(nullptr);
    json["detalle_cambio"] = registro.detalleCambio.empty() ? nlohmann::json(nullptr)
                                                            : nlohmann::json(registro.detalleCambio);
    json["fecha_modificacion"] = registro.fechaModificacion;
    return json;
}

}  // namespace

AuditoriaRoutes::AuditoriaRoutes(AuditoriaService& auditoriaService)
    : auditoriaService_(auditoriaService) {
}

void AuditoriaRoutes::registrarRutas(crow::App<crow::CORSHandler>& app) {
    // Historial de auditoría del paciente: quién cambió qué y cuándo.
    CROW_ROUTE(app, "/api/pacientes/<int>/auditoria")
        .methods(crow::HTTPMethod::Get)([this](const crow::request& peticion, int idPaciente) {
            std::string correo;
            std::string rol;
            if (!ApiHttp::peticionAutorizada(peticion, correo, rol)) {
                return ApiHttp::respuestaError(401, "Token ausente o inválido");
            }

            std::vector<RegistroAuditoria> registros;
            std::string mensaje;
            const ResultadoOperacion resultado =
                auditoriaService_.listarPorPaciente(idPaciente, registros, mensaje);

            if (resultado != ResultadoOperacion::Exito) {
                return ApiHttp::respuestaError(ApiHttp::codigoHttpDe(resultado), mensaje);
            }

            nlohmann::json lista = nlohmann::json::array();
            for (const RegistroAuditoria& registro : registros) {
                lista.push_back(aJson(registro));
            }

            nlohmann::json cuerpo;
            cuerpo["success"] = true;
            cuerpo["total"] = lista.size();
            cuerpo["auditoria"] = lista;
            return ApiHttp::respuestaJson(200, cuerpo.dump());
        });
}