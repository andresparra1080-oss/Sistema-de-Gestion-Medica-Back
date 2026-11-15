#pragma once

#include <string>
#include <vector>

#include "Cross/ResultadoOperacion.h"
#include "Models/Protocolo.h"

// Protocolos de emergencia (tablas protocolos y pasos_protocolo). Solo lectura.
class ProtocoloRepository {
public:
    // Todos los protocolos con sus pasos. Devuelve Exito o ErrorInterno.
    ResultadoOperacion listar(std::vector<Protocolo>& protocolos, std::string& error) const;

    // El protocolo de un tipo (TRAUMATISMO, HEMORRAGIA, ...) con sus pasos.
    // Devuelve Exito, NoEncontrado (no hay protocolo de ese tipo) o ErrorInterno.
    ResultadoOperacion obtenerPorTipo(const std::string& tipo,
                                      Protocolo& protocolo,
                                      std::string& error) const;
};