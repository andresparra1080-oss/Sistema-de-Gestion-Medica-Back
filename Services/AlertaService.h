#pragma once

#include <string>
#include <vector>

#include "Cross/ResultadoOperacion.h"
#include "Models/AlertaSistema.h"
#include "Repositories/AlertaRepository.h"

// Panel de alertas (KAN-58). `correoUsuario` sale del token: quien marca una
// alerta como leída es quien firma la petición. Basta una cuenta activa.
class AlertaService {
public:
    // soloSinResolver = true omite las alertas ya resueltas.
    ResultadoOperacion listar(bool soloSinResolver,
                              const std::string& correoUsuario,
                              std::vector<AlertaSistema>& alertas,
                              std::string& mensaje) const;

    ResultadoOperacion marcarLeida(long long idAlerta,
                                   const std::string& correoUsuario,
                                   std::string& mensaje) const;

    // `cantidad` = cuántas alertas pasaron de sin leer a leídas.
    ResultadoOperacion marcarTodasLeidas(const std::string& correoUsuario,
                                         int& cantidad,
                                         std::string& mensaje) const;
};