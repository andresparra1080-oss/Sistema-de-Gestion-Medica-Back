#include "Repositories/GestionUsuarioRepository.h"

#include <sqlite3.h>

#include <string>
#include <vector>

#include "Database/SQLite.h"

namespace {

std::string leerTexto(sqlite3_stmt* sentencia, int columna) {
    const unsigned char* valor = sqlite3_column_text(sentencia, columna);
    return valor == nullptr ? "" : reinterpret_cast<const char*>(valor);
}

Usuario leerUsuario(sqlite3_stmt* sentencia) {
    Usuario usuario;
    usuario.idUsuario = sqlite3_column_int64(sentencia, 0);
    usuario.idRol = sqlite3_column_int64(sentencia, 1);
    usuario.nombreRol = leerTexto(sentencia, 2);
    usuario.nombre = leerTexto(sentencia, 3);
    usuario.correo = leerTexto(sentencia, 4);
    usuario.activo = sqlite3_column_int(sentencia, 5) == 1;
    usuario.fechaCreacion = leerTexto(sentencia, 6);
    return usuario;
}

bool ejecutar(sqlite3* conexion, const char* sql) {
    return sqlite3_exec(conexion, sql, nullptr, nullptr, nullptr) == SQLITE_OK;
}

void revertirTransaccion(sqlite3* conexion) {
    sqlite3_exec(conexion, "ROLLBACK;", nullptr, nullptr, nullptr);
}

}  // namespace

