#include "Repositories/AlertaRepository.h"

#include <mutex>
#include <string>
#include <vector>

#include "Database/SQLite.h"

namespace {

std::mutex mutexEscrituras;

std::string leerTexto(sqlite3_stmt* sentencia, int columna) {
    const unsigned char* valor = sqlite3_column_text(sentencia, columna);
    if (valor == nullptr) {
        return "";
    }
    return reinterpret_cast<const char*>(valor);
}

std::optional<long long> leerEnteroONulo(sqlite3_stmt* sentencia, int columna) {
    if (sqlite3_column_type(sentencia, columna) == SQLITE_NULL) {
        return std::nullopt;
    }
    return sqlite3_column_int64(sentencia, columna);
}

bool ejecutar(sqlite3* conexion, const char* sql) {
    return sqlite3_exec(conexion, sql, nullptr, nullptr, nullptr) == SQLITE_OK;
}

bool preparar(sqlite3* conexion, const char* sql, sqlite3_stmt** sentencia, std::string& error) {
    if (sqlite3_prepare_v2(conexion, sql, -1, sentencia, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(conexion);
        return false;
    }
    return true;
}

// Ejecuta una sentencia que no devuelve filas y la libera.
bool terminar(sqlite3* conexion, sqlite3_stmt* sentencia, std::string& error) {
    const bool bien = sqlite3_step(sentencia) == SQLITE_DONE;
    if (!bien) {
        error = sqlite3_errmsg(conexion);
    }
    sqlite3_finalize(sentencia);
    return bien;
}

sqlite3* conexionAbierta() {
    if (!BaseDatosSqlite::instancia().estaAbierta()) {
        return nullptr;
    }
    return BaseDatosSqlite::instancia().obtenerConexion();
}

// Bitácora: una fila en logs_auditoria. idRegistro <= 0 se guarda como NULL.
// Se llama DENTRO de la transacción del cambio.
bool registrarAuditoria(sqlite3* conexion,
                        long long idUsuario,
                        long long idRegistro,
                        const std::string& detalle,
                        std::string& error) {
    sqlite3_stmt* sentencia = nullptr;
    if (!preparar(conexion,
                  "INSERT INTO logs_auditoria (id_usuario, accion, tabla_afectada, id_registro, detalle_cambio) "
                  "VALUES (?, 'UPDATE', 'alertas', ?, ?);",
                  &sentencia, error)) {
        return false;
    }
    sqlite3_bind_int64(sentencia, 1, idUsuario);
    if (idRegistro > 0) {
        sqlite3_bind_int64(sentencia, 2, idRegistro);
    } else {
        sqlite3_bind_null(sentencia, 2);
    }
    sqlite3_bind_text(sentencia, 3, detalle.c_str(), static_cast<int>(detalle.size()), SQLITE_TRANSIENT);
    return terminar(conexion, sentencia, error);
}

}  // namespace

ResultadoOperacion AlertaRepository::listar(bool soloSinResolver,
                                            std::vector<AlertaSistema>& alertas,
                                            std::string& error) const {
    sqlite3* conexion = conexionAbierta();
    if (conexion == nullptr) {
        error = "La base de datos no está disponible";
        return ResultadoOperacion::ErrorInterno;
    }

    // El medicamento sale de la alerta (stock bajo) o, si es de un lote
    // (vencimiento próximo), del medicamento del lote.
    const std::string sql =
        "SELECT a.id_alerta, a.id_medicamento, a.id_lote, a.id_tratamiento, a.id_emergencia, "
        "a.tipo_alerta, a.mensaje, a.prioridad, a.notificada, a.resuelta, "
        "a.fecha_generacion, a.fecha_resolucion, "
        "COALESCE(m.nombre, ml.nombre, ''), COALESCE(l.num_lote, '') "
        "FROM alertas a "
        "LEFT JOIN medicamentos m ON m.id_medicamento = a.id_medicamento "
        "LEFT JOIN lotes l ON l.id_lote = a.id_lote "
        "LEFT JOIN medicamentos ml ON ml.id_medicamento = l.id_medicamento " +
        std::string(soloSinResolver ? "WHERE a.resuelta = 0 " : "") +
        "ORDER BY a.fecha_generacion DESC, a.id_alerta DESC;";

    sqlite3_stmt* sentencia = nullptr;
    if (!preparar(conexion, sql.c_str(), &sentencia, error)) {
        return ResultadoOperacion::ErrorInterno;
    }

    alertas.clear();
    while (sqlite3_step(sentencia) == SQLITE_ROW) {
        AlertaSistema alerta;
        alerta.idAlerta = sqlite3_column_int64(sentencia, 0);
        alerta.idMedicamento = leerEnteroONulo(sentencia, 1);
        alerta.idLote = leerEnteroONulo(sentencia, 2);
        alerta.idTratamiento = leerEnteroONulo(sentencia, 3);
        alerta.idEmergencia = leerEnteroONulo(sentencia, 4);
        alerta.tipoAlerta = leerTexto(sentencia, 5);
        alerta.mensaje = leerTexto(sentencia, 6);
        alerta.prioridad = leerTexto(sentencia, 7);
        alerta.notificada = sqlite3_column_int(sentencia, 8) != 0;
        alerta.resuelta = sqlite3_column_int(sentencia, 9) != 0;
        alerta.fechaGeneracion = leerTexto(sentencia, 10);
        if (sqlite3_column_type(sentencia, 11) != SQLITE_NULL) {
            alerta.fechaResolucion = leerTexto(sentencia, 11);
        }
        alerta.nombreMedicamento = leerTexto(sentencia, 12);
        alerta.numLote = leerTexto(sentencia, 13);
        alertas.push_back(alerta);
    }
    sqlite3_finalize(sentencia);
    return ResultadoOperacion::Exito;
}

ResultadoOperacion AlertaRepository::marcarLeida(long long idAlerta,
                                                 long long idUsuario,
                                                 std::string& error) const {
    sqlite3* conexion = conexionAbierta();
    if (conexion == nullptr) {
        error = "La base de datos no está disponible";
        return ResultadoOperacion::ErrorInterno;
    }

    std::lock_guard<std::mutex> candado(mutexEscrituras);

    if (!ejecutar(conexion, "BEGIN IMMEDIATE;")) {
        error = sqlite3_errmsg(conexion);
        return ResultadoOperacion::ErrorInterno;
    }

    auto fallar = [&]() {
        ejecutar(conexion, "ROLLBACK;");
        return ResultadoOperacion::ErrorInterno;
    };

    sqlite3_stmt* sentencia = nullptr;
    if (!preparar(conexion, "UPDATE alertas SET notificada = 1 WHERE id_alerta = ? AND notificada = 0;",
                  &sentencia, error)) {
        return fallar();
    }
    sqlite3_bind_int64(sentencia, 1, idAlerta);
    if (!terminar(conexion, sentencia, error)) {
        return fallar();
    }
    const int cambiadas = sqlite3_changes(conexion);

    if (cambiadas == 0) {
        // O no existe, o ya estaba leída.
        if (!preparar(conexion, "SELECT 1 FROM alertas WHERE id_alerta = ?;", &sentencia, error)) {
            return fallar();
        }
        sqlite3_bind_int64(sentencia, 1, idAlerta);
        const bool existe = sqlite3_step(sentencia) == SQLITE_ROW;
        sqlite3_finalize(sentencia);

        ejecutar(conexion, "ROLLBACK;");
        if (!existe) {
            error = "La alerta no existe";
            return ResultadoOperacion::NoEncontrado;
        }
        return ResultadoOperacion::Exito;  // ya estaba leída
    }

    if (!registrarAuditoria(conexion, idUsuario, idAlerta,
                            "Marcó la alerta " + std::to_string(idAlerta) + " como leída.", error)) {
        return fallar();
    }

    if (!ejecutar(conexion, "COMMIT;")) {
        error = sqlite3_errmsg(conexion);
        return fallar();
    }
    return ResultadoOperacion::Exito;
}

ResultadoOperacion AlertaRepository::marcarTodasLeidas(long long idUsuario,
                                                       int& cantidad,
                                                       std::string& error) const {
    cantidad = 0;

    sqlite3* conexion = conexionAbierta();
    if (conexion == nullptr) {
        error = "La base de datos no está disponible";
        return ResultadoOperacion::ErrorInterno;
    }

    std::lock_guard<std::mutex> candado(mutexEscrituras);

    if (!ejecutar(conexion, "BEGIN IMMEDIATE;")) {
        error = sqlite3_errmsg(conexion);
        return ResultadoOperacion::ErrorInterno;
    }

    auto fallar = [&]() {
        ejecutar(conexion, "ROLLBACK;");
        return ResultadoOperacion::ErrorInterno;
    };

    sqlite3_stmt* sentencia = nullptr;
    if (!preparar(conexion, "UPDATE alertas SET notificada = 1 WHERE notificada = 0 AND resuelta = 0;",
                  &sentencia, error)) {
        return fallar();
    }
    if (!terminar(conexion, sentencia, error)) {
        return fallar();
    }
    const int cambiadas = sqlite3_changes(conexion);

    if (cambiadas > 0 &&
        !registrarAuditoria(conexion, idUsuario, 0,
                            "Marcó " + std::to_string(cambiadas) + " alertas como leídas.", error)) {
        return fallar();
    }

    if (!ejecutar(conexion, "COMMIT;")) {
        error = sqlite3_errmsg(conexion);
        return fallar();
    }

    cantidad = cambiadas;
    return ResultadoOperacion::Exito;
}