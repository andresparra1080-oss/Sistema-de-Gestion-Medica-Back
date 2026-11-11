#include "Routes/EmergenciaRoutes.h"

#include <nlohmann/json.hpp>
#include <string>

#include "Cross/ApiHttp.h"
#include "Models/Emergencia.h"

namespace {

nlohmann::json textoONulo(const std::string& valor) {
    if (valor.empty()) {
        return nullptr;
    }
    return valor;
}

nlohmann::json aJson(const Emergencia& emergencia) {
    nlohmann::json json;
    json["id_emergencia"] = emergencia.idEmergencia;
    json["tipo"] = emergencia.tipo;
    json["sintomas"] = emergencia.sintomas;
    json["lesiones"] = textoONulo(emergencia.lesiones);
    json["diagnostico_inicial"] = emergencia.diagnosticoInicial;
    json["nivel_urgencia"] = emergencia.nivelUrgencia;
    json["estado"] = emergencia.estado;
    json["fecha_inicio"] = emergencia.fechaInicio;
    json["fecha_fin"] = nullptr;

    nlohmann::json pacientes = nlohmann::json::array();
    for (const PacienteEmergencia& paciente : emergencia.pacientes) {
        nlohmann::json fila;
        fila["id_paciente"] = paciente.idPaciente;
        fila["nombre_completo"] = paciente.nombreCompleto;
        pacientes.push_back(fila);
    }
    json["pacientes"] = pacientes;

    return json;
}

nlohmann::json aJson(const AlertaEmergencia& alerta) {
    nlohmann::json json;
    json["id_alerta"] = alerta.idAlerta;
    json["tipo_alerta"] = alerta.tipoAlerta;
    json["mensaje"] = alerta.mensaje;
    json["prioridad"] = alerta.prioridad;
    json["notificada"] = alerta.notificada;
    json["fecha_generacion"] = alerta.fechaGeneracion;
    return json;
}

// Cuerpo esperado:
//   { "tipo": "TRAUMATISMO", "sintomas": "...", "lesiones": "..." | null,
//     "diagnostico_inicial": "...", "nivel_urgencia": "ALTA" (opcional),
//     "pacientes": [1, 2] }
// La fecha de inicio, el estado y quién registra no se leen: los pone el backend.
bool leerCuerpo(const std::string& textoJson, NuevaEmergencia& datos, std::string& error) {
    nlohmann::json cuerpo;
    try {
        cuerpo = nlohmann::json::parse(textoJson);
    } catch (const std::exception&) {
        error = "JSON inválido";
        return false;
    }

    if (!cuerpo.is_object()) {
        error = "Se esperaba un objeto JSON";
        return false;
    }

    try {
        datos.tipo = cuerpo.value("tipo", "");
        datos.sintomas = cuerpo.value("sintomas", "");
        datos.diagnosticoInicial = cuerpo.value("diagnostico_inicial", "");

        // lesiones es opcional: ausente o null = sin lesiones.
        if (cuerpo.contains("lesiones") && !cuerpo["lesiones"].is_null()) {
            datos.lesiones = cuerpo["lesiones"].get<std::string>();
        }

        if (cuerpo.contains("nivel_urgencia") && !cuerpo["nivel_urgencia"].is_null()) {
            datos.nivelUrgencia = cuerpo["nivel_urgencia"].get<std::string>();
        }

        if (!cuerpo.contains("pacientes") || !cuerpo["pacientes"].is_array()) {
            error = "El campo pacientes debe ser una lista con los identificadores de los tripulantes";
            return false;
        }

        for (const auto& entrada : cuerpo["pacientes"]) {
            if (!entrada.is_number_integer()) {
                error = "Cada elemento de pacientes debe ser el identificador numérico de un paciente";
                return false;
            }

            PacienteEmergencia paciente;
            paciente.idPaciente = entrada.get<long long>();
            datos.pacientes.push_back(paciente);
        }
    } catch (const std::exception&) {
        error = "Alguno de los campos tiene un tipo de dato incorrecto";
        return false;
    }

    error.clear();
    return true;
}

}  // namespace

EmergenciaRoutes::EmergenciaRoutes(EmergenciaService& emergenciaService)
    : emergenciaService_(emergenciaService) {
}

void EmergenciaRoutes::registrarRutas(crow::App<crow::CORSHandler>& app) {
    CROW_ROUTE(app, "/api/emergencias")
        .methods(crow::HTTPMethod::Post)([this](const crow::request& peticion) {
            std::string correo;
            std::string rol;
            if (!ApiHttp::peticionAutorizada(peticion, correo, rol)) {
                return ApiHttp::respuestaError(401, "Token ausente o inválido");
            }

            NuevaEmergencia datos;
            std::string errorCuerpo;
            if (!leerCuerpo(peticion.body, datos, errorCuerpo)) {
                return ApiHttp::respuestaError(400, errorCuerpo);
            }

            Emergencia creada;
            std::string mensaje;
            const ResultadoOperacion resultado = emergenciaService_.registrar(datos, correo, creada, mensaje);

            if (resultado != ResultadoOperacion::Exito) {
                return ApiHttp::respuestaError(ApiHttp::codigoHttpDe(resultado), mensaje);
            }

            nlohmann::json cuerpo;
            cuerpo["success"] = true;
            cuerpo["message"] = mensaje;
            cuerpo["emergencia"] = aJson(creada);
            cuerpo["alerta"] = aJson(creada.alerta);
            return ApiHttp::respuestaJson(201, cuerpo.dump());
        });
}
