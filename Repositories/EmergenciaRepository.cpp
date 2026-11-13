#include "Repositories/EmergenciaRepository.h"

#include <string>
#include <vector>
#include <mutex>

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
namespace {

// La conexión de SQLite es una sola y la comparten todos los hilos del
// servidor; BEGIN IMMEDIATE no se puede anidar. Estas escrituras entran de a una.
std::mutex mutexEscrituras;

// Lee una emergencia con sus pacientes (sin alerta).
ResultadoOperacion leerEmergencia(sqlite3* conexion,
                                  long long idEmergencia,
                                  Emergencia& emergencia,
                                  std::string& error) {
    sqlite3_stmt* sentencia = nullptr;
    if (!preparar(conexion,
                  "SELECT id_emergencia, tipo, sintomas, lesiones, diagnostico_inicial, nivel_urgencia, "
                  "estado, fecha_inicio, fecha_fin FROM emergencias WHERE id_emergencia = ?;",
                  &sentencia, error)) {
        return ResultadoOperacion::ErrorInterno;
    }
    sqlite3_bind_int64(sentencia, 1, idEmergencia);

    const int paso = sqlite3_step(sentencia);
    if (paso == SQLITE_DONE) {
        sqlite3_finalize(sentencia);
        error = "No existe la emergencia " + std::to_string(idEmergencia);
        return ResultadoOperacion::NoEncontrado;
    }
    if (paso != SQLITE_ROW) {
        error = sqlite3_errmsg(conexion);
        sqlite3_finalize(sentencia);
        return ResultadoOperacion::ErrorInterno;
    }

    Emergencia leida;
    leida.idEmergencia = sqlite3_column_int64(sentencia, 0);
    leida.tipo = leerTexto(sentencia, 1);
    leida.sintomas = leerTexto(sentencia, 2);
    leida.lesiones = leerTexto(sentencia, 3);
    leida.diagnosticoInicial = leerTexto(sentencia, 4);
    leida.nivelUrgencia = leerTexto(sentencia, 5);
    leida.estado = leerTexto(sentencia, 6);
    leida.fechaInicio = leerTexto(sentencia, 7);
    if (sqlite3_column_type(sentencia, 8) != SQLITE_NULL) {
        leida.fechaFin = leerTexto(sentencia, 8);
    }
    sqlite3_finalize(sentencia);

    if (!preparar(conexion,
                  "SELECT p.id_paciente, p.nombre_completo FROM emergencia_paciente ep "
                  "JOIN pacientes p ON p.id_paciente = ep.id_paciente "
                  "WHERE ep.id_emergencia = ? ORDER BY p.id_paciente;",
                  &sentencia, error)) {
        return ResultadoOperacion::ErrorInterno;
    }
    sqlite3_bind_int64(sentencia, 1, idEmergencia);
    while (sqlite3_step(sentencia) == SQLITE_ROW) {
        PacienteEmergencia paciente;
        paciente.idPaciente = sqlite3_column_int64(sentencia, 0);
        paciente.nombreCompleto = leerTexto(sentencia, 1);
        leida.pacientes.push_back(paciente);
    }
    sqlite3_finalize(sentencia);

    emergencia = leida;
    return ResultadoOperacion::Exito;
}

// Bitácora: una fila en logs_auditoria. Se llama DENTRO de la transacción del
// cambio, así que el cambio y su registro se guardan juntos o no se guarda ninguno.
bool registrarAuditoria(sqlite3* conexion,
                        long long idUsuario,
                        const char* accion,
                        const char* tabla,
                        long long idRegistro,
                        const std::string& detalle,
                        std::string& error) {
    sqlite3_stmt* sentencia = nullptr;
    if (!preparar(conexion,
                  "INSERT INTO logs_auditoria (id_usuario, accion, tabla_afectada, id_registro, detalle_cambio) "
                  "VALUES (?, ?, ?, ?, ?);",
                  &sentencia, error)) {
        return false;
    }
    sqlite3_bind_int64(sentencia, 1, idUsuario);
    sqlite3_bind_text(sentencia, 2, accion, -1, SQLITE_STATIC);
    sqlite3_bind_text(sentencia, 3, tabla, -1, SQLITE_STATIC);
    sqlite3_bind_int64(sentencia, 4, idRegistro);
    enlazarTexto(sentencia, 5, recortarUtf8(detalle, 1000));
    return terminar(conexion, sentencia, error);
}

ResultadoOperacion transicionar(long long idEmergencia,
                                long long idUsuario,
                                const char* sqlActualizar,
                                const std::string& mensajeConflicto,
                                Emergencia& actualizada,
                                std::string& error) {
    sqlite3* conexion = BaseDatosSqlite::instancia().obtenerConexion();
    if (conexion == nullptr || !BaseDatosSqlite::instancia().estaAbierta()) {
        error = "La base de datos no está disponible";
        return ResultadoOperacion::ErrorInterno;
    }

    std::lock_guard<std::mutex> candado(mutexEscrituras);

    if (!ejecutar(conexion, "BEGIN IMMEDIATE;")) {
        error = sqlite3_errmsg(conexion);
        return ResultadoOperacion::ErrorInterno;
    }

    auto fallar = [&](ResultadoOperacion resultado) {
        ejecutar(conexion, "ROLLBACK;");
        return resultado;
    };

    Emergencia antes;
    ResultadoOperacion lectura = leerEmergencia(conexion, idEmergencia, antes, error);
    if (lectura != ResultadoOperacion::Exito) {
        return fallar(lectura);
    }

    sqlite3_stmt* sentencia = nullptr;
    if (!preparar(conexion, sqlActualizar, &sentencia, error)) {
        return fallar(ResultadoOperacion::ErrorInterno);
    }
    sqlite3_bind_int64(sentencia, 1, idEmergencia);
    if (!terminar(conexion, sentencia, error)) {
        return fallar(ResultadoOperacion::ErrorInterno);
    }

    // 0 filas = el estado de origen no era el permitido.
    if (sqlite3_changes(conexion) == 0) {
        error = mensajeConflicto + " (estado actual: " + antes.estado + ")";
        return fallar(ResultadoOperacion::Conflicto);
    }

    Emergencia despues;
    lectura = leerEmergencia(conexion, idEmergencia, despues, error);
    if (lectura != ResultadoOperacion::Exito) {
        return fallar(ResultadoOperacion::ErrorInterno);
    }

    std::string detalle = "Cambio de estado de la emergencia " + std::to_string(idEmergencia) + ": " +
                          antes.estado + " -> " + despues.estado;
    if (despues.fechaFin.has_value()) {
        detalle += ". fecha_fin: " + *despues.fechaFin;
    }
    if (!registrarAuditoria(conexion, idUsuario, "UPDATE", "emergencias", idEmergencia, detalle, error)) {
        return fallar(ResultadoOperacion::ErrorInterno);
    }

    if (!ejecutar(conexion, "COMMIT;")) {
        error = sqlite3_errmsg(conexion);
        return fallar(ResultadoOperacion::ErrorInterno);
    }

    actualizada = despues;
    error.clear();
    return ResultadoOperacion::Exito;
}

}  // namespace

