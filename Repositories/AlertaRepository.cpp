#include "Repositories/AlertaRepository.h"

#include <sqlite3.h>
#include "Database/SQLite.h"

namespace {

Alerta mapearFila(sqlite3_stmt* stmt) {
    Alerta alerta;
    alerta.idAlerta = sqlite3_column_int64(stmt, 0);

    if (sqlite3_column_type(stmt, 1) != SQLITE_NULL) {
        alerta.idMedicamento = sqlite3_column_int64(stmt, 1);
    }
    if (sqlite3_column_type(stmt, 2) != SQLITE_NULL) {
        alerta.idLote = sqlite3_column_int64(stmt, 2);
    }
    if (sqlite3_column_type(stmt, 3) != SQLITE_NULL) {
        alerta.idTratamiento = sqlite3_column_int64(stmt, 3);
    }
    if (sqlite3_column_type(stmt, 4) != SQLITE_NULL) {
        alerta.idEmergencia = sqlite3_column_int64(stmt, 4);
    }

    const unsigned char* tipo = sqlite3_column_text(stmt, 5);
    alerta.tipoAlerta = tipo ? reinterpret_cast<const char*>(tipo) : "";

    const unsigned char* msg = sqlite3_column_text(stmt, 6);
    alerta.mensaje = msg ? reinterpret_cast<const char*>(msg) : "";

    const unsigned char* prio = sqlite3_column_text(stmt, 7);
    alerta.prioridad = prio ? reinterpret_cast<const char*>(prio) : "";

    alerta.notificada = sqlite3_column_int(stmt, 8) == 1;
    alerta.resuelta = sqlite3_column_int(stmt, 9) == 1;

    const unsigned char* fGen = sqlite3_column_text(stmt, 10);
    alerta.fechaGeneracion = fGen ? reinterpret_cast<const char*>(fGen) : "";

    if (sqlite3_column_type(stmt, 11) != SQLITE_NULL) {
        const unsigned char* fRes = sqlite3_column_text(stmt, 11);
        if (fRes) {
            alerta.fechaResolucion = reinterpret_cast<const char*>(fRes);
        }
    }

    return alerta;
}

bool consultarPorId(sqlite3* db, long long idAlerta, Alerta& salida) {
    const char* sql =
        "SELECT id_alerta, id_medicamento, id_lote, id_tratamiento, id_emergencia, "
        "tipo_alerta, mensaje, prioridad, notificada, resuelta, fecha_generacion, fecha_resolucion "
        "FROM alertas WHERE id_alerta = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, idAlerta);
    bool encontrado = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        salida = mapearFila(stmt);
        encontrado = true;
    }

    sqlite3_finalize(stmt);
    return encontrado;
}

bool registrarAuditoria(sqlite3* db,
                        long long idUsuario,
                        long long idRegistro,
                        bool tieneRegistro,
                        const std::string& detalle) {
    const char* sql =
        "INSERT INTO logs_auditoria (id_usuario, accion, tabla_afectada, id_registro, detalle_cambio, fecha_modificacion) "
        "VALUES (?, 'UPDATE', 'alertas', ?, ?, datetime('now', '-5 hours'));";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, idUsuario);
    if (tieneRegistro) {
        sqlite3_bind_int64(stmt, 2, idRegistro);
    } else {
        sqlite3_bind_null(stmt, 2);
    }
    sqlite3_bind_text(stmt, 3, detalle.c_str(), -1, SQLITE_TRANSIENT);

    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

}  // namespace

bool AlertaRepository::listar(const std::optional<int>& resueltaFiltro,
                              const std::string& tipoFiltro,
                              std::vector<Alerta>& alertas,
                              std::string& error) const {
    sqlite3* db = BaseDatosSqlite::instancia().obtenerConexion();
    if (!db) {
        error = "No hay conexión activa con la base de datos";
        return false;
    }

    std::string sql =
        "SELECT id_alerta, id_medicamento, id_lote, id_tratamiento, id_emergencia, "
        "tipo_alerta, mensaje, prioridad, notificada, resuelta, fecha_generacion, fecha_resolucion "
        "FROM alertas WHERE 1=1";

    if (resueltaFiltro.has_value()) {
        sql += " AND resuelta = ?";
    }
    if (!tipoFiltro.empty()) {
        sql += " AND tipo_alerta = ?";
    }
    sql += " ORDER BY resuelta ASC, fecha_generacion DESC, id_alerta DESC;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(db);
        return false;
    }

    int indice = 1;
    if (resueltaFiltro.has_value()) {
        sqlite3_bind_int(stmt, indice++, *resueltaFiltro);
    }
    if (!tipoFiltro.empty()) {
        sqlite3_bind_text(stmt, indice++, tipoFiltro.c_str(), -1, SQLITE_TRANSIENT);
    }

    alertas.clear();
    int rc = SQLITE_OK;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        alertas.push_back(mapearFila(stmt));
    }

    if (rc != SQLITE_DONE) {
        error = sqlite3_errmsg(db);
        sqlite3_finalize(stmt);
        return false;
    }

    sqlite3_finalize(stmt);
    error.clear();
    return true;
}

