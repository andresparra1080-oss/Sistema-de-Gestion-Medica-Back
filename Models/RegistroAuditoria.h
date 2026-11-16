#pragma once

#include <optional>
#include <string>

// Una fila de logs_auditoria con el nombre del usuario ya resuelto (KAN-54).
struct RegistroAuditoria {
    long long idLog = 0;
    long long idUsuario = 0;
    std::string nombreUsuario;       // usuarios.nombre
    std::string accion;              // INSERT, UPDATE, DELETE...
    std::string tablaAfectada;       // pacientes, historias_clinicas, emergencias...
    std::optional<long long> idRegistro; // NULL en tablas con llave compuesta
    std::string detalleCambio;       // texto ya redactado; vacío = sin detalle
    std::string fechaModificacion;   // AAAA-MM-DD HH:MM:SS (hora de Colombia)
};