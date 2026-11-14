#pragma once

#include <string>
#include <vector>

#include "Cross/ResultadoOperacion.h"
#include "Models/Usuario.h"

class GestionUsuarioService {
public:
    ResultadoOperacion listar(long long idUsuarioSolicitante,
                              std::vector<Usuario>& usuarios,
                              std::string& mensaje) const;
    ResultadoOperacion crear(long long idUsuarioSolicitante,
                             const Usuario& datos,
                             const std::string& contrasena,
                             Usuario& creado,
                             std::string& mensaje) const;
};