ResultadoOperacion EmergenciaRepository::listarAcciones(long long idEmergencia,
                                                        std::vector<AccionEmergencia>& acciones,
                                                        std::string& error) const {
    sqlite3* conexion = BaseDatosSqlite::instancia().obtenerConexion();
    if (conexion == nullptr || !BaseDatosSqlite::instancia().estaAbierta()) {
        error = "La base de datos no está disponible";
        return ResultadoOperacion::ErrorInterno;
    }

    if (!existeEn("SELECT 1 FROM emergencias WHERE id_emergencia = ? LIMIT 1;", idEmergencia)) {
        error = "No existe la emergencia " + std::to_string(idEmergencia);
        return ResultadoOperacion::NoEncontrado;
    }

    sqlite3_stmt* sentencia = nullptr;
    if (!preparar(conexion,
                  "SELECT a.id_accion, a.id_emergencia, a.id_usuario_ejecutor, u.nombre, "
                  "a.descripcion_accion, a.fecha_hora "
                  "FROM acciones_emergencia a "
                  "JOIN usuarios u ON u.id_usuario = a.id_usuario_ejecutor "
                  "WHERE a.id_emergencia = ? ORDER BY a.fecha_hora, a.id_accion;",
                  &sentencia, error)) {
        return ResultadoOperacion::ErrorInterno;
    }
    sqlite3_bind_int64(sentencia, 1, idEmergencia);

    acciones.clear();
    while (sqlite3_step(sentencia) == SQLITE_ROW) {
        AccionEmergencia accion;
        accion.idAccion = sqlite3_column_int64(sentencia, 0);
        accion.idEmergencia = sqlite3_column_int64(sentencia, 1);
        accion.idUsuarioEjecutor = sqlite3_column_int64(sentencia, 2);
        accion.nombreEjecutor = leerTexto(sentencia, 3);
        accion.descripcion = leerTexto(sentencia, 4);
        accion.fechaHora = leerTexto(sentencia, 5);
        acciones.push_back(accion);
    }
    sqlite3_finalize(sentencia);

    error.clear();
    return ResultadoOperacion::Exito;
}

