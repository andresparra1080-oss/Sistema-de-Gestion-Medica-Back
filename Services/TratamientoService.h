#pragma once

#include <string>
#include <vector>

#include "Cross/ResultadoOperacion.h"
#include "Models/Tratamiento.h"

class TratamientoService {
public:
    // Consulta todos los tratamientos registrados.
    ResultadoOperacion listar(std::vector<Tratamiento>& tratamientos,
                              std::string& mensaje) const;

    // Consulta los tratamientos de un paciente.
    ResultadoOperacion listarPorPaciente(long long idPaciente,
                                         std::vector<Tratamiento>& tratamientos,
                                         std::string& mensaje) const;

    // Registra un tratamiento usando el usuario autenticado.
    ResultadoOperacion registrar(const Tratamiento& datos,
                                 const std::string& correoUsuario,
                                 Tratamiento& registrado,
                                 std::string& mensaje) const;

    // Actualiza los datos de un tratamiento.
    ResultadoOperacion actualizar(const Tratamiento& datos,
                                  std::string& mensaje) const;

    // Suspende o finaliza un tratamiento.
    ResultadoOperacion cambiarEstado(long long idTratamiento,
                                     const std::string& estado,
                                     const std::string& fechaFinalizacion,
                                     std::string& mensaje) const;
};