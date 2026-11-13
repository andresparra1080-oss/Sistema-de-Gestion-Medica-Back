#include "Routes/TratamientoRoutes.h"

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "Cross/ApiHttp.h"

namespace {

nlohmann::json aJson(const Tratamiento& tratamiento) {
    nlohmann::json json;

    json["id_tratamiento"] = tratamiento.getIdTratamiento();
    json["id_paciente"] = tratamiento.getIdPaciente();
    json["id_medicamento"] = tratamiento.getIdMedicamento();
    json["id_usuario_registra"] = tratamiento.getIdUsuarioRegistra();
    json["dosis"] = tratamiento.getDosis();
    json["frecuencia"] = tratamiento.getFrecuencia();

    if (tratamiento.tieneIntervaloHoras()) {
        json["intervalo_horas"] = tratamiento.getIntervaloHoras();
    } else {
        json["intervalo_horas"] = nullptr;
    }

    json["fecha_inicio"] = tratamiento.getFechaInicio();

    if (tratamiento.getFechaFin().empty()) {
        json["fecha_fin"] = nullptr;
    } else {
        json["fecha_fin"] = tratamiento.getFechaFin();
    }

    if (tratamiento.getFechaFinalizacionEfectiva().empty()) {
        json["fecha_finalizacion_efectiva"] = nullptr;
    } else {
        json["fecha_finalizacion_efectiva"] =
            tratamiento.getFechaFinalizacionEfectiva();
    }

    json["estado_tratamiento"] = tratamiento.getEstadoTratamiento();

    return json;
}

bool leerCuerpo(const std::string& texto, Tratamiento& datos,
                std::string& error) {
    nlohmann::json cuerpo;

    try {
        cuerpo = nlohmann::json::parse(texto);

        if (!cuerpo.is_object()) {
            error = "Se esperaba un objeto JSON";
            return false;
        }

        datos.setIdPaciente(cuerpo.value("id_paciente", 0LL));
        datos.setIdMedicamento(cuerpo.value("id_medicamento", 0LL));
        datos.setDosis(cuerpo.value("dosis", ""));
        datos.setFrecuencia(cuerpo.value("frecuencia", ""));
        datos.setFechaInicio(cuerpo.value("fecha_inicio", ""));
        datos.setFechaFin(cuerpo.value("fecha_fin", ""));

        if (cuerpo.contains("intervalo_horas") &&
            !cuerpo["intervalo_horas"].is_null()) {
            datos.setIntervaloHoras(
                cuerpo["intervalo_horas"].get<int>());
        } else {
            datos.quitarIntervaloHoras();
        }

    } catch (const std::exception&) {
        error = "JSON inválido o campos con tipos incorrectos";
        return false;
    }

    error.clear();
    return true;
}

} // namespace

TratamientoRoutes::TratamientoRoutes(TratamientoService& tratamientoService)
    : tratamientoService_(tratamientoService) {
}

