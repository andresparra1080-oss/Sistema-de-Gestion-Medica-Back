#include "Repositories/LoteRepository.h"

#include <sqlite3.h>

#include "Database/SQLite.h"

namespace {

class ConexionLotes {
public:
    sqlite3* db = nullptr;

    ~ConexionLotes() {
        if (db != nullptr) {
            sqlite3_close(db);
        }
    }

    bool abrir(std::string& error) {
        sqlite3* principal =
            BaseDatosSqlite::instancia().obtenerConexion();

        if (principal == nullptr) {
            error = "La base de datos no esta disponible";
            return false;
        }

        const char* ruta = sqlite3_db_filename(principal, "main");

        if (ruta == nullptr || ruta[0] == '\0') {
            error = "No se encontro la ruta de la base de datos";
            return false;
        }

        if (sqlite3_open_v2(
                ruta,
                &db,
                SQLITE_OPEN_READWRITE | SQLITE_OPEN_FULLMUTEX,
                nullptr) != SQLITE_OK) {
            error = db != nullptr
                ? sqlite3_errmsg(db)
                : "No se pudo abrir la conexion";
            return false;
        }

        sqlite3_busy_timeout(db, 5000);

        if (sqlite3_exec(
                db,
                "PRAGMA foreign_keys = ON;",
                nullptr,
                nullptr,
                nullptr) != SQLITE_OK) {
            error = sqlite3_errmsg(db);
            return false;
        }

        return true;
    }

    ConexionLotes() = default;
    ConexionLotes(const ConexionLotes&) = delete;
    ConexionLotes& operator=(const ConexionLotes&) = delete;
};

std::string leerTexto(sqlite3_stmt* sentencia, int columna) {
    const unsigned char* valor =
        sqlite3_column_text(sentencia, columna);

    return valor == nullptr
        ? ""
        : reinterpret_cast<const char*>(valor);
}

}  // namespace

bool LoteRepository::listar(
    std::vector<Lote>& lotes,
    std::string& error) const {

    lotes.clear();
    error.clear();

    ConexionLotes conexion;
    if (!conexion.abrir(error)) {
        return false;
    }

    const char* sql =
        "SELECT id_lote, id_medicamento, num_lote, "
        "ubicacion, fecha_vencimiento, cantidad "
        "FROM lotes ORDER BY fecha_vencimiento, id_lote;";

    sqlite3_stmt* sentencia = nullptr;

    if (sqlite3_prepare_v2(
            conexion.db, sql, -1,
            &sentencia, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(conexion.db);
        return false;
    }

    int resultado;

    while ((resultado = sqlite3_step(sentencia)) == SQLITE_ROW) {
        Lote lote;

        lote.setIdLote(sqlite3_column_int64(sentencia, 0));
        lote.setIdMedicamento(sqlite3_column_int64(sentencia, 1));
        lote.setNumLote(leerTexto(sentencia, 2));
        lote.setUbicacion(leerTexto(sentencia, 3));
        lote.setFechaVencimiento(leerTexto(sentencia, 4));
        lote.setCantidad(sqlite3_column_int64(sentencia, 5));

        lotes.push_back(lote);
    }

    if (resultado != SQLITE_DONE) {
        error = sqlite3_errmsg(conexion.db);
        sqlite3_finalize(sentencia);
        lotes.clear();
        return false;
    }

    sqlite3_finalize(sentencia);
    return true;
}

bool LoteRepository::crear(
    const Lote& datos,
    long long& idGenerado,
    std::string& error) const {

    idGenerado = 0;
    error.clear();

    ConexionLotes conexion;
    if (!conexion.abrir(error)) {
        return false;
    }

    const char* sql =
        "INSERT INTO lotes "
        "(id_medicamento, num_lote, ubicacion, fecha_vencimiento) "
        "VALUES (?, ?, ?, ?);";

    sqlite3_stmt* sentencia = nullptr;

    if (sqlite3_prepare_v2(
            conexion.db, sql, -1,
            &sentencia, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(conexion.db);
        return false;
    }

    const std::string numero = datos.getNumLote();
    const std::string ubicacion = datos.getUbicacion();
    const std::string fecha = datos.getFechaVencimiento();

    sqlite3_bind_int64(
        sentencia, 1, datos.getIdMedicamento());

    sqlite3_bind_text(
        sentencia, 2, numero.c_str(),
        static_cast<int>(numero.size()), SQLITE_TRANSIENT);

    sqlite3_bind_text(
        sentencia, 3, ubicacion.c_str(),
        static_cast<int>(ubicacion.size()), SQLITE_TRANSIENT);

    sqlite3_bind_text(
        sentencia, 4, fecha.c_str(),
        static_cast<int>(fecha.size()), SQLITE_TRANSIENT);

    const int resultado = sqlite3_step(sentencia);

    if (resultado != SQLITE_DONE) {
        const int codigo = sqlite3_extended_errcode(conexion.db);

        if (codigo == SQLITE_CONSTRAINT_UNIQUE) {
            error = "Ya existe ese numero de lote para el medicamento";
        } else if (codigo == SQLITE_CONSTRAINT_FOREIGNKEY) {
            error = "No existe el medicamento indicado";
        } else {
            error = sqlite3_errmsg(conexion.db);
        }

        sqlite3_finalize(sentencia);
        return false;
    }

    idGenerado = sqlite3_last_insert_rowid(conexion.db);
    sqlite3_finalize(sentencia);

    return true;
}