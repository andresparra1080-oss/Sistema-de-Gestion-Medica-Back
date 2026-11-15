#include "Repositories/ProtocoloRepository.h"

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

sqlite3* conexionAbierta() {
    if (!BaseDatosSqlite::instancia().estaAbierta()) {
        return nullptr;
    }
    return BaseDatosSqlite::instancia().obtenerConexion();
}

bool cargarPasos(sqlite3* conexion, Protocolo& protocolo, std::string& error) {
    sqlite3_stmt* sentencia = nullptr;
    if (sqlite3_prepare_v2(conexion,
                           "SELECT id_paso, numero, descripcion FROM pasos_protocolo "
                           "WHERE id_protocolo = ? ORDER BY numero, id_paso;",
                           -1, &sentencia, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(conexion);
        return false;
    }

    sqlite3_bind_int64(sentencia, 1, protocolo.idProtocolo);

    protocolo.pasos.clear();
    while (sqlite3_step(sentencia) == SQLITE_ROW) {
        PasoProtocolo paso;
        paso.idPaso = sqlite3_column_int64(sentencia, 0);
        paso.numero = sqlite3_column_int(sentencia, 1);
        paso.descripcion = leerTexto(sentencia, 2);
        protocolo.pasos.push_back(paso);
    }
    sqlite3_finalize(sentencia);
    return true;
}

}  // namespace

ResultadoOperacion ProtocoloRepository::listar(std::vector<Protocolo>& protocolos, std::string& error) const {
    sqlite3* conexion = conexionAbierta();
    if (conexion == nullptr) {
        error = "La base de datos no está disponible";
        return ResultadoOperacion::ErrorInterno;
    }

    sqlite3_stmt* sentencia = nullptr;
    if (sqlite3_prepare_v2(conexion,
                           "SELECT id_protocolo, tipo_emergencia, nombre FROM protocolos ORDER BY id_protocolo;",
                           -1, &sentencia, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(conexion);
        return ResultadoOperacion::ErrorInterno;
    }

    protocolos.clear();
    while (sqlite3_step(sentencia) == SQLITE_ROW) {
        Protocolo protocolo;
        protocolo.idProtocolo = sqlite3_column_int64(sentencia, 0);
        protocolo.tipoEmergencia = leerTexto(sentencia, 1);
        protocolo.nombre = leerTexto(sentencia, 2);
        protocolos.push_back(protocolo);
    }
    sqlite3_finalize(sentencia);

    for (Protocolo& protocolo : protocolos) {
        if (!cargarPasos(conexion, protocolo, error)) {
            return ResultadoOperacion::ErrorInterno;
        }
    }
    return ResultadoOperacion::Exito;
}

ResultadoOperacion ProtocoloRepository::obtenerPorTipo(const std::string& tipo,
                                                       Protocolo& protocolo,
                                                       std::string& error) const {
    sqlite3* conexion = conexionAbierta();
    if (conexion == nullptr) {
        error = "La base de datos no está disponible";
        return ResultadoOperacion::ErrorInterno;
    }

    sqlite3_stmt* sentencia = nullptr;
    if (sqlite3_prepare_v2(conexion,
                           "SELECT id_protocolo, tipo_emergencia, nombre FROM protocolos "
                           "WHERE tipo_emergencia = ?;",
                           -1, &sentencia, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(conexion);
        return ResultadoOperacion::ErrorInterno;
    }

    sqlite3_bind_text(sentencia, 1, tipo.c_str(), static_cast<int>(tipo.size()), SQLITE_TRANSIENT);

    Protocolo leido;
    const bool encontrado = sqlite3_step(sentencia) == SQLITE_ROW;
    if (encontrado) {
        leido.idProtocolo = sqlite3_column_int64(sentencia, 0);
        leido.tipoEmergencia = leerTexto(sentencia, 1);
        leido.nombre = leerTexto(sentencia, 2);
    }
    sqlite3_finalize(sentencia);

    if (!encontrado) {
        error = "No hay un protocolo para el tipo " + tipo;
        return ResultadoOperacion::NoEncontrado;
    }

    if (!cargarPasos(conexion, leido, error)) {
        return ResultadoOperacion::ErrorInterno;
    }

    protocolo = leido;
    return ResultadoOperacion::Exito;
}