#pragma once

#include <optional>
#include <string>

// Fila de la tabla alertas, con el nombre del medicamento y el número de lote
// ya resueltos para mostrarlos en pantalla.
// (AlertaEmergencia, en Emergencia.h, es solo la alerta que nace al registrar
// una emergencia.)
struct AlertaSistema {
    long long idAlerta = 0;
    std::optional<long long> idMedicamento;
    std::optional<long long> idLote;
    std::optional<long long> idTratamiento;
    std::optional<long long> idEmergencia;
    std::string tipoAlerta;
    std::string mensaje;
    std::string prioridad;
    bool notificada = false;  // "leída"
    bool resuelta = false;
    std::string fechaGeneracion;
    std::optional<std::string> fechaResolucion;
    std::string nombreMedicamento;  // vacío si la alerta no es de un medicamento o lote
    std::string numLote;            // vacío si la alerta no es de un lote
};