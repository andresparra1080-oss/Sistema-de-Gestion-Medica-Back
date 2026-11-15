#pragma once

#include <string>
#include <vector>

#include "Cross/ResultadoOperacion.h"
#include "Models/Protocolo.h"
#include "Repositories/ProtocoloRepository.h"

// Consulta de protocolos de emergencia. `correoUsuario` sale del token; basta
// una cuenta activa, porque en una emergencia el protocolo debe verse sin
// trabas.
class ProtocoloService {
public:
    ResultadoOperacion listar(const std::string& correoUsuario,
                              std::vector<Protocolo>& protocolos,
                              std::string& mensaje) const;

    // `tipo` se normaliza (espacios y mayúsculas) antes de buscar.
    ResultadoOperacion obtenerPorTipo(const std::string& tipo,
                                      const std::string& correoUsuario,
                                      Protocolo& protocolo,
                                      std::string& mensaje) const;
};