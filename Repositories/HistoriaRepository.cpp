#include "Repositories/HistoriaRepository.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

#include "Database/SQLite.h"

namespace {

std::string leerTexto(sqlite3_stmt* sentencia, int columna) {
    const unsigned char* valor = sqlite3_column_text(sentencia, columna);
    if (valor == nullptr) {
        return "";
    }
    return reinterpret_cast<const char*>(valor);
}

bool existeEn(const char* sql, long long id) {
    sqlite3* conexion = BaseDatosSqlite::instancia().obtenerConexion();
    if (conexion == nullptr || !BaseDatosSqlite::instancia().estaAbierta()) {
        return false;
    }

    sqlite3_stmt* sentencia = nullptr;
    if (sqlite3_prepare_v2(conexion, sql, -1, &sentencia, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(sentencia, 1, id);
    const int resultado = sqlite3_step(sentencia);
    sqlite3_finalize(sentencia);
    return resultado == SQLITE_ROW;
}

bool ejecutar(sqlite3* conexion, const char* sql) {
    return sqlite3_exec(conexion, sql, nullptr, nullptr, nullptr) == SQLITE_OK;
}

// Enlaza un texto, o NULL si viene vacío: en la base "sin dato" es NULL, no
// una cadena en blanco.
void enlazarTextoONulo(sqlite3_stmt* sentencia, int posicion, const std::string& valor) {
    if (valor.empty()) {
        sqlite3_bind_null(sentencia, posicion);
    } else {
        sqlite3_bind_text(sentencia, posicion, valor.c_str(), static_cast<int>(valor.size()), SQLITE_TRANSIENT);
    }
}

bool cargarAlergias(sqlite3* conexion, HistoriaClinica& historia) {
    const std::string sql =
        "SELECT a.id_alergia, a.sustancia, a.id_medicamento, m.nombre "
        "FROM alergias a LEFT JOIN medicamentos m ON m.id_medicamento = a.id_medicamento "
        "WHERE a.id_historia = ? ORDER BY a.sustancia;";

    sqlite3_stmt* sentencia = nullptr;
    if (sqlite3_prepare_v2(conexion, sql.c_str(), -1, &sentencia, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(sentencia, 1, historia.getIdHistoria());

    while (sqlite3_step(sentencia) == SQLITE_ROW) {
        Alergia alergia;
        alergia.setIdAlergia(sqlite3_column_int64(sentencia, 0));
        alergia.setSustancia(leerTexto(sentencia, 1));

        if (sqlite3_column_type(sentencia, 2) == SQLITE_NULL) {
            alergia.quitarIdMedicamento();
        } else {
            alergia.setIdMedicamento(sqlite3_column_int64(sentencia, 2));
        }

        alergia.setNombreMedicamento(leerTexto(sentencia, 3));
        historia.agregarAlergia(alergia);
    }

    sqlite3_finalize(sentencia);
    return true;
}

// ---------------------------------------------------------------------
// Auditoría: cada cambio de la historia deja una fila en logs_auditoria,
// dentro de la misma transacción que el cambio. Se registra con
// tabla_afectada = 'historias_clinicas' e id_registro = id_historia porque
// es lo que AuditoriaRepository busca para armar el historial del paciente.
// ---------------------------------------------------------------------
struct EstadoHistoria {
    std::string antecedentes;
    std::string cirugiasPrevias;
    std::string observaciones;
    int frutaDiablo = 0;
    int alergiasRevisadas = 0;
    std::vector<std::string> alergias;
};

std::string aMinusculas(const std::string& valor) {
    std::string resultado = valor;
    for (char& caracter : resultado) {
        caracter = static_cast<char>(std::tolower(static_cast<unsigned char>(caracter)));
    }
    return resultado;
}

bool leerEstado(sqlite3* conexion, long long idHistoria, EstadoHistoria& estado) {
    sqlite3_stmt* sentencia = nullptr;
    if (sqlite3_prepare_v2(conexion,
                           "SELECT COALESCE(antecedentes, ''), COALESCE(cirugias_previas, ''), "
                           "       COALESCE(observaciones, ''), fruta_diablo, alergias_revisadas "
                           "FROM historias_clinicas WHERE id_historia = ?;",
                           -1, &sentencia, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(sentencia, 1, idHistoria);
    if (sqlite3_step(sentencia) != SQLITE_ROW) {
        sqlite3_finalize(sentencia);
        return false;
    }

    estado.antecedentes = leerTexto(sentencia, 0);
    estado.cirugiasPrevias = leerTexto(sentencia, 1);
    estado.observaciones = leerTexto(sentencia, 2);
    estado.frutaDiablo = sqlite3_column_int(sentencia, 3);
    estado.alergiasRevisadas = sqlite3_column_int(sentencia, 4);
    sqlite3_finalize(sentencia);

    sqlite3_stmt* alergias = nullptr;
    if (sqlite3_prepare_v2(conexion,
                           "SELECT sustancia FROM alergias WHERE id_historia = ? ORDER BY sustancia;",
                           -1, &alergias, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(alergias, 1, idHistoria);
    while (sqlite3_step(alergias) == SQLITE_ROW) {
        estado.alergias.push_back(leerTexto(alergias, 0));
    }
    sqlite3_finalize(alergias);
    return true;
}

bool registrarLog(sqlite3* conexion,
                  long long idUsuario,
                  const std::string& accion,
                  const std::string& tabla,
                  long long idRegistro,
                  const std::string& detalle) {
    sqlite3_stmt* sentencia = nullptr;
    if (sqlite3_prepare_v2(conexion,
                           "INSERT INTO logs_auditoria "
                           "(id_usuario, accion, tabla_afectada, id_registro, detalle_cambio) "
                           "VALUES (?, ?, ?, ?, ?);",
                           -1, &sentencia, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(sentencia, 1, idUsuario);
    sqlite3_bind_text(sentencia, 2, accion.c_str(), static_cast<int>(accion.size()), SQLITE_TRANSIENT);
    sqlite3_bind_text(sentencia, 3, tabla.c_str(), static_cast<int>(tabla.size()), SQLITE_TRANSIENT);
    sqlite3_bind_int64(sentencia, 4, idRegistro);
    sqlite3_bind_text(sentencia, 5, detalle.c_str(), static_cast<int>(detalle.size()), SQLITE_TRANSIENT);

    const bool ok = sqlite3_step(sentencia) == SQLITE_DONE;
    sqlite3_finalize(sentencia);
    return ok;
}

// Compara el estado de antes y de después y devuelve una frase por cada cosa
// que cambió de verdad (el formulario envía todos los campos aunque solo se
// haya tocado uno, así que no se puede fiar de qué campos llegaron).
std::vector<std::string> describirCambios(const EstadoHistoria& antes, const EstadoHistoria& despues) {
    std::vector<std::string> detalles;

    if (antes.antecedentes != despues.antecedentes) {
        detalles.push_back("Modificó los antecedentes de la historia clínica.");
    }
    if (antes.cirugiasPrevias != despues.cirugiasPrevias) {
        detalles.push_back("Modificó las cirugías previas de la historia clínica.");
    }
    if (antes.observaciones != despues.observaciones) {
        detalles.push_back("Modificó las observaciones de la historia clínica.");
    }
    if (antes.frutaDiablo != despues.frutaDiablo) {
        detalles.push_back(despues.frutaDiablo != 0
                               ? "Marcó al paciente como usuario de fruta del diablo."
                               : "Quitó la marca de usuario de fruta del diablo.");
    }

    bool cambiaronAlergias = false;

    for (const std::string& nueva : despues.alergias) {
        const std::string clave = aMinusculas(nueva);
        const bool existia = std::any_of(antes.alergias.begin(), antes.alergias.end(),
                                         [&](const std::string& vieja) { return aMinusculas(vieja) == clave; });
        if (!existia) {
            detalles.push_back("Agregó la alergia a " + nueva + ".");
            cambiaronAlergias = true;
        }
    }

    for (const std::string& vieja : antes.alergias) {
        const std::string clave = aMinusculas(vieja);
        const bool sigue = std::any_of(despues.alergias.begin(), despues.alergias.end(),
                                       [&](const std::string& nueva) { return aMinusculas(nueva) == clave; });
        if (!sigue) {
            detalles.push_back("Quitó la alergia a " + vieja + ".");
            cambiaronAlergias = true;
        }
    }

    // Agregar la primera alergia sube la marca sola (trigger); solo se anota
    // aparte cuando alguien la cambió sin tocar la lista.
    if (antes.alergiasRevisadas != despues.alergiasRevisadas && !cambiaronAlergias) {
        detalles.push_back(despues.alergiasRevisadas != 0
                               ? "Marcó las alergias como revisadas."
                               : "Quitó la marca de alergias revisadas.");
    }

    return detalles;
}

}  // namespace

bool HistoriaRepository::existePaciente(long long idPaciente) const {
    return existeEn("SELECT 1 FROM pacientes WHERE id_paciente = ? LIMIT 1;", idPaciente);
}

bool HistoriaRepository::existeMedicamento(long long idMedicamento) const {
    return existeEn("SELECT 1 FROM medicamentos WHERE id_medicamento = ? LIMIT 1;", idMedicamento);
}

bool HistoriaRepository::buscarPorPaciente(long long idPaciente,
                                           HistoriaClinica& historia,
                                           std::string& error) const {
    sqlite3* conexion = BaseDatosSqlite::instancia().obtenerConexion();
    if (conexion == nullptr || !BaseDatosSqlite::instancia().estaAbierta()) {
        error = "La base de datos no está disponible";
        return false;
    }

    const std::string sql =
        "SELECT h.id_historia, h.id_paciente, p.nombre_completo, h.alergias_revisadas, "
        "       h.antecedentes, h.cirugias_previas, h.observaciones, h.fruta_diablo, h.fecha_creacion "
        "FROM historias_clinicas h JOIN pacientes p ON p.id_paciente = h.id_paciente "
        "WHERE h.id_paciente = ? LIMIT 1;";

    sqlite3_stmt* sentencia = nullptr;
    if (sqlite3_prepare_v2(conexion, sql.c_str(), -1, &sentencia, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(conexion);
        return false;
    }

    sqlite3_bind_int64(sentencia, 1, idPaciente);

    bool encontrada = false;
    if (sqlite3_step(sentencia) == SQLITE_ROW) {
        historia.setIdHistoria(sqlite3_column_int64(sentencia, 0));
        historia.setIdPaciente(sqlite3_column_int64(sentencia, 1));
        historia.setNombrePaciente(leerTexto(sentencia, 2));
        historia.setAlergiasRevisadas(sqlite3_column_int(sentencia, 3) != 0);
        historia.setAntecedentes(leerTexto(sentencia, 4));
        historia.setCirugiasPrevias(leerTexto(sentencia, 5));
        historia.setObservaciones(leerTexto(sentencia, 6));
        historia.setFrutaDiablo(sqlite3_column_int(sentencia, 7) != 0);
        historia.setFechaCreacion(leerTexto(sentencia, 8));
        encontrada = true;
    }

    sqlite3_finalize(sentencia);

    if (!encontrada) {
        error.clear();
        return false;
    }

    cargarAlergias(conexion, historia);
    error.clear();
    return true;
}

bool HistoriaRepository::actualizar(long long idHistoria,
                                    long long idUsuario,
                                    const CambiosHistoria& cambios,
                                    std::string& error) const {
    sqlite3* conexion = BaseDatosSqlite::instancia().obtenerConexion();
    if (conexion == nullptr || !BaseDatosSqlite::instancia().estaAbierta()) {
        error = "La base de datos no está disponible";
        return false;
    }

    if (!ejecutar(conexion, "BEGIN IMMEDIATE;")) {
        error = sqlite3_errmsg(conexion);
        return false;
    }

    // Foto de la historia antes de tocarla, para saber qué cambió de verdad.
    EstadoHistoria antes;
    if (!leerEstado(conexion, idHistoria, antes)) {
        error = "No se pudo leer la historia clínica antes de actualizarla";
        ejecutar(conexion, "ROLLBACK;");
        return false;
    }

    // Se arma el UPDATE solo con los campos que llegaron, para no pisar con
    // NULL los que el cliente no envió.
    std::string asignaciones;
    std::vector<const std::string*> textos;
    std::vector<int> enteros;

    if (cambios.antecedentes.has_value()) {
        asignaciones += (asignaciones.empty() ? "" : ", ");
        asignaciones += "antecedentes = ?";
        textos.push_back(&cambios.antecedentes.value());
    }

    if (cambios.cirugiasPrevias.has_value()) {
        asignaciones += (asignaciones.empty() ? "" : ", ");
        asignaciones += "cirugias_previas = ?";
        textos.push_back(&cambios.cirugiasPrevias.value());
    }

    if (cambios.observaciones.has_value()) {
        asignaciones += (asignaciones.empty() ? "" : ", ");
        asignaciones += "observaciones = ?";
        textos.push_back(&cambios.observaciones.value());
    }

    if (cambios.frutaDiablo.has_value()) {
        asignaciones += (asignaciones.empty() ? "" : ", ");
        asignaciones += "fruta_diablo = ?";
        enteros.push_back(cambios.frutaDiablo.value() ? 1 : 0);
    }

    if (!asignaciones.empty()) {
        const std::string sql =
            "UPDATE historias_clinicas SET " + asignaciones + " WHERE id_historia = ?;";

        sqlite3_stmt* sentencia = nullptr;
        if (sqlite3_prepare_v2(conexion, sql.c_str(), -1, &sentencia, nullptr) != SQLITE_OK) {
            error = sqlite3_errmsg(conexion);
            ejecutar(conexion, "ROLLBACK;");
            return false;
        }

        int posicion = 1;
        for (const std::string* texto : textos) {
            enlazarTextoONulo(sentencia, posicion++, *texto);
        }
        for (int entero : enteros) {
            sqlite3_bind_int(sentencia, posicion++, entero);
        }
        sqlite3_bind_int64(sentencia, posicion, idHistoria);

        if (sqlite3_step(sentencia) != SQLITE_DONE) {
            error = sqlite3_errmsg(conexion);
            sqlite3_finalize(sentencia);
            ejecutar(conexion, "ROLLBACK;");
            return false;
        }

        sqlite3_finalize(sentencia);
    }

    // Las alergias se reemplazan en bloque: es la forma de que el formulario
    // pueda quitar una que se registró por error.
    if (cambios.alergias.has_value()) {
        sqlite3_stmt* borrado = nullptr;
        if (sqlite3_prepare_v2(conexion, "DELETE FROM alergias WHERE id_historia = ?;", -1, &borrado,
                               nullptr) != SQLITE_OK) {
            error = sqlite3_errmsg(conexion);
            ejecutar(conexion, "ROLLBACK;");
            return false;
        }

        sqlite3_bind_int64(borrado, 1, idHistoria);
        if (sqlite3_step(borrado) != SQLITE_DONE) {
            error = sqlite3_errmsg(conexion);
            sqlite3_finalize(borrado);
            ejecutar(conexion, "ROLLBACK;");
            return false;
        }
        sqlite3_finalize(borrado);

        for (const Alergia& alergia : cambios.alergias.value()) {
            sqlite3_stmt* insercion = nullptr;
            if (sqlite3_prepare_v2(conexion,
                                   "INSERT INTO alergias (id_historia, sustancia, id_medicamento) "
                                   "VALUES (?, ?, ?);",
                                   -1, &insercion, nullptr) != SQLITE_OK) {
                error = sqlite3_errmsg(conexion);
                ejecutar(conexion, "ROLLBACK;");
                return false;
            }

            const std::string sustancia = alergia.getSustancia();
            sqlite3_bind_int64(insercion, 1, idHistoria);
            sqlite3_bind_text(insercion, 2, sustancia.c_str(), static_cast<int>(sustancia.size()),
                              SQLITE_TRANSIENT);

            if (alergia.tieneIdMedicamento()) {
                sqlite3_bind_int64(insercion, 3, alergia.getIdMedicamento());
            } else {
                sqlite3_bind_null(insercion, 3);
            }

            if (sqlite3_step(insercion) != SQLITE_DONE) {
                error = sqlite3_errmsg(conexion);
                sqlite3_finalize(insercion);
                ejecutar(conexion, "ROLLBACK;");
                return false;
            }

            sqlite3_finalize(insercion);
        }
    }

    // Va al final a propósito: el trigger trg_alergias_ai_historia_revisada
    // pone la marca a 1 al insertar la primera alergia, y aquí manda lo que
    // diga el servicio (puede ser "revisadas y ninguna").
    if (cambios.alergiasRevisadas.has_value()) {
        sqlite3_stmt* sentencia = nullptr;
        if (sqlite3_prepare_v2(conexion,
                               "UPDATE historias_clinicas SET alergias_revisadas = ? WHERE id_historia = ?;",
                               -1, &sentencia, nullptr) != SQLITE_OK) {
            error = sqlite3_errmsg(conexion);
            ejecutar(conexion, "ROLLBACK;");
            return false;
        }

        sqlite3_bind_int(sentencia, 1, cambios.alergiasRevisadas.value() ? 1 : 0);
        sqlite3_bind_int64(sentencia, 2, idHistoria);

        if (sqlite3_step(sentencia) != SQLITE_DONE) {
            error = sqlite3_errmsg(conexion);
            sqlite3_finalize(sentencia);
            ejecutar(conexion, "ROLLBACK;");
            return false;
        }

        sqlite3_finalize(sentencia);
    }

    // Auditoría: una fila por cada cambio real, en la misma transacción.
    EstadoHistoria despues;
    if (!leerEstado(conexion, idHistoria, despues)) {
        error = "No se pudo leer la historia clínica después de actualizarla";
        ejecutar(conexion, "ROLLBACK;");
        return false;
    }

    for (const std::string& detalle : describirCambios(antes, despues)) {
        if (!registrarLog(conexion, idUsuario, "UPDATE", "historias_clinicas", idHistoria, detalle)) {
            error = sqlite3_errmsg(conexion);
            ejecutar(conexion, "ROLLBACK;");
            return false;
        }
    }

    if (!ejecutar(conexion, "COMMIT;")) {
        error = sqlite3_errmsg(conexion);
        ejecutar(conexion, "ROLLBACK;");
        return false;
    }

    error.clear();
    return true;
}

bool HistoriaRepository::eliminarPorPaciente(long long idPaciente,
                                             long long idUsuario,
                                             std::string& error) const {
    sqlite3* conexion = BaseDatosSqlite::instancia().obtenerConexion();
    if (conexion == nullptr || !BaseDatosSqlite::instancia().estaAbierta()) {
        error = "La base de datos no está disponible";
        return false;
    }

    if (!ejecutar(conexion, "BEGIN IMMEDIATE;")) {
        error = sqlite3_errmsg(conexion);
        return false;
    }

    sqlite3_stmt* borradoAlergias = nullptr;
    const std::string sqlAlergias =
        "DELETE FROM alergias "
        "WHERE id_historia IN (SELECT id_historia FROM historias_clinicas WHERE id_paciente = ?);";
    if (sqlite3_prepare_v2(conexion, sqlAlergias.c_str(), -1, &borradoAlergias, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(conexion);
        ejecutar(conexion, "ROLLBACK;");
        return false;
    }

    sqlite3_bind_int64(borradoAlergias, 1, idPaciente);
    if (sqlite3_step(borradoAlergias) != SQLITE_DONE) {
        error = sqlite3_errmsg(conexion);
        sqlite3_finalize(borradoAlergias);
        ejecutar(conexion, "ROLLBACK;");
        return false;
    }
    sqlite3_finalize(borradoAlergias);

    sqlite3_stmt* borradoHistoria = nullptr;
    const std::string sqlHistoria = "DELETE FROM historias_clinicas WHERE id_paciente = ?;";
    if (sqlite3_prepare_v2(conexion, sqlHistoria.c_str(), -1, &borradoHistoria, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(conexion);
        ejecutar(conexion, "ROLLBACK;");
        return false;
    }

    sqlite3_bind_int64(borradoHistoria, 1, idPaciente);
    if (sqlite3_step(borradoHistoria) != SQLITE_DONE) {
        error = sqlite3_errmsg(conexion);
        sqlite3_finalize(borradoHistoria);
        ejecutar(conexion, "ROLLBACK;");
        return false;
    }
    sqlite3_finalize(borradoHistoria);

    // sqlite3_changes se lee aquí, justo después del DELETE: con la fila de
    // auditoría y el COMMIT de por medio ya no reflejaría el borrado.
    if (sqlite3_changes(conexion) == 0) {
        error = "No existe una historia clínica para eliminar";
        ejecutar(conexion, "ROLLBACK;");
        return false;
    }

    // La historia ya no existe, así que el log se cuelga del paciente
    // (tabla 'pacientes'), que es lo que el historial del paciente consulta.
    if (!registrarLog(conexion, idUsuario, "DELETE", "pacientes", idPaciente,
                      "Eliminó la historia clínica del paciente.")) {
        error = sqlite3_errmsg(conexion);
        ejecutar(conexion, "ROLLBACK;");
        return false;
    }

    if (!ejecutar(conexion, "COMMIT;")) {
        error = sqlite3_errmsg(conexion);
        ejecutar(conexion, "ROLLBACK;");
        return false;
    }

    error.clear();
    return true;
}