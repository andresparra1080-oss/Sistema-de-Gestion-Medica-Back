#include "Repositories/InventarioRepository.h"

#include <map>
#include <string>
#include <vector>

#include "Database/SQLite.h"
#include "Models/Lote.h"

namespace {

std::string leerTexto(sqlite3_stmt* sentencia, int columna) {
    const unsigned char* valor = sqlite3_column_text(sentencia, columna);
    if (valor == nullptr) {
        return "";
    }
    return reinterpret_cast<const char*>(valor);
}

}  // namespace

bool InventarioRepository::existeId(long long idMedicamento) const {
    sqlite3* conexion = BaseDatosSqlite::instancia().obtenerConexion();
    if (conexion == nullptr || !BaseDatosSqlite::instancia().estaAbierta()) {
        return false;
    }

    sqlite3_stmt* sentencia = nullptr;
    if (sqlite3_prepare_v2(conexion,
                           "SELECT 1 FROM medicamentos WHERE id_medicamento = ? LIMIT 1;",
                           -1, &sentencia, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(sentencia, 1, idMedicamento);
    const int resultado = sqlite3_step(sentencia);
    sqlite3_finalize(sentencia);
    return resultado == SQLITE_ROW;
}

bool InventarioRepository::crear(const Medicamento& datos,
                                long long& idGenerado,
                                std::string& error) const {
    sqlite3* conexion = BaseDatosSqlite::instancia().obtenerConexion();
    if (conexion == nullptr || !BaseDatosSqlite::instancia().estaAbierta()) {
        error = "La base de datos no está disponible";
        return false;
    }

    sqlite3_stmt* sentencia = nullptr;
    const std::string sql =
        "INSERT INTO medicamentos (nombre, tipo_insumo, unidad_medida, descripcion, umbral_minimo) "
        "VALUES (?, ?, ?, ?, ?);";
    if (sqlite3_prepare_v2(conexion, sql.c_str(), -1, &sentencia, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(conexion);
        return false;
    }

    const std::string nombre = datos.getNombre();
    const std::string tipo = datos.getTipoInsumo();
    const std::string unidad = datos.getUnidadMedida();
    const std::string descripcion = datos.getDescripcion();

    sqlite3_bind_text(sentencia, 1, nombre.c_str(), static_cast<int>(nombre.size()), SQLITE_TRANSIENT);
    sqlite3_bind_text(sentencia, 2, tipo.c_str(), static_cast<int>(tipo.size()), SQLITE_TRANSIENT);
    sqlite3_bind_text(sentencia, 3, unidad.c_str(), static_cast<int>(unidad.size()), SQLITE_TRANSIENT);
    if (descripcion.empty()) {
        sqlite3_bind_null(sentencia, 4);
    } else {
        sqlite3_bind_text(sentencia, 4, descripcion.c_str(), static_cast<int>(descripcion.size()), SQLITE_TRANSIENT);
    }
    sqlite3_bind_int64(sentencia, 5, datos.getUmbralMinimo());

    const int resultado = sqlite3_step(sentencia);
    sqlite3_finalize(sentencia);

    if (resultado != SQLITE_DONE) {
        error = sqlite3_errmsg(conexion);
        return false;
    }

    idGenerado = sqlite3_last_insert_rowid(conexion);
    error.clear();
    return true;
}

bool InventarioRepository::actualizar(long long idMedicamento,
                                      const Medicamento& datos,
                                      std::string& error) const {
    sqlite3* conexion = BaseDatosSqlite::instancia().obtenerConexion();
    if (conexion == nullptr || !BaseDatosSqlite::instancia().estaAbierta()) {
        error = "La base de datos no está disponible";
        return false;
    }

    const std::string sql =
        "UPDATE medicamentos SET nombre = ?, tipo_insumo = ?, unidad_medida = ?, descripcion = ?, umbral_minimo = ? "
        "WHERE id_medicamento = ?;";
    sqlite3_stmt* sentencia = nullptr;
    if (sqlite3_prepare_v2(conexion, sql.c_str(), -1, &sentencia, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(conexion);
        return false;
    }

    const std::string nombre = datos.getNombre();
    const std::string tipo = datos.getTipoInsumo();
    const std::string unidad = datos.getUnidadMedida();
    const std::string descripcion = datos.getDescripcion();

    sqlite3_bind_text(sentencia, 1, nombre.c_str(), static_cast<int>(nombre.size()), SQLITE_TRANSIENT);
    sqlite3_bind_text(sentencia, 2, tipo.c_str(), static_cast<int>(tipo.size()), SQLITE_TRANSIENT);
    sqlite3_bind_text(sentencia, 3, unidad.c_str(), static_cast<int>(unidad.size()), SQLITE_TRANSIENT);
    if (descripcion.empty()) {
        sqlite3_bind_null(sentencia, 4);
    } else {
        sqlite3_bind_text(sentencia, 4, descripcion.c_str(), static_cast<int>(descripcion.size()), SQLITE_TRANSIENT);
    }
    sqlite3_bind_int64(sentencia, 5, datos.getUmbralMinimo());
    sqlite3_bind_int64(sentencia, 6, idMedicamento);

    const int resultado = sqlite3_step(sentencia);
    sqlite3_finalize(sentencia);

    if (resultado != SQLITE_DONE) {
        error = sqlite3_errmsg(conexion);
        return false;
    }

    if (sqlite3_changes(conexion) == 0) {
        error = "No se encontró el medicamento para actualizar";
        return false;
    }

    error.clear();
    return true;
}

bool InventarioRepository::listar(std::vector<Medicamento>& medicamentos, std::string& error) const {
    sqlite3* conexion = BaseDatosSqlite::instancia().obtenerConexion();
    if (conexion == nullptr || !BaseDatosSqlite::instancia().estaAbierta()) {
        error = "La base de datos no está disponible";
        return false;
    }

    const std::string sqlMedicamentos =
        "SELECT id_medicamento, nombre, tipo_insumo, unidad_medida, descripcion, "
        "       cantidad_total_disponible, umbral_minimo "
        "FROM medicamentos ORDER BY nombre;";

    sqlite3_stmt* sentencia = nullptr;
    if (sqlite3_prepare_v2(conexion, sqlMedicamentos.c_str(), -1, &sentencia, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(conexion);
        return false;
    }

    medicamentos.clear();

    // Guarda en qué posición del vector quedó cada medicamento para colgarle
    // después sus lotes sin tener que recorrerlo entero por cada uno.
    std::map<long long, std::size_t> posicionPorId;

    while (sqlite3_step(sentencia) == SQLITE_ROW) {
        Medicamento medicamento;
        medicamento.setIdMedicamento(sqlite3_column_int64(sentencia, 0));
        medicamento.setNombre(leerTexto(sentencia, 1));
        medicamento.setTipoInsumo(leerTexto(sentencia, 2));
        medicamento.setUnidadMedida(leerTexto(sentencia, 3));
        medicamento.setDescripcion(leerTexto(sentencia, 4));
        medicamento.setCantidadTotalDisponible(sqlite3_column_int64(sentencia, 5));
        medicamento.setUmbralMinimo(sqlite3_column_int64(sentencia, 6));

        posicionPorId[medicamento.getIdMedicamento()] = medicamentos.size();
        medicamentos.push_back(medicamento);
    }

    sqlite3_finalize(sentencia);

    // Segunda consulta para todos los lotes de golpe: con una consulta por
    // medicamento tendríamos el problema N+1.
    const std::string sqlLotes =
        "SELECT id_lote, id_medicamento, num_lote, ubicacion, fecha_vencimiento, cantidad "
        "FROM lotes ORDER BY fecha_vencimiento, id_lote;";

    sqlite3_stmt* sentenciaLotes = nullptr;
    if (sqlite3_prepare_v2(conexion, sqlLotes.c_str(), -1, &sentenciaLotes, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(conexion);
        return false;
    }

    while (sqlite3_step(sentenciaLotes) == SQLITE_ROW) {
        Lote lote;
        lote.setIdLote(sqlite3_column_int64(sentenciaLotes, 0));
        lote.setIdMedicamento(sqlite3_column_int64(sentenciaLotes, 1));
        lote.setNumLote(leerTexto(sentenciaLotes, 2));
        lote.setUbicacion(leerTexto(sentenciaLotes, 3));
        lote.setFechaVencimiento(leerTexto(sentenciaLotes, 4));
        lote.setCantidad(sqlite3_column_int64(sentenciaLotes, 5));

        const auto posicion = posicionPorId.find(lote.getIdMedicamento());
        if (posicion != posicionPorId.end()) {
            medicamentos[posicion->second].agregarLote(lote);
        }
    }

    sqlite3_finalize(sentenciaLotes);
    error.clear();
    return true;
}

bool InventarioRepository::eliminar(long long idMedicamento, std::string& error) const {
    sqlite3* conexion = BaseDatosSqlite::instancia().obtenerConexion();
    if (conexion == nullptr || !BaseDatosSqlite::instancia().estaAbierta()) {
        error = "La base de datos no está disponible";
        return false;
    }

    const std::string sql = "DELETE FROM medicamentos WHERE id_medicamento = ?;";
    sqlite3_stmt* sentencia = nullptr;
    if (sqlite3_prepare_v2(conexion, sql.c_str(), -1, &sentencia, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(conexion);
        return false;
    }

    sqlite3_bind_int64(sentencia, 1, idMedicamento);
    const int resultado = sqlite3_step(sentencia);
    sqlite3_finalize(sentencia);

    if (resultado != SQLITE_DONE) {
        error = sqlite3_errmsg(conexion);
        return false;
    }

    if (sqlite3_changes(conexion) == 0) {
        error = "No se encontró el medicamento para eliminar";
        return false;
    }

    error.clear();
    return true;
}