void TratamientoRoutes::registrarRutas(
    crow::App<crow::CORSHandler>& app) {

    // Consultar todos los tratamientos.
    CROW_ROUTE(app, "/api/tratamientos")
        .methods(crow::HTTPMethod::Get)(
            [this](const crow::request& peticion) {

        std::string correo;
        std::string rol;

        if (!ApiHttp::peticionAutorizada(peticion, correo, rol)) {
            return ApiHttp::respuestaError(401, "Token ausente o inválido");
        }

        std::vector<Tratamiento> tratamientos;
        std::string mensaje;

        const ResultadoOperacion resultado =
            tratamientoService_.listar(tratamientos, mensaje);

        if (resultado != ResultadoOperacion::Exito) {
            return ApiHttp::respuestaError(
                ApiHttp::codigoHttpDe(resultado), mensaje);
        }

        nlohmann::json lista = nlohmann::json::array();

        for (const Tratamiento& tratamiento : tratamientos) {
            lista.push_back(aJson(tratamiento));
        }

        nlohmann::json cuerpo;
        cuerpo["success"] = true;
        cuerpo["total"] = lista.size();
        cuerpo["tratamientos"] = lista;

        return ApiHttp::respuestaJson(200, cuerpo.dump());
    });

    // Consultar tratamientos de un paciente.
    CROW_ROUTE(app, "/api/tratamientos/paciente/<int>")
        .methods(crow::HTTPMethod::Get)(
            [this](const crow::request& peticion, int idPaciente) {

        std::string correo;
        std::string rol;

        if (!ApiHttp::peticionAutorizada(peticion, correo, rol)) {
            return ApiHttp::respuestaError(401, "Token ausente o inválido");
        }

        std::vector<Tratamiento> tratamientos;
        std::string mensaje;

        const ResultadoOperacion resultado =
            tratamientoService_.listarPorPaciente(
                idPaciente, tratamientos, mensaje);

        if (resultado != ResultadoOperacion::Exito) {
            return ApiHttp::respuestaError(
                ApiHttp::codigoHttpDe(resultado), mensaje);
        }

        nlohmann::json lista = nlohmann::json::array();

        for (const Tratamiento& tratamiento : tratamientos) {
            lista.push_back(aJson(tratamiento));
        }

        nlohmann::json cuerpo;
        cuerpo["success"] = true;
        cuerpo["total"] = lista.size();
        cuerpo["tratamientos"] = lista;

        return ApiHttp::respuestaJson(200, cuerpo.dump());
    });

    // Registrar un tratamiento.
    CROW_ROUTE(app, "/api/tratamientos")
        .methods(crow::HTTPMethod::Post)(
            [this](const crow::request& peticion) {

        std::string correo;
        std::string rol;

        if (!ApiHttp::peticionAutorizada(peticion, correo, rol)) {
            return ApiHttp::respuestaError(401, "Token ausente o inválido");
        }

        Tratamiento datos;
        std::string error;

        if (!leerCuerpo(peticion.body, datos, error)) {
            return ApiHttp::respuestaError(400, error);
        }

        Tratamiento registrado;
        std::string mensaje;

        const ResultadoOperacion resultado =
            tratamientoService_.registrar(
                datos, correo, registrado, mensaje);

        if (resultado != ResultadoOperacion::Exito) {
            return ApiHttp::respuestaError(
                ApiHttp::codigoHttpDe(resultado), mensaje);
        }

        nlohmann::json cuerpo;
        cuerpo["success"] = true;
        cuerpo["message"] = mensaje;
        cuerpo["tratamiento"] = aJson(registrado);

        return ApiHttp::respuestaJson(201, cuerpo.dump());
    });

    // Actualizar los datos de un tratamiento.
    CROW_ROUTE(app, "/api/tratamientos/<int>")
        .methods(crow::HTTPMethod::Put)(
            [this](const crow::request& peticion, int idTratamiento) {

        std::string correo;
        std::string rol;

        if (!ApiHttp::peticionAutorizada(peticion, correo, rol)) {
            return ApiHttp::respuestaError(401, "Token ausente o inválido");
        }

        Tratamiento datos;
        std::string error;

        if (!leerCuerpo(peticion.body, datos, error)) {
            return ApiHttp::respuestaError(400, error);
        }

        datos.setIdTratamiento(idTratamiento);

        std::string mensaje;

        const ResultadoOperacion resultado =
            tratamientoService_.actualizar(datos, mensaje);

        if (resultado != ResultadoOperacion::Exito) {
            return ApiHttp::respuestaError(
                ApiHttp::codigoHttpDe(resultado), mensaje);
        }

        nlohmann::json cuerpo;
        cuerpo["success"] = true;
        cuerpo["message"] = mensaje;

        return ApiHttp::respuestaJson(200, cuerpo.dump());
    });

    // Cambiar el estado de un tratamiento.
    CROW_ROUTE(app, "/api/tratamientos/<int>/estado")
        .methods(crow::HTTPMethod::Patch)(
            [this](const crow::request& peticion, int idTratamiento) {

        std::string correo;
        std::string rol;

        if (!ApiHttp::peticionAutorizada(peticion, correo, rol)) {
            return ApiHttp::respuestaError(401, "Token ausente o inválido");
        }

        try {
            const auto cuerpo = nlohmann::json::parse(peticion.body);

            if (!cuerpo.is_object()) {
                return ApiHttp::respuestaError(
                    400, "Se esperaba un objeto JSON");
            }

            const std::string estado =
                cuerpo.value("estado_tratamiento", "");

            const std::string fechaFinalizacion =
                cuerpo.value("fecha_finalizacion_efectiva", "");

            std::string mensaje;

            const ResultadoOperacion resultado =
                tratamientoService_.cambiarEstado(
                    idTratamiento, estado, fechaFinalizacion, mensaje);

            if (resultado != ResultadoOperacion::Exito) {
                return ApiHttp::respuestaError(
                    ApiHttp::codigoHttpDe(resultado), mensaje);
            }

            nlohmann::json respuesta;
            respuesta["success"] = true;
            respuesta["message"] = mensaje;

            return ApiHttp::respuestaJson(200, respuesta.dump());

        } catch (const std::exception&) {
            return ApiHttp::respuestaError(400, "JSON inválido");
        }
    });
}