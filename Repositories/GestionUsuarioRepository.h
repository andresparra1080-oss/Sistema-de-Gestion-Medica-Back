#pragma once

#include <string>
#include <vector>

#include "Models/Usuario.h"

enum class ResultadoCreacionUsuario {
    Exito,
    CorreoDuplicado,
    Error,
};

class GestionUsuarioRepository {
public:
    bool listar(std::vector<Usuario>& usuarios, std::string& error) const;
    bool existeRol(long long idRol, std::string& error) const;
    ResultadoCreacionUsuario crear(const Usuario& usuario,
                                   const std::string& hashContrasena,
                                   long long idUsuarioCreador,
                                   Usuario& creado,
                                   std::string& error) const;
};
