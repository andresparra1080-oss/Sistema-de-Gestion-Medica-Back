#pragma once

#include <optional>
#include <string>

// Una fila de logs_auditoria que se muestra en el historial de una emergencia (KAN-54):
// la emergencia misma (registro, cambios de estado) y sus acciones de atención.
struct CambioEmergencia {
    long long idLog = 0;
    long long idUsuario = 0;
    std::string nombreUsuario;   // usuarios.nombre, resuelto al leer
    std::string accion;          // INSERT, UPDATE...
    std::string tablaAfectada;   // emergencias, acciones_emergencia...
    std::optional<long long> idRegistro; // NULL en tablas con llave compuesta
    std::string detalleCambio;   // texto ya redactado, vacío = sin detalle
    std::string fechaModificacion; // AAAA-MM-DD HH:MM:SS (hora de Colombia)
};