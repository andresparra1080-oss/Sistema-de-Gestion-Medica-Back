#pragma once

#include <string>

#include "Cross/ResultadoOperacion.h"
#include "Models/HistoriaClinica.h"
#include "Repositories/HistoriaRepository.h"

class HistoriaService {
public:
    ResultadoOperacion consultar(long long idPaciente,
                                 HistoriaClinica& historia,
                                 std::string& mensaje) const;

    ResultadoOperacion actualizar(long long idPaciente, const CambiosHistoria& cambios,
                              const std::string& correoUsuario,
                              HistoriaClinica& actualizada, std::string& mensaje) const;

    ResultadoOperacion eliminar(long long idPaciente, const std::string& correoUsuario,
                            std::string& mensaje) const;
};
