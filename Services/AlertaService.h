#pragma once

#include <optional>
#include <string>
#include <vector>

#include "Cross/ResultadoOperacion.h"
#include "Models/Alerta.h"

class AlertaService {
public:
    ResultadoOperacion listar(const std::optional<int>& resueltaFiltro,
                              const std::string& tipoFiltro,
                              const std::string& correoUsuario,
                              std::vector<Alerta>& alertas,
                              std::string& mensaje) const;

    ResultadoOperacion marcarLeida(long long idAlerta,
                                   const std::string& correoUsuario,
                                   Alerta& actualizada,
                                   std::string& mensaje) const;

    ResultadoOperacion marcarTodasLeidas(const std::string& correoUsuario,
                                         int& cantidadActualizada,
                                         std::string& mensaje) const;
};