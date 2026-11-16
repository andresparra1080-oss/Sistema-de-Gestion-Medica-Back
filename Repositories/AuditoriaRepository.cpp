#include "Repositories/AuditoriaRepository.h"

#include <sqlite3.h>

#include <memory>

#include "Cross/Config.h"

namespace {

// Cierre automático de la conexión y de las sentencias.
struct CierreConexion {
    void operator()(sqlite3* conexion) const {
        if (conexion != nullptr) {
            sqlite3_close(conexion);
        }
    }
};

struct CierreSentencia {
    void operator()(sqlite3_stmt* sentencia) const {
        if (sentencia != nullptr) {
            sqlite3_finalize(sentencia);
        }
    }
};

using Conexion = std::unique_ptr<sqlite3, CierreConexion>;
using Sentencia = std::unique_ptr<sqlite3_stmt, CierreSentencia>;

std::string leerTexto(sqlite3_stmt* sentencia, int columna) {
    const unsigned char* texto = sqlite3_column_text(sentencia, columna);
    return texto == nullptr ? std::string() : std::string(reinterpret_cast<const char*>(texto));
}

// ?1 = id del paciente. Los valores de tabla_afectada son los que escriben los
// repositorios al registrar un cambio en logs_auditoria.
const char* const SQL_AUDITORIA_PACIENTE =
    "SELECT l.id_log, l.id_usuario, u.nombre, l.accion, l.tabla_afectada, "
    "       l.id_registro, l.detalle_cambio, l.fecha_modificacion "
    "  FROM logs_auditoria l "
    "  JOIN usuarios u ON u.id_usuario = l.id_usuario "
    " WHERE (l.tabla_afectada = 'pacientes' AND l.id_registro = ?1) "
    "    OR (l.tabla_afectada = 'historias_clinicas' "
    "        AND l.id_registro IN (SELECT id_historia FROM historias_clinicas WHERE id_paciente = ?1)) "
    "    OR (l.tabla_afectada = 'eventos_medicos' "
    "        AND l.id_registro IN (SELECT e.id_evento FROM eventos_medicos e "
    "                                JOIN historias_clinicas h ON h.id_historia = e.id_historia "
    "                               WHERE h.id_paciente = ?1)) "
    "    OR (l.tabla_afectada = 'controles_evolucion' "
    "        AND l.id_registro IN (SELECT id_control FROM controles_evolucion WHERE id_paciente = ?1)) "
    "    OR (l.tabla_afectada = 'tratamientos' "
    "        AND l.id_registro IN (SELECT id_tratamiento FROM tratamientos WHERE id_paciente = ?1)) "
    "    OR (l.tabla_afectada = 'emergencias' "
    "        AND l.id_registro IN (SELECT id_emergencia FROM emergencia_paciente WHERE id_paciente = ?1)) "
    "    OR (l.tabla_afectada = 'acciones_emergencia' "
    "        AND l.id_registro IN (SELECT a.id_accion FROM acciones_emergencia a "
    "                                JOIN emergencia_paciente ep ON ep.id_emergencia = a.id_emergencia "
    "                               WHERE ep.id_paciente = ?1)) "
    " ORDER BY l.fecha_modificacion DESC, l.id_log DESC "
    " LIMIT 200;";

}  // namespace

ResultadoOperacion AuditoriaRepository::listarPorPaciente(long long idPaciente,
                                                          std::vector<RegistroAuditoria>& registros,
                                                          std::string& error) const {
    registros.clear();

    // Conexión propia de solo lectura sobre el mismo archivo que usa el resto del backend.
    const std::string ruta =
        Configuracion::instancia().obtenerCadena("DB_PATH", "./Database/heartmed_db.sqlite3");

    sqlite3* conexionCruda = nullptr;
    if (sqlite3_open_v2(ruta.c_str(), &conexionCruda, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
        error = "No se pudo abrir la base de datos para consultar la auditoría";
        if (conexionCruda != nullptr) {
            sqlite3_close(conexionCruda);
        }
        return ResultadoOperacion::ErrorInterno;
    }
    Conexion conexion(conexionCruda);
    sqlite3_busy_timeout(conexion.get(), 3000);

    // El paciente tiene que existir.
    {
        sqlite3_stmt* cruda = nullptr;
        if (sqlite3_prepare_v2(conexion.get(), "SELECT 1 FROM pacientes WHERE id_paciente = ?;", -1,
                               &cruda, nullptr) != SQLITE_OK) {
            error = "No se pudo consultar el paciente";
            return ResultadoOperacion::ErrorInterno;
        }
        Sentencia existe(cruda);
        sqlite3_bind_int64(existe.get(), 1, idPaciente);

        const int paso = sqlite3_step(existe.get());
        if (paso == SQLITE_DONE) {
            error = "El paciente no existe";
            return ResultadoOperacion::NoEncontrado;
        }
        if (paso != SQLITE_ROW) {
            error = "No se pudo consultar el paciente";
            return ResultadoOperacion::ErrorInterno;
        }
    }

    sqlite3_stmt* cruda = nullptr;
    if (sqlite3_prepare_v2(conexion.get(), SQL_AUDITORIA_PACIENTE, -1, &cruda, nullptr) != SQLITE_OK) {
        error = "No se pudo preparar la consulta de auditoría";
        return ResultadoOperacion::ErrorInterno;
    }
    Sentencia sentencia(cruda);
    sqlite3_bind_int64(sentencia.get(), 1, idPaciente);

    int paso = SQLITE_ROW;
    while ((paso = sqlite3_step(sentencia.get())) == SQLITE_ROW) {
        RegistroAuditoria registro;
        registro.idLog = sqlite3_column_int64(sentencia.get(), 0);
        registro.idUsuario = sqlite3_column_int64(sentencia.get(), 1);
        registro.nombreUsuario = leerTexto(sentencia.get(), 2);
        registro.accion = leerTexto(sentencia.get(), 3);
        registro.tablaAfectada = leerTexto(sentencia.get(), 4);
        if (sqlite3_column_type(sentencia.get(), 5) != SQLITE_NULL) {
            registro.idRegistro = sqlite3_column_int64(sentencia.get(), 5);
        }
        registro.detalleCambio = leerTexto(sentencia.get(), 6);
        registro.fechaModificacion = leerTexto(sentencia.get(), 7);
        registros.push_back(registro);
    }

    if (paso != SQLITE_DONE) {
        registros.clear();
        error = "No se pudo leer el historial de auditoría";
        return ResultadoOperacion::ErrorInterno;
    }

    error.clear();
    return ResultadoOperacion::Exito;
}