ResultadoOperacion EmergenciaRepository::registrarAccion(long long idEmergencia,
                                                         long long idUsuario,
                                                         const std::string& descripcion,
                                                         AccionEmergencia& creada,
                                                         std::string& error) const {
    sqlite3* conexion = BaseDatosSqlite::instancia().obtenerConexion();
    if (conexion == nullptr || !BaseDatosSqlite::instancia().estaAbierta()) {
        error = "La base de datos no está disponible";
        return ResultadoOperacion::ErrorInterno;
    }

    std::lock_guard<std::mutex> candado(mutexEscrituras);

    if (!ejecutar(conexion, "BEGIN IMMEDIATE;")) {
        error = sqlite3_errmsg(conexion);
        return ResultadoOperacion::ErrorInterno;
    }

    auto fallar = [&](ResultadoOperacion resultado) {
        ejecutar(conexion, "ROLLBACK;");
        return resultado;
    };

    // Con la transacción abierta nadie más escribe: lo que se lee aquí sigue
    // siendo cierto al insertar.
    Emergencia emergencia;
    const ResultadoOperacion lectura = leerEmergencia(conexion, idEmergencia, emergencia, error);
    if (lectura != ResultadoOperacion::Exito) {
        return fallar(lectura);
    }

    if (emergencia.estado == "FINALIZADA") {
        error = "La emergencia " + std::to_string(idEmergencia) +
                " ya está FINALIZADA: no admite más acciones";
        return fallar(ResultadoOperacion::Conflicto);
    }

    // fecha_hora no se envía: la base pone la hora actual de Colombia.
    sqlite3_stmt* sentencia = nullptr;
    if (!preparar(conexion,
                  "INSERT INTO acciones_emergencia (id_emergencia, id_usuario_ejecutor, descripcion_accion) "
                  "VALUES (?, ?, ?);",
                  &sentencia, error)) {
        return fallar(ResultadoOperacion::ErrorInterno);
    }
    sqlite3_bind_int64(sentencia, 1, idEmergencia);
    sqlite3_bind_int64(sentencia, 2, idUsuario);
    enlazarTexto(sentencia, 3, descripcion);
    if (!terminar(conexion, sentencia, error)) {
        return fallar(ResultadoOperacion::ErrorInterno);
    }

    const long long idAccion = sqlite3_last_insert_rowid(conexion);

    // Se relee tal como quedó (fecha_hora la puso la base).
    AccionEmergencia leida;
    bool encontrada = false;
    if (!preparar(conexion,
                  "SELECT a.id_accion, a.id_emergencia, a.id_usuario_ejecutor, u.nombre, "
                  "a.descripcion_accion, a.fecha_hora "
                  "FROM acciones_emergencia a "
                  "JOIN usuarios u ON u.id_usuario = a.id_usuario_ejecutor "
                  "WHERE a.id_accion = ?;",
                  &sentencia, error)) {
        return fallar(ResultadoOperacion::ErrorInterno);
    }
    sqlite3_bind_int64(sentencia, 1, idAccion);
    if (sqlite3_step(sentencia) == SQLITE_ROW) {
        leida.idAccion = sqlite3_column_int64(sentencia, 0);
        leida.idEmergencia = sqlite3_column_int64(sentencia, 1);
        leida.idUsuarioEjecutor = sqlite3_column_int64(sentencia, 2);
        leida.nombreEjecutor = leerTexto(sentencia, 3);
        leida.descripcion = leerTexto(sentencia, 4);
        leida.fechaHora = leerTexto(sentencia, 5);
        encontrada = true;
    }
    sqlite3_finalize(sentencia);

    if (!encontrada) {
        error = "No se pudo leer la acción recién registrada";
        return fallar(ResultadoOperacion::ErrorInterno);
    }

    if (!registrarAuditoria(conexion, idUsuario, "INSERT", "acciones_emergencia", idAccion,
                            "Registró una acción en la emergencia " + std::to_string(idEmergencia) + ": " +
                                descripcion,
                            error)) {
        return fallar(ResultadoOperacion::ErrorInterno);
    }

    if (!ejecutar(conexion, "COMMIT;")) {
        error = sqlite3_errmsg(conexion);
        return fallar(ResultadoOperacion::ErrorInterno);
    }

    creada = leida;
    error.clear();
    return ResultadoOperacion::Exito;
}

ResultadoOperacion EmergenciaRepository::marcarEnAtencion(long long idEmergencia,
                                                          long long idUsuario,
                                                          Emergencia& actualizada,
                                                          std::string& error) const {
    return transicionar(idEmergencia, idUsuario,
                        "UPDATE emergencias SET estado = 'EN_ATENCION' "
                        "WHERE id_emergencia = ? AND estado = 'ACTIVA';",
                        "Solo una emergencia ACTIVA puede pasar a EN_ATENCION", actualizada, error);
}

ResultadoOperacion EmergenciaRepository::finalizar(long long idEmergencia,
                                                   long long idUsuario,
                                                   Emergencia& actualizada,
                                                   std::string& error) const {
    // fecha_fin: hora actual de Colombia, puesta por la base (la tabla la exige al finalizar).
    return transicionar(idEmergencia, idUsuario,
                        "UPDATE emergencias SET estado = 'FINALIZADA', fecha_fin = datetime('now', '-5 hours') "
                        "WHERE id_emergencia = ? AND estado IN ('ACTIVA', 'EN_ATENCION');",
                        "Solo una emergencia ACTIVA o EN_ATENCION puede finalizarse", actualizada, error);
}
