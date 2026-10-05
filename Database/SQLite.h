#pragma once

#include <string>

#include <sqlite3.h>


class BaseDatosSqlite {
public:
    static BaseDatosSqlite& instancia();

    bool abrir(const std::string& ruta);
    void cerrar();

    sqlite3* obtenerConexion() const;
    bool estaAbierta() const;
    std::string obtenerUltimoError() const;

private:
    BaseDatosSqlite();
    ~BaseDatosSqlite();

    sqlite3* conexion_;
    std::string rutaBaseDatos_;
    std::string ultimoError_;
};