ResultadoOperacion AlertaRepository::marcarLeida(long long idAlerta,
                                                 long long idUsuario,
                                                 Alerta& actualizada,
                                                 std::string& error) const {
    sqlite3* db = BaseDatosSqlite::instancia().obtenerConexion();
    if (!db) {
        error = "No hay conexión activa con la base de datos";
        return ResultadoOperacion::ErrorInterno;
    }

    Alerta existente;
    if (!consultarPorId(db, idAlerta, existente)) {
        error = "No existe la alerta solicitada";
        return ResultadoOperacion::NoEncontrado;
    }

    if (existente.resuelta) {
        error = "La alerta ya fue marcada como leída";
        return ResultadoOperacion::Conflicto;
    }

    if (sqlite3_exec(db, "BEGIN IMMEDIATE TRANSACTION;", nullptr, nullptr, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(db);
        return ResultadoOperacion::ErrorInterno;
    }

    const char* sqlUpdate =
        "UPDATE alertas "
        "SET notificada = 1, resuelta = 1, fecha_resolucion = datetime('now', '-5 hours') "
        "WHERE id_alerta = ? AND resuelta = 0;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sqlUpdate, -1, &stmt, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(db);
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return ResultadoOperacion::ErrorInterno;
    }

    sqlite3_bind_int64(stmt, 1, idAlerta);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        error = sqlite3_errmsg(db);
        sqlite3_finalize(stmt);
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return ResultadoOperacion::ErrorInterno;
    }
    sqlite3_finalize(stmt);

    const std::string detalle = "Marcó como leída la alerta " + std::to_string(idAlerta) +
                                " (" + existente.tipoAlerta + ").";
    if (!registrarAuditoria(db, idUsuario, idAlerta, true, detalle)) {
        error = sqlite3_errmsg(db);
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return ResultadoOperacion::ErrorInterno;
    }

    if (sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(db);
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return ResultadoOperacion::ErrorInterno;
    }

    consultarPorId(db, idAlerta, actualizada);
    error.clear();
    return ResultadoOperacion::Exito;
}

ResultadoOperacion AlertaRepository::marcarTodasLeidas(long long idUsuario,
                                                       int& cantidadActualizada,
                                                       std::string& error) const {
    sqlite3* db = BaseDatosSqlite::instancia().obtenerConexion();
    if (!db) {
        error = "No hay conexión activa con la base de datos";
        return ResultadoOperacion::ErrorInterno;
    }

    if (sqlite3_exec(db, "BEGIN IMMEDIATE TRANSACTION;", nullptr, nullptr, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(db);
        return ResultadoOperacion::ErrorInterno;
    }

    const char* sqlUpdate =
        "UPDATE alertas "
        "SET notificada = 1, resuelta = 1, fecha_resolucion = datetime('now', '-5 hours') "
        "WHERE resuelta = 0;";

    if (sqlite3_exec(db, sqlUpdate, nullptr, nullptr, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(db);
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return ResultadoOperacion::ErrorInterno;
    }

    cantidadActualizada = sqlite3_changes(db);

    if (cantidadActualizada > 0) {
        const std::string detalle =
            "Marcó todas las alertas pendientes como leídas (" + std::to_string(cantidadActualizada) + ").";
        if (!registrarAuditoria(db, idUsuario, 0, false, detalle)) {
            error = sqlite3_errmsg(db);
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
            return ResultadoOperacion::ErrorInterno;
        }
    }

    if (sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(db);
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return ResultadoOperacion::ErrorInterno;
    }

    error.clear();
    return ResultadoOperacion::Exito;
}