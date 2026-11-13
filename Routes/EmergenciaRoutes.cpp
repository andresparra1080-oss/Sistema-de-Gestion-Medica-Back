#include "Routes/EmergenciaRoutes.h"

#include <cctype>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "Cross/ApiHttp.h"
#include "Models/AccionEmergencia.h"
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
    // fecha_fin solo tiene valor cuando la emergencia está FINALIZADA.
    json["fecha_fin"] = emergencia.fechaFin.has_value() ? nlohmann::json(*emergencia.fechaFin) : nlohmann::json(nullptr);

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

nlohmann::json aJson(const AccionEmergencia& accion) {
    nlohmann::json json;
    json["id_accion"] = accion.idAccion;
    json["id_emergencia"] = accion.idEmergencia;
    json["id_usuario_ejecutor"] = accion.idUsuarioEjecutor;
    json["nombre_ejecutor"] = accion.nombreEjecutor;
    json["descripcion_accion"] = accion.descripcion;
    json["fecha_hora"] = accion.fechaHora;
    return json;
}

// Cuerpo esperado en POST /api/emergencias/{id}/acciones:
//   { "descripcion": "..." }   (también se acepta "descripcion_accion")
// El usuario ejecutor y la fecha y hora no se leen: salen del token y de la base.
bool leerDescripcion(const std::string& textoJson, std::string& descripcion, std::string& error) {
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
        descripcion = cuerpo.value("descripcion", "");
        if (descripcion.empty()) {
            descripcion = cuerpo.value("descripcion_accion", "");
        }
    } catch (const std::exception&) {
        error = "La descripción debe ser un texto";
        return false;
    }

    error.clear();
    return true;
}

// PUT /api/emergencias/{id}/estado solo hace ACTIVA -> EN_ATENCION. El cuerpo
// es opcional; si trae "estado", tiene que ser EN_ATENCION.
bool validarCuerpoEstado(const std::string& textoJson, std::string& error) {
    bool soloEspacios = true;
    for (char caracter : textoJson) {
        if (!std::isspace(static_cast<unsigned char>(caracter))) {
            soloEspacios = false;
            break;
        }
    }
    if (soloEspacios) {
        return true;
    }

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

    if (cuerpo.contains("estado") && !cuerpo["estado"].is_null()) {
        std::string estado;
        if (cuerpo["estado"].is_string()) {
            estado = cuerpo["estado"].get<std::string>();
        }
        for (char& caracter : estado) {
            caracter = static_cast<char>(std::toupper(static_cast<unsigned char>(caracter)));
        }
        if (estado != "EN_ATENCION") {
            error = "Este endpoint solo permite la transición ACTIVA -> EN_ATENCION";
            return false;
        }
    }

    error.clear();
    return true;
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

    // Registrar una acción de atención. Quién la ejecuta sale del token.
    CROW_ROUTE(app, "/api/emergencias/<int>/acciones")
        .methods(crow::HTTPMethod::Post)([this](const crow::request& peticion, int idEmergencia) {
            std::string correo;
            std::string rol;
            if (!ApiHttp::peticionAutorizada(peticion, correo, rol)) {
                return ApiHttp::respuestaError(401, "Token ausente o inválido");
            }

            std::string descripcion;
            std::string errorCuerpo;
            if (!leerDescripcion(peticion.body, descripcion, errorCuerpo)) {
                return ApiHttp::respuestaError(400, errorCuerpo);
            }

            AccionEmergencia creada;
            std::string mensaje;
            const ResultadoOperacion resultado =
                emergenciaService_.registrarAccion(idEmergencia, descripcion, correo, creada, mensaje);

            if (resultado != ResultadoOperacion::Exito) {
                return ApiHttp::respuestaError(ApiHttp::codigoHttpDe(resultado), mensaje);
            }

            nlohmann::json cuerpo;
            cuerpo["success"] = true;
            cuerpo["message"] = mensaje;
            cuerpo["accion"] = aJson(creada);
            return ApiHttp::respuestaJson(201, cuerpo.dump());
        });

    // Historial de acciones de una emergencia.
    CROW_ROUTE(app, "/api/emergencias/<int>/acciones")
        .methods(crow::HTTPMethod::Get)([this](const crow::request& peticion, int idEmergencia) {
            std::string correo;
            std::string rol;
            if (!ApiHttp::peticionAutorizada(peticion, correo, rol)) {
                return ApiHttp::respuestaError(401, "Token ausente o inválido");
            }

            std::vector<AccionEmergencia> acciones;
            std::string mensaje;
            const ResultadoOperacion resultado =
                emergenciaService_.listarAcciones(idEmergencia, correo, acciones, mensaje);

            if (resultado != ResultadoOperacion::Exito) { 
                                return ApiHttp::respuestaError(ApiHttp::codigoHttpDe(resultado), mensaje);
            }

            nlohmann::json lista = nlohmann::json::array();
            for (const AccionEmergencia& accion : acciones) {
                lista.push_back(aJson(accion));
            }

            nlohmann::json cuerpo;
            cuerpo["success"] = true;
            cuerpo["total"] = lista.size();
            cuerpo["acciones"] = lista;
            return ApiHttp::respuestaJson(200, cuerpo.dump());
        });

    // ACTIVA -> EN_ATENCION.
    CROW_ROUTE(app, "/api/emergencias/<int>/estado")
        .methods(crow::HTTPMethod::Put)([this](const crow::request& peticion, int idEmergencia) {
            std::string correo;
            std::string rol;
            if (!ApiHttp::peticionAutorizada(peticion, correo, rol)) {
                return ApiHttp::respuestaError(401, "Token ausente o inválido");
            }

            std::string errorCuerpo;
            if (!validarCuerpoEstado(peticion.body, errorCuerpo)) {
                return ApiHttp::respuestaError(400, errorCuerpo);
            }

            Emergencia actualizada;
            std::string mensaje;
            const ResultadoOperacion resultado =
                emergenciaService_.marcarEnAtencion(idEmergencia, correo, actualizada, mensaje);

            if (resultado != ResultadoOperacion::Exito) {
                return ApiHttp::respuestaError(ApiHttp::codigoHttpDe(resultado), mensaje);
            }

            nlohmann::json cuerpo;
            cuerpo["success"] = true;
            cuerpo["message"] = mensaje;
            cuerpo["emergencia"] = aJson(actualizada);
            return ApiHttp::respuestaJson(200, cuerpo.dump());
        });

    // Cierra la emergencia: estado FINALIZADA y fecha_fin automática.
    CROW_ROUTE(app, "/api/emergencias/<int>/finalizar")
        .methods(crow::HTTPMethod::Put)([this](const crow::request& peticion, int idEmergencia) {
            std::string correo;
            std::string rol;
            if (!ApiHttp::peticionAutorizada(peticion, correo, rol)) {
                return ApiHttp::respuestaError(401, "Token ausente o inválido");
            }

            Emergencia actualizada;
            std::string mensaje;
            const ResultadoOperacion resultado =
                emergenciaService_.finalizar(idEmergencia, correo, actualizada, mensaje);

            if (resultado != ResultadoOperacion::Exito) {
                return ApiHttp::respuestaError(ApiHttp::codigoHttpDe(resultado), mensaje);
            }

            nlohmann::json cuerpo;
            cuerpo["success"] = true;
            cuerpo["message"] = mensaje;
            cuerpo["emergencia"] = aJson(actualizada);
            return ApiHttp::respuestaJson(200, cuerpo.dump());
        });
}
                
