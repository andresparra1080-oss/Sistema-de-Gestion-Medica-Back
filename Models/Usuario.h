#pragma once

#include <string>

struct Usuario {
    long long idUsuario = 0;
    long long idRol = 0;
    std::string nombreRol;
    std::string nombre;
    std::string correo;
    bool activo = true;
    std::string fechaCreacion;
};
