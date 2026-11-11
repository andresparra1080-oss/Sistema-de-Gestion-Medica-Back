#include "Repositories/EmergenciaRepository.h"

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

bool ejecutar(sqlite3* conexion, const char* sql) {
    return sqlite3_exec(conexion, sql, nullptr, nullptr, nullptr) == SQLITE_OK;
}

void enlazarTexto(sqlite3_stmt* sentencia, int posicion, const std::string& valor) {
    sqlite3_bind_text(sentencia, posicion, valor.c_str(), static_cast<int>(valor.size()), SQLITE_TRANSIENT);
}

// Corta a `maximo` bytes sin partir un carácter UTF-8 por la mitad y, si
// recortó, termina en "…". alertas.mensaje admite 300 caracteres; contar bytes
// es más estricto, así que nunca se pasa.
std::string recortarUtf8(const std::string& texto, std::size_t maximo) {
    if (texto.size() <= maximo) {
        return texto;
    }

    const std::string puntos = "\xE2\x80\xA6";  // …
    std::size_t corte = maximo - puntos.size();
    // Retrocede hasta el inicio de un carácter (los bytes de continuación son 10xxxxxx).
    while (corte > 0 && (static_cast<unsigned char>(texto[corte]) & 0xC0) == 0x80) {
        --corte;
    }
    return texto.substr(0, corte) + puntos;
}

std::string nombresDe(const std::vector<PacienteEmergencia>& pacientes) {
    std::string nombres;
    for (const PacienteEmergencia& paciente : pacientes) {
        if (!nombres.empty()) {
            nombres += ", ";
        }
        nombres += paciente.nombreCompleto;
    }
    return nombres;
}

