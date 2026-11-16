#pragma once

#include <string>
#include <vector>

#include "Cross/ResultadoOperacion.h"
#include "Models/Lote.h"

class LoteService {
public:
    ResultadoOperacion listar(
        const std::string& correoUsuario,
        std::vector<Lote>& lotes,
        std::string& mensaje
    ) const;

    ResultadoOperacion crear(
        const Lote& datos,
        const std::string& correoUsuario,
        Lote& creado,
        std::string& mensaje
    ) const;
};