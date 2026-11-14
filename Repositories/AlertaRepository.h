#pragma once

#include <optional>
#include <string>
#include <vector>

#include "Cross/ResultadoOperacion.h"
#include "Models/Alerta.h"

class AlertaRepository {
public:
    // Lista alertas ordenadas de la más reciente a la más antigua.
    // resueltaFiltro: std::nullopt = todas, 0 = abiertas, 1 = resueltas.
    // tipoFiltro: vacío = todos los tipos.
    bool listar(const std::optional<int>& resueltaFiltro,
                const std::string& tipoFiltro,
                std::vector<Alerta>& alertas,
                std::string& error) const;

    // Marca una alerta como leída/resuelta (resuelta = 1, fecha_resolucion = hora Colombia)
    // y registra la acción en logs_auditoria dentro de una transacción.
    ResultadoOperacion marcarLeida(long long idAlerta,
                                   long long idUsuario,
                                   Alerta& actualizada,
                                   std::string& error) const;

    // Marca todas las alertas abiertas (resuelta = 0) como leídas/resueltas
    // y registra la operación en logs_auditoria.
    ResultadoOperacion marcarTodasLeidas(long long idUsuario,
                                         int& cantidadActualizada,
                                         std::string& error) const;
};