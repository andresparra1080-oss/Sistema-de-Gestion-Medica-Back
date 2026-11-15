#pragma once

#include <string>
#include <vector>

#include "Cross/ResultadoOperacion.h"
#include "Models/AlertaSistema.h"

// Tabla alertas. "Leída" es la columna notificada.
class AlertaRepository {
public:
    // Alertas, las más recientes primero. soloSinResolver = true omite las
    // resueltas. Devuelve Exito o ErrorInterno.
    ResultadoOperacion listar(bool soloSinResolver,
                              std::vector<AlertaSistema>& alertas,
                              std::string& error) const;

    // notificada = 1, con su registro en logs_auditoria en una sola
    // transacción. Si ya estaba leída no cambia nada y devuelve Exito.
    // Devuelve Exito, NoEncontrado (la alerta no existe) o ErrorInterno.
    ResultadoOperacion marcarLeida(long long idAlerta,
                                   long long idUsuario,
                                   std::string& error) const;

    // Marca como leídas todas las alertas sin resolver que aún no lo estaban.
    // `cantidad` = cuántas cambiaron. Devuelve Exito o ErrorInterno.
    ResultadoOperacion marcarTodasLeidas(long long idUsuario,
                                         int& cantidad,
                                         std::string& error) const;
};