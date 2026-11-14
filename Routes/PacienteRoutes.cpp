#include "Routes/PacienteRoutes.h"

#include <cctype>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "Cross/ApiHttp.h"
#include "Models/Paciente.h"

namespace {

std::string normalizarRol(const std::string& rol) {
    std::string salida;
    salida.reserve(rol.size());

    for (unsigned char c : rol) {
        switch (c) {
            case 0xC1: case 0xE1: salida += 'a'; break; // Á / á
            case 0xC9: case 0xE9: salida += 'e'; break; // É / é
            case 0xCD: case 0xED: salida += 'i'; break; // Í / í
            case 0xD3: case 0xF3: salida += 'o'; break; // Ó / ó
            case 0xDA: case 0xFA: salida += 'u'; break; // Ú / ú
            case 0xDC: case 0xFC: salida += 'u'; break; // Ü / ü
            default:
                if (std::isalnum(c) || c == '/' || c == '-' || c == ' ') {
                    salida.push_back(static_cast<char>(std::tolower(c)));
                }
                break;
        }
    }

    std::string limpio;
    limpio.reserve(salida.size());
    for (char c : salida) {
        if (c == ' ' && (!limpio.empty() && limpio.back() == ' ')) {
            continue;
        }
        limpio.push_back(c);
    }

    return limpio;
}

bool tienePermisoPaciente(const std::string& rol) {
    const std::string rolNormalizado = normalizarRol(rol);
    return rolNormalizado == "capitan" ||
           rolNormalizado == "capitan medico principal" ||
           rolNormalizado == "cirujano" ||
           rolNormalizado == "cirujano naval";
}

nlohmann::json aJson(const Paciente& paciente) {
    nlohmann::json json;
    json["id_paciente"] = paciente.getIdPaciente();

    if (paciente.tieneIdUsuario()) {
        json["id_usuario"] = paciente.getIdUsuario();
    } else {
        json["id_usuario"] = nullptr;
    }

    json["documento_identidad"] = paciente.getDocumentoIdentidad();
    json["nombre_completo"] = paciente.getNombreCompleto();

    if (paciente.getFotoPerfil().empty()) {
        json["foto_perfil"] = nullptr;
    } else {
        json["foto_perfil"] = paciente.getFotoPerfil();
    }

    json["fecha_nacimiento"] = paciente.getFechaNacimiento();
    json["tipo_sangre"] = paciente.getTipoSangre();
    json["rango_cargo"] = paciente.getRangoCargo();
    json["recompensa"] = paciente.getRecompensa();
    json["estado_salud"] = paciente.getEstadoSalud();

    if (paciente.getObservaciones().empty()) {
        json["observaciones"] = nullptr;
    } else {
        json["observaciones"] = paciente.getObservaciones();
    }

    json["estado"] = paciente.getEstadoSalud();
    return json;
}

bool leerCuerpo(const std::string& textoJson, Paciente& datos, std::string& error) {
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
        datos.setDocumentoIdentidad(cuerpo.value("documento_identidad", ""));
        datos.setNombreCompleto(cuerpo.value("nombre_completo", ""));
        datos.setFotoPerfil(cuerpo.value("foto_perfil", ""));
        datos.setFechaNacimiento(cuerpo.value("fecha_nacimiento", ""));
        datos.setTipoSangre(cuerpo.value("tipo_sangre", ""));
        datos.setRangoCargo(cuerpo.value("rango_cargo", ""));
        datos.setRecompensa(cuerpo.value("recompensa", static_cast<long long>(0)));
        datos.setObservaciones(cuerpo.value("observaciones", ""));

        std::string estado = cuerpo.value("estado_salud", "");
        if (estado.empty()) {
            estado = cuerpo.value("estado", "");
        }
        datos.setEstadoSalud(estado);

        if (cuerpo.contains("id_usuario") && !cuerpo["id_usuario"].is_null()) {
            datos.setIdUsuario(cuerpo["id_usuario"].get<long long>());
        }
    } catch (const std::exception&) {
        error = "Alguno de los campos tiene un tipo de dato incorrecto";
        return false;
    }

    error.clear();
    return true;
}

}  // namespace

PacienteRoutes::PacienteRoutes(PacienteService& pacienteService)
    : pacienteService_(pacienteService) {
}