// Prepara una sentencia; si falla deja el error y devuelve false.
bool preparar(sqlite3* conexion, const char* sql, sqlite3_stmt** sentencia, std::string& error) {
    if (sqlite3_prepare_v2(conexion, sql, -1, sentencia, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(conexion);
        return false;
    }
    return true;
}

// Ejecuta una sentencia ya preparada que no devuelve filas y la libera.
bool terminar(sqlite3* conexion, sqlite3_stmt* sentencia, std::string& error) {
    const bool bien = sqlite3_step(sentencia) == SQLITE_DONE;
    if (!bien) {
        error = sqlite3_errmsg(conexion);
    }
    sqlite3_finalize(sentencia);
    return bien;
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

}  // namespace

bool EmergenciaRepository::usuarioPuedeGestionarEmergencias(long long idUsuario) const {
    return existeEn(
        "SELECT 1 FROM usuarios u "
        "JOIN rol_permiso rp ON rp.id_rol = u.id_rol "
        "JOIN permisos p ON p.id_permiso = rp.id_permiso "
        "WHERE u.id_usuario = ? AND u.activo = 1 AND p.cod_acceso = 'GESTIONAR_EMERGENCIAS' LIMIT 1;",
        idUsuario);
}

bool EmergenciaRepository::listarTipos(std::vector<std::string>& tipos, std::string& error) const {
    sqlite3* conexion = BaseDatosSqlite::instancia().obtenerConexion();
    if (conexion == nullptr || !BaseDatosSqlite::instancia().estaAbierta()) {
        error = "La base de datos no está disponible";
        return false;
    }

    sqlite3_stmt* sentencia = nullptr;
    if (!preparar(conexion, "SELECT tipo_emergencia FROM protocolos ORDER BY id_protocolo;", &sentencia, error)) {
        return false;
    }

    tipos.clear();
    while (sqlite3_step(sentencia) == SQLITE_ROW) {
        tipos.push_back(leerTexto(sentencia, 0));
    }
    sqlite3_finalize(sentencia);
    return true;
}

bool EmergenciaRepository::buscarPaciente(long long idPaciente, PacienteEmergencia& paciente) const {
    sqlite3* conexion = BaseDatosSqlite::instancia().obtenerConexion();
    if (conexion == nullptr || !BaseDatosSqlite::instancia().estaAbierta()) {
        return false;
    }

    sqlite3_stmt* sentencia = nullptr;
    if (sqlite3_prepare_v2(conexion, "SELECT id_paciente, nombre_completo FROM pacientes WHERE id_paciente = ?;", -1,
                           &sentencia, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(sentencia, 1, idPaciente);

    bool encontrado = false;
    if (sqlite3_step(sentencia) == SQLITE_ROW) {
        paciente.idPaciente = sqlite3_column_int64(sentencia, 0);
        paciente.nombreCompleto = leerTexto(sentencia, 1);
        encontrado = true;
    }

    sqlite3_finalize(sentencia);
    return encontrado;
}

bool EmergenciaRepository::tieneHistoria(long long idPaciente) const {
    return existeEn("SELECT 1 FROM historias_clinicas WHERE id_paciente = ? LIMIT 1;", idPaciente);
}

bool EmergenciaRepository::crear(const NuevaEmergencia& datos,
                                 long long idUsuario,
                                 Emergencia& creada,
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

    // Cualquier fallo de aquí en adelante deshace todo: no puede quedar una
    // emergencia sin pacientes, ni pacientes sin su evento, ni sin alerta.
    auto fallar = [&]() {
        ejecutar(conexion, "ROLLBACK;");
        return false;
    };

    sqlite3_stmt* sentencia = nullptr;

    // 1. La emergencia. La fecha de inicio la pone la base (hora de Colombia,
    //    formato AAAA-MM-DD HH:MM:SS): el cliente no puede enviarla.
    if (!preparar(conexion,
                  "INSERT INTO emergencias (tipo, sintomas, lesiones, diagnostico_inicial, nivel_urgencia, "
                  "estado, fecha_inicio) "
                  "VALUES (?, ?, ?, ?, ?, 'ACTIVA', datetime('now', '-5 hours'));",
                  &sentencia, error)) {
        return fallar();
    }

    enlazarTexto(sentencia, 1, datos.tipo);
    enlazarTexto(sentencia, 2, datos.sintomas);
    if (datos.lesiones.empty()) {
        sqlite3_bind_null(sentencia, 3);
    } else {
        enlazarTexto(sentencia, 3, datos.lesiones);
    }
    enlazarTexto(sentencia, 4, datos.diagnosticoInicial);
    enlazarTexto(sentencia, 5, datos.nivelUrgencia);

    if (!terminar(conexion, sentencia, error)) {
        return fallar();
    }

    const long long idEmergencia = sqlite3_last_insert_rowid(conexion);

    // Se relee la fecha tal como quedó, para usar exactamente la misma en los
    // eventos del historial.
    std::string fechaInicio;
    if (!preparar(conexion, "SELECT fecha_inicio FROM emergencias WHERE id_emergencia = ?;", &sentencia, error)) {
        return fallar();
    }
    sqlite3_bind_int64(sentencia, 1, idEmergencia);
    if (sqlite3_step(sentencia) == SQLITE_ROW) {
        fechaInicio = leerTexto(sentencia, 0);
    }
    sqlite3_finalize(sentencia);

    if (fechaInicio.empty()) {
        error = "No se pudo leer la emergencia recién registrada";
        return fallar();
    }

    // 2 y 3. Cada paciente queda vinculado a la emergencia y con un evento en
    //        su historia clínica.
    const std::string motivo = "Emergencia #" + std::to_string(idEmergencia) + " (" + datos.tipo +
                               "). Síntomas: " + datos.sintomas;
    std::string diagnostico = "Diagnóstico inicial: " + datos.diagnosticoInicial;
    if (!datos.lesiones.empty()) {
        diagnostico += ". Lesiones: " + datos.lesiones;
    }

    for (const PacienteEmergencia& paciente : datos.pacientes) {
        if (!preparar(conexion, "INSERT INTO emergencia_paciente (id_emergencia, id_paciente) VALUES (?, ?);",
                      &sentencia, error)) {
            return fallar();
        }
        sqlite3_bind_int64(sentencia, 1, idEmergencia);
        sqlite3_bind_int64(sentencia, 2, paciente.idPaciente);
        if (!terminar(conexion, sentencia, error)) {
            return fallar();
        }

        if (!preparar(conexion,
                      "INSERT INTO eventos_medicos (id_historia, id_usuario_registra, tipo_evento, motivo, "
                      "diagnostico, fecha_evento, id_emergencia) "
                      "VALUES ((SELECT id_historia FROM historias_clinicas WHERE id_paciente = ?), "
                      "?, 'DIAGNOSTICO', ?, ?, ?, ?);",
                      &sentencia, error)) {
            return fallar();
        }
        sqlite3_bind_int64(sentencia, 1, paciente.idPaciente);
        sqlite3_bind_int64(sentencia, 2, idUsuario);
        enlazarTexto(sentencia, 3, motivo);
        enlazarTexto(sentencia, 4, diagnostico);
        enlazarTexto(sentencia, 5, fechaInicio);
        sqlite3_bind_int64(sentencia, 6, idEmergencia);
        if (!terminar(conexion, sentencia, error)) {
            return fallar();
        }
    }

    // 4. Alerta EMERGENCIA. La prioridad es el nivel de urgencia (mismo
    //    dominio). Nace sin notificar: la marca como notificada quien la
    //    entregue de verdad.
    const std::string nombres = nombresDe(datos.pacientes);
    const std::string mensajeAlerta = recortarUtf8(
        "EMERGENCIA #" + std::to_string(idEmergencia) + " (" + datos.tipo + ", urgencia " + datos.nivelUrgencia +
            "): " + nombres + ". Diagnóstico inicial: " + datos.diagnosticoInicial,
        300);

    if (!preparar(conexion,
                  "INSERT INTO alertas (id_emergencia, tipo_alerta, mensaje, prioridad) "
                  "VALUES (?, 'EMERGENCIA', ?, ?);",
                  &sentencia, error)) {
        return fallar();
    }
    sqlite3_bind_int64(sentencia, 1, idEmergencia);
    enlazarTexto(sentencia, 2, mensajeAlerta);
    enlazarTexto(sentencia, 3, datos.nivelUrgencia);
    if (!terminar(conexion, sentencia, error)) {
        return fallar();
    }

    const long long idAlerta = sqlite3_last_insert_rowid(conexion);

    // 5. Auditoría.
    if (!preparar(conexion,
                  "INSERT INTO logs_auditoria (id_usuario, accion, tabla_afectada, id_registro, detalle_cambio) "
                  "VALUES (?, 'INSERT', 'emergencias', ?, ?);",
                  &sentencia, error)) {
        return fallar();
    }
    sqlite3_bind_int64(sentencia, 1, idUsuario);
    sqlite3_bind_int64(sentencia, 2, idEmergencia);
    enlazarTexto(sentencia, 3,
                 recortarUtf8("Registró la emergencia " + std::to_string(idEmergencia) + " (" + datos.tipo +
                                  ") de " + nombres + ".",
                              1000));
    if (!terminar(conexion, sentencia, error)) {
        return fallar();
    }

    // Se lee la alerta tal como quedó (fecha_generacion la pone la base).
    AlertaEmergencia alerta;
    if (!preparar(conexion,
                  "SELECT id_alerta, tipo_alerta, mensaje, prioridad, notificada, fecha_generacion "
                  "FROM alertas WHERE id_alerta = ?;",
                  &sentencia, error)) {
        return fallar();
    }
    sqlite3_bind_int64(sentencia, 1, idAlerta);
    if (sqlite3_step(sentencia) == SQLITE_ROW) {
        alerta.idAlerta = sqlite3_column_int64(sentencia, 0);
        alerta.tipoAlerta = leerTexto(sentencia, 1);
        alerta.mensaje = leerTexto(sentencia, 2);
        alerta.prioridad = leerTexto(sentencia, 3);
        alerta.notificada = sqlite3_column_int(sentencia, 4) != 0;
        alerta.fechaGeneracion = leerTexto(sentencia, 5);
    }
    sqlite3_finalize(sentencia);

    if (!ejecutar(conexion, "COMMIT;")) {
        error = sqlite3_errmsg(conexion);
        return fallar();
    }

    creada.idEmergencia = idEmergencia;
    creada.tipo = datos.tipo;
    creada.sintomas = datos.sintomas;
    creada.lesiones = datos.lesiones;
    creada.diagnosticoInicial = datos.diagnosticoInicial;
    creada.nivelUrgencia = datos.nivelUrgencia;
    creada.estado = "ACTIVA";
    creada.fechaInicio = fechaInicio;
    creada.pacientes = datos.pacientes;
    creada.alerta = alerta;

    error.clear();
    return true;
}
