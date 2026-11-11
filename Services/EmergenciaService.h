#pragma once

#include <string>

#include "Cross/ResultadoOperacion.h"
#include "Models/Emergencia.h"
#include "Repositories/EmergenciaRepository.h"

class EmergenciaService {
public:
    // Registra una emergencia y la asocia a uno o varios tripulantes.
    // `correoUsuario` sale del token, no del cuerpo: quien registra es quien
    // firma la petición. En `datos.pacientes` solo se lee idPaciente; los
    // nombres los completa el servicio desde la base.
    ResultadoOperacion registrar(const NuevaEmergencia& datos,
                                 const std::string& correoUsuario,
                                 Emergencia& creada,
                                 std::string& mensaje) const;
};
