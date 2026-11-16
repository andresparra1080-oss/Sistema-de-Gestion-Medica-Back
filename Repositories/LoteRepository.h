#pragma once

#include <string>
#include <vector>

#include "Models/Lote.h"

class LoteRepository {
public:
    bool listar(
        std::vector<Lote>& lotes,
        std::string& error
    ) const;

    bool crear(
        const Lote& datos,
        long long& idGenerado,
        std::string& error
    ) const;
};