void PacienteRoutes::registrarRutas(crow::App<crow::CORSHandler>& app) {
    CROW_ROUTE(app, "/api/pacientes")
        .methods(crow::HTTPMethod::Get)([this](const crow::request& peticion) {
            std::string correo;
            std::string rol;
            if (!ApiHttp::peticionAutorizada(peticion, correo, rol)) {
                return ApiHttp::respuestaError(401, "Token ausente o inválido");
            }

            std::vector<Paciente> pacientes;
            std::string mensaje;
            const ResultadoOperacion resultado = pacienteService_.listar(pacientes, mensaje);
            if (resultado != ResultadoOperacion::Exito) {
                return ApiHttp::respuestaError(ApiHttp::codigoHttpDe(resultado), mensaje);
            }

            nlohmann::json lista = nlohmann::json::array();
            for (const Paciente& paciente : pacientes) {
                lista.push_back(aJson(paciente));
            }

            nlohmann::json cuerpo;
            cuerpo["success"] = true;
            cuerpo["total"] = lista.size();
            cuerpo["pacientes"] = lista;
            return ApiHttp::respuestaJson(200, cuerpo.dump());
        });

    CROW_ROUTE(app, "/api/pacientes")
        .methods(crow::HTTPMethod::Post)([this](const crow::request& peticion) {
            std::string correo;
            std::string rol;
            if (!ApiHttp::peticionAutorizada(peticion, correo, rol)) {
                return ApiHttp::respuestaError(401, "Token ausente o inválido");
            }

            if (!tienePermisoPaciente(rol)) {
                return ApiHttp::respuestaError(403, "Acceso denegado: Solo Capitán y Cirujano pueden registrar pacientes");
            }

            Paciente datos;
            std::string errorCuerpo;
            if (!leerCuerpo(peticion.body, datos, errorCuerpo)) {
                return ApiHttp::respuestaError(400, errorCuerpo);
            }

            Paciente creado;
            std::string mensaje;
            const ResultadoOperacion resultado = pacienteService_.crear(datos, creado, mensaje);
            if (resultado != ResultadoOperacion::Exito) {
                return ApiHttp::respuestaError(ApiHttp::codigoHttpDe(resultado), mensaje);
            }

            nlohmann::json cuerpo;
            cuerpo["success"] = true;
            cuerpo["message"] = mensaje;
            cuerpo["paciente"] = aJson(creado);
            return ApiHttp::respuestaJson(201, cuerpo.dump());
        });

    CROW_ROUTE(app, "/api/pacientes/<int>")
        .methods(crow::HTTPMethod::Get)([this](const crow::request& peticion, int idPaciente) {
            std::string correo;
            std::string rol;
            if (!ApiHttp::peticionAutorizada(peticion, correo, rol)) {
                return ApiHttp::respuestaError(401, "Token ausente o inválido");
            }

            Paciente paciente;
            std::string mensaje;
            const ResultadoOperacion resultado = pacienteService_.obtenerPorId(idPaciente, paciente, mensaje);
            if (resultado != ResultadoOperacion::Exito) {
                return ApiHttp::respuestaError(ApiHttp::codigoHttpDe(resultado), mensaje);
            }

            nlohmann::json cuerpo;
            cuerpo["success"] = true;
            cuerpo["paciente"] = aJson(paciente);
            return ApiHttp::respuestaJson(200, cuerpo.dump());
        });

    CROW_ROUTE(app, "/api/pacientes/<int>")
        .methods(crow::HTTPMethod::Put)([this](const crow::request& peticion, int idPaciente) {
            std::string correo;
            std::string rol;
            if (!ApiHttp::peticionAutorizada(peticion, correo, rol)) {
                return ApiHttp::respuestaError(401, "Token ausente o inválido");
            }

            if (!tienePermisoPaciente(rol)) {
                return ApiHttp::respuestaError(403, "Acceso denegado: Solo Capitán y Cirujano pueden modificar pacientes");
            }

            Paciente datos;
            std::string errorCuerpo;
            if (!leerCuerpo(peticion.body, datos, errorCuerpo)) {
                return ApiHttp::respuestaError(400, errorCuerpo);
            }

            Paciente actualizado;
            std::string mensaje;
            const ResultadoOperacion resultado = pacienteService_.actualizar(idPaciente, datos, actualizado, mensaje);
            if (resultado != ResultadoOperacion::Exito) {
                return ApiHttp::respuestaError(ApiHttp::codigoHttpDe(resultado), mensaje);
            }

            nlohmann::json cuerpo;
            cuerpo["success"] = true;
            cuerpo["message"] = mensaje;
            cuerpo["paciente"] = aJson(actualizado);
            return ApiHttp::respuestaJson(200, cuerpo.dump());
        });

    CROW_ROUTE(app, "/api/pacientes/<int>")
        .methods(crow::HTTPMethod::Delete)([this](const crow::request& peticion, int idPaciente) {
            std::string correo;
            std::string rol;
            if (!ApiHttp::peticionAutorizada(peticion, correo, rol)) {
                return ApiHttp::respuestaError(401, "Token ausente o inválido");
            }

            if (!tienePermisoPaciente(rol)) {
                return ApiHttp::respuestaError(403, "Acceso denegado: Solo Capitán y Cirujano pueden eliminar pacientes");
            }

            std::string mensaje;
            const ResultadoOperacion resultado = pacienteService_.eliminar(idPaciente, mensaje);
            if (resultado != ResultadoOperacion::Exito) {
                return ApiHttp::respuestaError(ApiHttp::codigoHttpDe(resultado), mensaje);
            }

            nlohmann::json cuerpo;
            cuerpo["success"] = true;
            cuerpo["message"] = mensaje;
            return ApiHttp::respuestaJson(200, cuerpo.dump());
        });
}