bool GestionUsuarioRepository::listar(std::vector<Usuario>& usuarios, std::string& error) const {
    sqlite3* conexion = BaseDatosSqlite::instancia().obtenerConexion();
    if (conexion == nullptr || !BaseDatosSqlite::instancia().estaAbierta()) {
        error = "La base de datos no está disponible";
        return false;
    }

    const char* sql =
        "SELECT u.id_usuario, u.id_rol, r.nombre_rol, u.nombre, u.correo, u.activo, u.fecha_creacion "
        "FROM usuarios u JOIN roles r ON r.id_rol = u.id_rol "
        "ORDER BY u.id_usuario;";

    sqlite3_stmt* sentencia = nullptr;
    if (sqlite3_prepare_v2(conexion, sql, -1, &sentencia, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(conexion);
        return false;
    }

    usuarios.clear();
    int resultado = SQLITE_OK;
    while ((resultado = sqlite3_step(sentencia)) == SQLITE_ROW) {
        usuarios.push_back(leerUsuario(sentencia));
    }

    if (resultado != SQLITE_DONE) {
        error = sqlite3_errmsg(conexion);
        sqlite3_finalize(sentencia);
        usuarios.clear();
        return false;
    }

    sqlite3_finalize(sentencia);
    error.clear();
    return true;
}

bool GestionUsuarioRepository::existeRol(long long idRol, std::string& error) const {
    sqlite3* conexion = BaseDatosSqlite::instancia().obtenerConexion();
    if (conexion == nullptr || !BaseDatosSqlite::instancia().estaAbierta()) {
        error = "La base de datos no está disponible";
        return false;
    }

    const char* sql = "SELECT 1 FROM roles WHERE id_rol = ? LIMIT 1;";
    sqlite3_stmt* sentencia = nullptr;
    if (sqlite3_prepare_v2(conexion, sql, -1, &sentencia, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(conexion);
        return false;
    }

    sqlite3_bind_int64(sentencia, 1, idRol);
    const int resultado = sqlite3_step(sentencia);
    sqlite3_finalize(sentencia);

    if (resultado == SQLITE_ROW) {
        error.clear();
        return true;
    }
    if (resultado == SQLITE_DONE) {
        error.clear();
        return false;
    }

    error = sqlite3_errmsg(conexion);
    return false;
}

ResultadoCreacionUsuario GestionUsuarioRepository::crear(const Usuario& usuario,
                                                         const std::string& hashContrasena,
                                                         long long idUsuarioCreador,
                                                         Usuario& creado,
                                                         std::string& error) const {
    sqlite3* conexion = BaseDatosSqlite::instancia().obtenerConexion();
    if (conexion == nullptr || !BaseDatosSqlite::instancia().estaAbierta()) {
        error = "La base de datos no está disponible";
        return ResultadoCreacionUsuario::Error;
    }

    if (!ejecutar(conexion, "BEGIN IMMEDIATE;")) {
        error = sqlite3_errmsg(conexion);
        return ResultadoCreacionUsuario::Error;
    }

    const char* sqlUsuario =
        "INSERT INTO usuarios (id_rol, nombre, correo, contrasena_hash) VALUES (?, ?, ?, ?);";
    sqlite3_stmt* sentenciaUsuario = nullptr;
    if (sqlite3_prepare_v2(conexion, sqlUsuario, -1, &sentenciaUsuario, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(conexion);
        revertirTransaccion(conexion);
        return ResultadoCreacionUsuario::Error;
    }

    sqlite3_bind_int64(sentenciaUsuario, 1, usuario.idRol);
    sqlite3_bind_text(sentenciaUsuario, 2, usuario.nombre.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(sentenciaUsuario, 3, usuario.correo.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(sentenciaUsuario, 4, hashContrasena.c_str(), -1, SQLITE_TRANSIENT);

    const int resultadoUsuario = sqlite3_step(sentenciaUsuario);
    sqlite3_finalize(sentenciaUsuario);
    if (resultadoUsuario != SQLITE_DONE) {
        const int codigoExtendido = sqlite3_extended_errcode(conexion);
        error = sqlite3_errmsg(conexion);
        revertirTransaccion(conexion);
        return codigoExtendido == SQLITE_CONSTRAINT_UNIQUE ||
                       codigoExtendido == SQLITE_CONSTRAINT_PRIMARYKEY
                   ? ResultadoCreacionUsuario::CorreoDuplicado
                   : ResultadoCreacionUsuario::Error;
    }

    const long long idGenerado = sqlite3_last_insert_rowid(conexion);
    const std::string detalle =
        "Creó el usuario " + usuario.correo + " con rol " + std::to_string(usuario.idRol) + ".";
    const char* sqlAuditoria =
        "INSERT INTO logs_auditoria "
        "(id_usuario, accion, tabla_afectada, id_registro, detalle_cambio) "
        "VALUES (?, 'INSERT', 'usuarios', ?, ?);";
    sqlite3_stmt* sentenciaAuditoria = nullptr;
    if (sqlite3_prepare_v2(conexion, sqlAuditoria, -1, &sentenciaAuditoria, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(conexion);
        revertirTransaccion(conexion);
        return ResultadoCreacionUsuario::Error;
    }

    sqlite3_bind_int64(sentenciaAuditoria, 1, idUsuarioCreador);
    sqlite3_bind_int64(sentenciaAuditoria, 2, idGenerado);
    sqlite3_bind_text(sentenciaAuditoria, 3, detalle.c_str(), -1, SQLITE_TRANSIENT);
    const bool auditoriaCreada = sqlite3_step(sentenciaAuditoria) == SQLITE_DONE;
    sqlite3_finalize(sentenciaAuditoria);
    if (!auditoriaCreada) {
        error = sqlite3_errmsg(conexion);
        revertirTransaccion(conexion);
        return ResultadoCreacionUsuario::Error;
    }

    const char* sqlUsuarioCreado =
        "SELECT u.id_usuario, u.id_rol, r.nombre_rol, u.nombre, u.correo, u.activo, u.fecha_creacion "
        "FROM usuarios u JOIN roles r ON r.id_rol = u.id_rol "
        "WHERE u.id_usuario = ?;";
    sqlite3_stmt* sentenciaUsuarioCreado = nullptr;
    if (sqlite3_prepare_v2(conexion, sqlUsuarioCreado, -1, &sentenciaUsuarioCreado, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(conexion);
        revertirTransaccion(conexion);
        return ResultadoCreacionUsuario::Error;
    }

    sqlite3_bind_int64(sentenciaUsuarioCreado, 1, idGenerado);
    if (sqlite3_step(sentenciaUsuarioCreado) != SQLITE_ROW) {
        error = sqlite3_errmsg(conexion);
        sqlite3_finalize(sentenciaUsuarioCreado);
        revertirTransaccion(conexion);
        return ResultadoCreacionUsuario::Error;
    }
    creado = leerUsuario(sentenciaUsuarioCreado);
    sqlite3_finalize(sentenciaUsuarioCreado);

    if (!ejecutar(conexion, "COMMIT;")) {
        error = sqlite3_errmsg(conexion);
        revertirTransaccion(conexion);
        return ResultadoCreacionUsuario::Error;
    }

    error.clear();
    return ResultadoCreacionUsuario::Exito;
}
