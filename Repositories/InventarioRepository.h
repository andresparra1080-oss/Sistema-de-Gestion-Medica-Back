#pragma once

#include <string>
#include <vector>

#include "Models/Medicamento.h"

class InventarioRepository {
public:
    bool existeId(long long idMedicamento) const;
    // Devuelve los medicamentos e insumos con sus lotes ya asociados.
    bool listar(std::vector<Medicamento>& medicamentos, std::string& error) const;
    // Elimina un medicamento solo si no tiene lotes ni dependencias registradas.
    bool eliminar(long long idMedicamento, std::string& error) const;
};
