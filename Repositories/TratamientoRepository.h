#pragma once

#include <string>
#include <vector>

#include "Models/Tratamiento.h"

class TratamientoRepository {
public:
    // Consulta todos los tratamientos registrados.
    bool listar(std::vector<Tratamiento>& tratamientos,
                std::string& error) const;

    // Consulta los tratamientos asociados a un paciente.
    bool listarPorPaciente(long long idPaciente,
                           std::vector<Tratamiento>& tratamientos,
                           std::string& error) const;

    // Busca un tratamiento por su identificador.
    bool buscarPorId(long long idTratamiento,
                     Tratamiento& tratamiento,
                     std::string& error) const;

    // Registra un nuevo tratamiento y devuelve su identificador.
    bool crear(const Tratamiento& tratamiento,
               long long& idGenerado,
               std::string& error) const;

    // Modifica los datos de un tratamiento.
    bool actualizar(const Tratamiento& tratamiento,
                    std::string& error) const;

    // Cambia el estado de un tratamiento.
    bool cambiarEstado(long long idTratamiento,
                       const std::string& estado,
                       const std::string& fechaFinalizacionEfectiva,
                       std::string& error) const;
};