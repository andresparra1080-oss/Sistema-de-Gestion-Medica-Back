#pragma once

#include <optional>
#include <string>

struct Alerta {
    long long idAlerta = 0;
    std::optional<long long> idMedicamento;
    std::optional<long long> idLote;
    std::optional<long long> idTratamiento;
    std::optional<long long> idEmergencia;
    std::string tipoAlerta;
    std::string mensaje;
    std::string prioridad;
    bool notificada = false;
    bool resuelta = false;
    std::string fechaGeneracion;
    std::optional<std::string> fechaResolucion;
};