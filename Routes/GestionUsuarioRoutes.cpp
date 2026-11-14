#include "Routes/GestionUsuarioRoutes.h"

#include <nlohmann/json.hpp>

#include <exception>
#include <string>
#include <vector>

#include "Cross/ApiHttp.h"
#include "Repositories/UsuarioRepository.h"

namespace {

nlohmann::json aJson(const Usuario& usuario) {
    nlohmann::json json;
    json["id_usuario"] = usuario.idUsuario;
    json["id_rol"] = usuario.idRol;
    json["nombre_rol"] = usuario.nombreRol;
    json["nombre"] = usuario.nombre;
    json["correo"] = usuario.correo;
    json["activo"] = usuario.activo;
    json["fecha_creacion"] = usuario.fechaCreacion;
    return json;
}

}  // namespace

GestionUsuarioRoutes::GestionUsuarioRoutes(GestionUsuarioService& gestionUsuarioService)
    : gestionUsuarioService_(gestionUsuarioService) {
}

void GestionUsuarioRoutes::registrarRutas(crow::App<crow::CORSHandler>& app) {
    CROW_ROUTE(app, "/api/usuarios")
        .methods(crow::HTTPMethod::Get)([this](const crow::request& peticion) {
            std::string correo;
            std::string rol;
            if (!ApiHttp::peticionAutorizada(peticion, correo, rol)) {
                return ApiHttp::respuestaError(401, "Token ausente o inválido");
            }

            UsuarioRepository usuariosRepositorio;
            const long long idSolicitante = usuariosRepositorio.obtenerIdPorCorreo(correo);
            if (idSolicitante <= 0) {
                return ApiHttp::respuestaError(403, "La cuenta no está activa o no tiene permiso");
            }

            std::vector<Usuario> usuarios;
            std::string mensaje;
            const ResultadoOperacion resultado =
                gestionUsuarioService_.listar(idSolicitante, usuarios, mensaje);
            if (resultado != ResultadoOperacion::Exito) {
                return ApiHttp::respuestaError(ApiHttp::codigoHttpDe(resultado), mensaje);
            }

            nlohmann::json lista = nlohmann::json::array();
            for (const Usuario& usuario : usuarios) {
                lista.push_back(aJson(usuario));
            }

            nlohmann::json cuerpo;
            cuerpo["success"] = true;
            cuerpo["total"] = lista.size();
            cuerpo["usuarios"] = lista;
            return ApiHttp::respuestaJson(200, cuerpo.dump());
        });

    CROW_ROUTE(app, "/api/usuarios")
        .methods(crow::HTTPMethod::Post)([this](const crow::request& peticion) {
            std::string correoSolicitante;
            std::string rolSolicitante;
            if (!ApiHttp::peticionAutorizada(peticion, correoSolicitante, rolSolicitante)) {
                return ApiHttp::respuestaError(401, "Token ausente o inválido");
            }

            UsuarioRepository usuariosRepositorio;
            const long long idSolicitante = usuariosRepositorio.obtenerIdPorCorreo(correoSolicitante);
            if (idSolicitante <= 0) {
                return ApiHttp::respuestaError(403, "La cuenta no está activa o no tiene permiso");
            }
            if (idSolicitante != 1) {
                return ApiHttp::respuestaError(403, "Acceso denegado: solo el usuario 1 puede crear usuarios");
            }

            nlohmann::json cuerpoSolicitud;
            try {
                cuerpoSolicitud = nlohmann::json::parse(peticion.body);
            } catch (const std::exception&) {
                return ApiHttp::respuestaError(400, "JSON inválido");
            }
            if (!cuerpoSolicitud.is_object()) {
                return ApiHttp::respuestaError(400, "Se esperaba un objeto JSON");
            }
            if ((cuerpoSolicitud.contains("nombre") && !cuerpoSolicitud["nombre"].is_string()) ||
                (cuerpoSolicitud.contains("correo") && !cuerpoSolicitud["correo"].is_string()) ||
                (cuerpoSolicitud.contains("contrasena") && !cuerpoSolicitud["contrasena"].is_string()) ||
                (cuerpoSolicitud.contains("id_rol") && !cuerpoSolicitud["id_rol"].is_number_integer())) {
                return ApiHttp::respuestaError(400, "Alguno de los campos tiene un tipo de dato incorrecto");
            }

            Usuario datos;
            std::string contrasena;
            try {
                datos.nombre = cuerpoSolicitud.value("nombre", "");
                datos.correo = cuerpoSolicitud.value("correo", "");
                contrasena = cuerpoSolicitud.value("contrasena", "");
                datos.idRol = cuerpoSolicitud.value("id_rol", 0LL);
            } catch (const std::exception&) {
                return ApiHttp::respuestaError(400, "Alguno de los campos tiene un tipo de dato incorrecto");
            }

            Usuario creado;
            std::string mensaje;
            const ResultadoOperacion resultado =
                gestionUsuarioService_.crear(idSolicitante, datos, contrasena, creado, mensaje);
            if (resultado != ResultadoOperacion::Exito) {
                return ApiHttp::respuestaError(ApiHttp::codigoHttpDe(resultado), mensaje);
            }

            nlohmann::json respuesta;
            respuesta["success"] = true;
            respuesta["message"] = mensaje;
            respuesta["usuario"] = aJson(creado);
            return ApiHttp::respuestaJson(201, respuesta.dump());
        });
}
