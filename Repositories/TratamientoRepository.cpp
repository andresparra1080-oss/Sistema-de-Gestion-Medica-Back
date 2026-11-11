#include "Repositories/TratamientoRepository.h"

#include "Database/SQLite.h"

namespace {

std::string leerTexto(sqlite3_stmt* sentencia, int columna) {
    const unsigned char* valor = sqlite3_column_text(sentencia, columna);

    if (valor == nullptr) {
        return "";
    }

    return reinterpret_cast<const char*>(valor);
}

void leerTratamiento(sqlite3_stmt* sentencia, Tratamiento& tratamiento) {
    tratamiento.setIdTratamiento(sqlite3_column_int64(sentencia, 0));
    tratamiento.setIdPaciente(sqlite3_column_int64(sentencia, 1));
    tratamiento.setIdMedicamento(sqlite3_column_int64(sentencia, 2));
    tratamiento.setIdUsuarioRegistra(sqlite3_column_int64(sentencia, 3));
    tratamiento.setDosis(leerTexto(sentencia, 4));
    tratamiento.setFrecuencia(leerTexto(sentencia, 5));

    if (sqlite3_column_type(sentencia, 6) == SQLITE_NULL) {
        tratamiento.quitarIntervaloHoras();
    } else {
        tratamiento.setIntervaloHoras(sqlite3_column_int(sentencia, 6));
    }

    tratamiento.setFechaInicio(leerTexto(sentencia, 7));
    tratamiento.setFechaFin(leerTexto(sentencia, 8));
    tratamiento.setFechaFinalizacionEfectiva(leerTexto(sentencia, 9));
    tratamiento.setEstadoTratamiento(leerTexto(sentencia, 10));
}

void enlazarTexto(sqlite3_stmt* sentencia, int posicion,
                  const std::string& valor) {
    if (valor.empty()) {
        sqlite3_bind_null(sentencia, posicion);
    } else {
        sqlite3_bind_text(sentencia, posicion, valor.c_str(), -1, SQLITE_TRANSIENT);
    }
}

bool prepararConsulta(sqlite3* conexion, const std::string& sql,
                      sqlite3_stmt*& sentencia, std::string& error) {
    if (sqlite3_prepare_v2(conexion, sql.c_str(), -1, &sentencia, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(conexion);
        return false;
    }

    return true;
}

bool obtenerConexion(sqlite3*& conexion, std::string& error) {
    conexion = BaseDatosSqlite::instancia().obtenerConexion();

    if (conexion == nullptr || !BaseDatosSqlite::instancia().estaAbierta()) {
        error = "La base de datos no está disponible";
        return false;
    }

    return true;
}

}  // namespace

bool TratamientoRepository::listar(
    std::vector<Tratamiento>& tratamientos,
    std::string& error) const {

    sqlite3* conexion = nullptr;

    if (!obtenerConexion(conexion, error)) {
        return false;
    }

    const std::string sql =
        "SELECT id_tratamiento, id_paciente, id_medicamento, "
        "id_usuario_registra, dosis, frecuencia, intervalo_horas, "
        "fecha_inicio, fecha_fin, fecha_finalizacion_efectiva, "
        "estado_tratamiento "
        "FROM tratamientos "
        "ORDER BY fecha_inicio DESC, id_tratamiento DESC;";

    sqlite3_stmt* sentencia = nullptr;

    if (!prepararConsulta(conexion, sql, sentencia, error)) {
        return false;
    }

    tratamientos.clear();

    int resultado;

    while ((resultado = sqlite3_step(sentencia)) == SQLITE_ROW) {
        Tratamiento tratamiento;
        leerTratamiento(sentencia, tratamiento);
        tratamientos.push_back(tratamiento);
    }

    if (resultado != SQLITE_DONE) {
        error = sqlite3_errmsg(conexion);
        sqlite3_finalize(sentencia);
        return false;
    }

    sqlite3_finalize(sentencia);
    error.clear();
    return true;
}

bool TratamientoRepository::listarPorPaciente(
    long long idPaciente,
    std::vector<Tratamiento>& tratamientos,
    std::string& error) const {

    sqlite3* conexion = nullptr;

    if (!obtenerConexion(conexion, error)) {
        return false;
    }

    const std::string sql =
        "SELECT id_tratamiento, id_paciente, id_medicamento, "
        "id_usuario_registra, dosis, frecuencia, intervalo_horas, "
        "fecha_inicio, fecha_fin, fecha_finalizacion_efectiva, "
        "estado_tratamiento "
        "FROM tratamientos "
        "WHERE id_paciente = ? "
        "ORDER BY fecha_inicio DESC, id_tratamiento DESC;";

    sqlite3_stmt* sentencia = nullptr;

    if (!prepararConsulta(conexion, sql, sentencia, error)) {
        return false;
    }

    sqlite3_bind_int64(sentencia, 1, idPaciente);

    tratamientos.clear();

    int resultado;

    while ((resultado = sqlite3_step(sentencia)) == SQLITE_ROW) {
        Tratamiento tratamiento;
        leerTratamiento(sentencia, tratamiento);
        tratamientos.push_back(tratamiento);
    }

    if (resultado != SQLITE_DONE) {
        error = sqlite3_errmsg(conexion);
        sqlite3_finalize(sentencia);
        return false;
    }

    sqlite3_finalize(sentencia);
    error.clear();
    return true;
}

bool TratamientoRepository::buscarPorId(
    long long idTratamiento,
    Tratamiento& tratamiento,
    std::string& error) const {

    sqlite3* conexion = nullptr;

    if (!obtenerConexion(conexion, error)) {
        return false;
    }

    const std::string sql =
        "SELECT id_tratamiento, id_paciente, id_medicamento, "
        "id_usuario_registra, dosis, frecuencia, intervalo_horas, "
        "fecha_inicio, fecha_fin, fecha_finalizacion_efectiva, "
        "estado_tratamiento "
        "FROM tratamientos "
        "WHERE id_tratamiento = ?;";

    sqlite3_stmt* sentencia = nullptr;

    if (!prepararConsulta(conexion, sql, sentencia, error)) {
        return false;
    }

    sqlite3_bind_int64(sentencia, 1, idTratamiento);

    const int resultado = sqlite3_step(sentencia);

    if (resultado == SQLITE_ROW) {
        leerTratamiento(sentencia, tratamiento);
        sqlite3_finalize(sentencia);
        error.clear();
        return true;
    }

    if (resultado == SQLITE_DONE) {
        error = "No se encontró el tratamiento";
    } else {
        error = sqlite3_errmsg(conexion);
    }

    sqlite3_finalize(sentencia);
    return false;
}

bool TratamientoRepository::crear(
    const Tratamiento& tratamiento,
    long long& idGenerado,
    std::string& error) const {

    sqlite3* conexion = nullptr;

    if (!obtenerConexion(conexion, error)) {
        return false;
    }

    const std::string sql =
        "INSERT INTO tratamientos ("
        "id_paciente, id_medicamento, id_usuario_registra, "
        "dosis, frecuencia, intervalo_horas, fecha_inicio, "
        "fecha_fin, fecha_finalizacion_efectiva, estado_tratamiento"
        ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

    sqlite3_stmt* sentencia = nullptr;

    if (!prepararConsulta(conexion, sql, sentencia, error)) {
        return false;
    }

    sqlite3_bind_int64(sentencia, 1, tratamiento.getIdPaciente());
    sqlite3_bind_int64(sentencia, 2, tratamiento.getIdMedicamento());
    sqlite3_bind_int64(sentencia, 3, tratamiento.getIdUsuarioRegistra());

    enlazarTexto(sentencia, 4, tratamiento.getDosis());
    enlazarTexto(sentencia, 5, tratamiento.getFrecuencia());

    if (tratamiento.tieneIntervaloHoras()) {
        sqlite3_bind_int(sentencia, 6, tratamiento.getIntervaloHoras());
    } else {
        sqlite3_bind_null(sentencia, 6);
    }

    enlazarTexto(sentencia, 7, tratamiento.getFechaInicio());
    enlazarTexto(sentencia, 8, tratamiento.getFechaFin());
    enlazarTexto(sentencia, 9, tratamiento.getFechaFinalizacionEfectiva());

    const std::string estado =
        tratamiento.getEstadoTratamiento().empty()
            ? "ACTIVO"
            : tratamiento.getEstadoTratamiento();

    enlazarTexto(sentencia, 10, estado);

    const int resultado = sqlite3_step(sentencia);

    if (resultado != SQLITE_DONE) {
        error = sqlite3_errmsg(conexion);
        sqlite3_finalize(sentencia);
        return false;
    }

    idGenerado = sqlite3_last_insert_rowid(conexion);

    sqlite3_finalize(sentencia);
    error.clear();
    return true;
}

bool TratamientoRepository::actualizar(
    const Tratamiento& tratamiento,
    std::string& error) const {

    sqlite3* conexion = nullptr;

    if (!obtenerConexion(conexion, error)) {
        return false;
    }

    const std::string sql =
        "UPDATE tratamientos "
        "SET dosis = ?, frecuencia = ?, intervalo_horas = ?, fecha_fin = ? "
        "WHERE id_tratamiento = ?;";

    sqlite3_stmt* sentencia = nullptr;

    if (!prepararConsulta(conexion, sql, sentencia, error)) {
        return false;
    }

    enlazarTexto(sentencia, 1, tratamiento.getDosis());
    enlazarTexto(sentencia, 2, tratamiento.getFrecuencia());

    if (tratamiento.tieneIntervaloHoras()) {
        sqlite3_bind_int(sentencia, 3, tratamiento.getIntervaloHoras());
    } else {
        sqlite3_bind_null(sentencia, 3);
    }

    enlazarTexto(sentencia, 4, tratamiento.getFechaFin());
    sqlite3_bind_int64(sentencia, 5, tratamiento.getIdTratamiento());

    const int resultado = sqlite3_step(sentencia);

    if (resultado != SQLITE_DONE) {
        error = sqlite3_errmsg(conexion);
        sqlite3_finalize(sentencia);
        return false;
    }

    if (sqlite3_changes(conexion) == 0) {
        error = "No se encontró el tratamiento para actualizar";
        sqlite3_finalize(sentencia);
        return false;
    }

    sqlite3_finalize(sentencia);
    error.clear();
    return true;
}

bool TratamientoRepository::cambiarEstado(
    long long idTratamiento,
    const std::string& estado,
    const std::string& fechaFinalizacionEfectiva,
    std::string& error) const {

    sqlite3* conexion = nullptr;

    if (!obtenerConexion(conexion, error)) {
        return false;
    }

    const std::string sql =
        "UPDATE tratamientos "
        "SET estado_tratamiento = ?, "
        "fecha_finalizacion_efectiva = ? "
        "WHERE id_tratamiento = ?;";

    sqlite3_stmt* sentencia = nullptr;

    if (!prepararConsulta(conexion, sql, sentencia, error)) {
        return false;
    }

    enlazarTexto(sentencia, 1, estado);
    enlazarTexto(sentencia, 2, fechaFinalizacionEfectiva);
    sqlite3_bind_int64(sentencia, 3, idTratamiento);

    const int resultado = sqlite3_step(sentencia);

    if (resultado != SQLITE_DONE) {
        error = sqlite3_errmsg(conexion);
        sqlite3_finalize(sentencia);
        return false;
    }

    if (sqlite3_changes(conexion) == 0) {
        error = "No se encontró el tratamiento para cambiar su estado";
        sqlite3_finalize(sentencia);
        return false;
    }

    sqlite3_finalize(sentencia);
    error.clear();
    return true;
}