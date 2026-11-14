#include <sqlite3.h>

#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "Cross/PasswordHash.h"
#include "Database/SQLite.h"
#include "Services/GestionUsuarioService.h"

namespace {

int pruebasEjecutadas = 0;
int pruebasFallidas = 0;
std::filesystem::path rutaBaseDatos;

void comprobar(bool condicion, const std::string& nombre) {
    ++pruebasEjecutadas;
    if (condicion) {
        std::cout << "[OK] " << nombre << '\n';
        return;
    }

    ++pruebasFallidas;
    std::cerr << "[FALLO] " << nombre << '\n';
}

bool ejecutarSql(const std::string& sql) {
    sqlite3* conexion = BaseDatosSqlite::instancia().obtenerConexion();
    return conexion != nullptr &&
           sqlite3_exec(conexion, sql.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK;
}

long long consultarEntero(const std::string& sql) {
    sqlite3* conexion = BaseDatosSqlite::instancia().obtenerConexion();
    sqlite3_stmt* sentencia = nullptr;
    if (conexion == nullptr ||
        sqlite3_prepare_v2(conexion, sql.c_str(), -1, &sentencia, nullptr) != SQLITE_OK) {
        return -1;
    }

    long long resultado = -1;
    if (sqlite3_step(sentencia) == SQLITE_ROW) {
        resultado = sqlite3_column_int64(sentencia, 0);
    }
    sqlite3_finalize(sentencia);
    return resultado;
}

bool prepararBaseDatos() {
    const auto marcaTiempo = std::chrono::steady_clock::now().time_since_epoch().count();
    rutaBaseDatos = std::filesystem::temp_directory_path() /
                    ("heartmed-usuarios-tests-" + std::to_string(marcaTiempo) + ".sqlite3");

    if (!BaseDatosSqlite::instancia().abrir(rutaBaseDatos.string())) {
        std::cerr << "No se pudo abrir la base de datos temporal\n";
        return false;
    }

    const std::string esquema =
        "CREATE TABLE roles ("
        "id_rol INTEGER PRIMARY KEY, nombre_rol TEXT NOT NULL);"
        "CREATE TABLE usuarios ("
        "id_usuario INTEGER PRIMARY KEY AUTOINCREMENT,"
        "id_rol INTEGER NOT NULL REFERENCES roles(id_rol),"
        "nombre TEXT NOT NULL,"
        "correo TEXT NOT NULL COLLATE NOCASE UNIQUE,"
        "contrasena_hash TEXT NOT NULL,"
        "activo INTEGER NOT NULL DEFAULT 1,"
        "fecha_creacion TEXT NOT NULL DEFAULT '2026-11-14 08:00:00');"
        "CREATE TABLE logs_auditoria ("
        "id_log INTEGER PRIMARY KEY AUTOINCREMENT,"
        "id_usuario INTEGER NOT NULL,"
        "accion TEXT NOT NULL,"
        "tabla_afectada TEXT NOT NULL,"
        "id_registro INTEGER,"
        "detalle_cambio TEXT,"
        "fecha_modificacion TEXT NOT NULL DEFAULT '2026-11-14 08:00:00');"
        "INSERT INTO roles VALUES "
        "(1, 'Capitán'), (2, 'Cirujano'), (3, 'Inventario'), "
        "(4, 'Analista'), (5, 'Tripulante');"
        "INSERT INTO usuarios (id_usuario, id_rol, nombre, correo, contrasena_hash, activo) VALUES "
        "(1, 1, 'Law', 'law@example.com', 'hash', 1),"
        "(2, 2, 'Ikkaku', 'ikkaku@example.com', 'hash', 1),"
        "(3, 3, 'Shachi', 'shachi@example.com', 'hash', 1),"
        "(4, 4, 'Penguin', 'penguin@example.com', 'hash', 1),"
        "(5, 5, 'Bepo', 'bepo@example.com', 'hash', 1),"
        "(6, 5, 'Uni', 'uni@example.com', 'hash', 0);";

    if (!ejecutarSql(esquema)) {
        std::cerr << "No se pudo crear el esquema de prueba: "
                  << sqlite3_errmsg(BaseDatosSqlite::instancia().obtenerConexion()) << '\n';
        return false;
    }
    return true;
}

void probarPermisosListado() {
    GestionUsuarioService servicio;
    for (long long idSolicitante = 1; idSolicitante <= 4; ++idSolicitante) {
        std::vector<Usuario> usuarios;
        std::string mensaje;
        const auto resultado = servicio.listar(idSolicitante, usuarios, mensaje);
        comprobar(resultado == ResultadoOperacion::Exito && usuarios.size() == 6,
                  "El usuario " + std::to_string(idSolicitante) + " puede listar todos los usuarios");
        comprobar(!usuarios.empty() && usuarios.front().idUsuario == 1 &&
                      usuarios.back().idUsuario == 6 && !usuarios.back().activo,
                  "El listado está ordenado e incluye cuentas inactivas");
    }

    for (const long long idSolicitante : {0LL, 5LL, 6LL}) {
        std::vector<Usuario> usuarios;
        std::string mensaje;
        comprobar(servicio.listar(idSolicitante, usuarios, mensaje) == ResultadoOperacion::SinPermiso,
                  "El usuario " + std::to_string(idSolicitante) + " no puede listar usuarios");
    }
}

void probarPermisosCreacion() {
    GestionUsuarioService servicio;
    const long long cantidadInicial = consultarEntero("SELECT COUNT(*) FROM usuarios;");

    for (const long long idSolicitante : {0LL, 2LL, 3LL, 4LL, 5LL}) {
        Usuario datos;
        datos.idRol = 5;
        datos.nombre = "No autorizado";
        datos.correo = "no-autorizado-" + std::to_string(idSolicitante) + "@example.com";
        Usuario creado;
        std::string mensaje;
        comprobar(servicio.crear(idSolicitante, datos, "Clave123", creado, mensaje) ==
                      ResultadoOperacion::SinPermiso,
                  "El usuario " + std::to_string(idSolicitante) + " no puede crear usuarios");
    }

    comprobar(consultarEntero("SELECT COUNT(*) FROM usuarios;") == cantidadInicial,
              "Las solicitudes no autorizadas no insertan usuarios");
}

void probarValidaciones() {
    GestionUsuarioService servicio;

    struct CasoInvalido {
        std::string nombre;
        std::string correo;
        std::string contrasena;
        long long idRol;
        std::string descripcion;
    };

    const std::vector<CasoInvalido> casos = {
        {" ", "vacio@example.com", "Clave123", 5, "Rechaza un nombre vacío"},
        {std::string(101, 'N'), "largo@example.com", "Clave123", 5, "Rechaza un nombre largo"},
        {"Usuario", "correo-invalido", "Clave123", 5, "Rechaza un correo inválido"},
        {"Usuario", "correo con espacio@example.com", "Clave123", 5, "Rechaza espacios en el correo"},
        {"Usuario", "corto@example.com", "12345", 5, "Rechaza una contraseña corta"},
        {"Usuario", "rol-cero@example.com", "Clave123", 0, "Rechaza un ID de rol no positivo"},
        {"Usuario", "rol-inexistente@example.com", "Clave123", 99, "Rechaza un rol inexistente"},
    };

    for (const CasoInvalido& caso : casos) {
        Usuario datos;
        datos.nombre = caso.nombre;
        datos.correo = caso.correo;
        datos.idRol = caso.idRol;
        Usuario creado;
        std::string mensaje;
        comprobar(servicio.crear(1, datos, caso.contrasena, creado, mensaje) ==
                      ResultadoOperacion::DatosInvalidos,
                  caso.descripcion);
    }
}

void probarCreacionYAuditoria() {
    GestionUsuarioService servicio;
    Usuario datos;
    datos.idRol = 5;
    datos.nombre = "  Tripulante Nuevo  ";
    datos.correo = "  nuevo@example.com  ";
    Usuario creado;
    std::string mensaje;

    const auto resultado = servicio.crear(1, datos, "Secreta123", creado, mensaje);
    comprobar(resultado == ResultadoOperacion::Exito, "Law puede crear un usuario válido");
    comprobar(creado.idUsuario == 7 && creado.idRol == 5 && creado.nombreRol == "Tripulante" &&
                  creado.nombre == "Tripulante Nuevo" && creado.correo == "nuevo@example.com" &&
                  creado.activo && !creado.fechaCreacion.empty(),
              "La respuesta del alta contiene los datos normalizados y el estado activo");
    comprobar(consultarEntero(
                  "SELECT COUNT(*) FROM usuarios WHERE correo = 'nuevo@example.com' COLLATE NOCASE;") == 1,
              "El usuario nuevo queda persistido");
    comprobar(consultarEntero(
                  "SELECT COUNT(*) FROM logs_auditoria WHERE id_usuario = 1 "
                  "AND accion = 'INSERT' AND tabla_afectada = 'usuarios' AND id_registro = 7;") == 1,
              "La creación registra auditoría vinculada al usuario creador");

    sqlite3* conexion = BaseDatosSqlite::instancia().obtenerConexion();
    sqlite3_stmt* sentencia = nullptr;
    std::string hash;
    if (conexion != nullptr &&
        sqlite3_prepare_v2(conexion,
                           "SELECT contrasena_hash FROM usuarios WHERE id_usuario = 7;",
                           -1, &sentencia, nullptr) == SQLITE_OK &&
        sqlite3_step(sentencia) == SQLITE_ROW) {
        const unsigned char* valor = sqlite3_column_text(sentencia, 0);
        if (valor != nullptr) {
            hash = reinterpret_cast<const char*>(valor);
        }
    }
    if (sentencia != nullptr) {
        sqlite3_finalize(sentencia);
    }
    comprobar(hash != "Secreta123" && HashContrasena::verificar("Secreta123", hash),
              "La contraseña se persiste como hash y permite validación");

    datos.nombre = "Otro usuario";
    datos.correo = "NUEVO@example.com";
    Usuario duplicado;
    comprobar(servicio.crear(1, datos, "Secreta123", duplicado, mensaje) ==
                  ResultadoOperacion::Duplicado,
              "El alta rechaza correos duplicados sin distinguir mayúsculas");
    comprobar(consultarEntero("SELECT COUNT(*) FROM usuarios;") == 7 &&
                  consultarEntero("SELECT COUNT(*) FROM logs_auditoria;") == 1,
              "El correo duplicado no crea usuario ni auditoría adicional");
}

void probarRollbackDeAuditoria() {
    comprobar(ejecutarSql(
                  "CREATE TRIGGER fallar_auditoria_usuario BEFORE INSERT ON logs_auditoria "
                  "BEGIN SELECT RAISE(ABORT, 'fallo de auditoria'); END;"),
              "Prepara el fallo controlado de auditoría");

    GestionUsuarioService servicio;
    Usuario datos;
    datos.idRol = 5;
    datos.nombre = "Rollback";
    datos.correo = "rollback@example.com";
    Usuario creado;
    std::string mensaje;
    comprobar(servicio.crear(1, datos, "Secreta123", creado, mensaje) ==
                  ResultadoOperacion::ErrorInterno,
              "El servicio informa el error al fallar la auditoría");
    comprobar(consultarEntero(
                  "SELECT COUNT(*) FROM usuarios WHERE correo = 'rollback@example.com';") == 0,
              "Un fallo de auditoría revierte la creación del usuario");
    comprobar(ejecutarSql("DROP TRIGGER fallar_auditoria_usuario;"),
              "Limpia el trigger temporal de prueba");
}

}  // namespace

int main() {
    if (!prepararBaseDatos()) {
        BaseDatosSqlite::instancia().cerrar();
        if (!rutaBaseDatos.empty()) {
            std::error_code error;
            std::filesystem::remove(rutaBaseDatos, error);
        }
        return 1;
    }

    probarPermisosListado();
    probarPermisosCreacion();
    probarValidaciones();
    probarCreacionYAuditoria();
    probarRollbackDeAuditoria();

    BaseDatosSqlite::instancia().cerrar();
    std::error_code error;
    std::filesystem::remove(rutaBaseDatos, error);
    if (error) {
        std::cerr << "No se pudo eliminar la base temporal de prueba: " << error.message() << '\n';
        ++pruebasFallidas;
    }

    std::cout << "\nPruebas: " << pruebasEjecutadas
              << ", fallidas: " << pruebasFallidas << '\n';
    return pruebasFallidas == 0 ? 0 : 1;
}
