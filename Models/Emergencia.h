#pragma once

#include <string>
#include <vector>
#include <optional>

// Tripulante involucrado en una emergencia (fila de emergencia_paciente con
// el nombre ya resuelto).
struct PacienteEmergencia {
    long long idPaciente = 0;
    std::string nombreCompleto;
};

// Lo que llega en POST /api/emergencias, ya limpio y validado por el servicio.
// No trae fecha de inicio, estado ni usuario: los pone el backend.
struct NuevaEmergencia {
    std::string tipo;
    std::string sintomas;
    std::string lesiones;  // vacío = sin lesiones (NULL en la base)
    std::string diagnosticoInicial;
    std::string nivelUrgencia;
    std::vector<PacienteEmergencia> pacientes;
};

// Alerta de tipo EMERGENCIA que genera el registro.
struct AlertaEmergencia {
    long long idAlerta = 0;
    std::string tipoAlerta;
    std::string mensaje;
    std::string prioridad;
    bool notificada = false;
    std::string fechaGeneracion;
};

// Emergencia registrada, tal como se devuelve al cliente.
struct Emergencia {
    long long idEmergencia = 0;
    std::string tipo;
    std::string sintomas;
    std::string lesiones;
    std::string diagnosticoInicial;
    std::string nivelUrgencia;
    std::string estado;
    std::string fechaInicio;
    std::optional<std::string> fechaFin; // sin valor mientras no esté FINALIZADA
    std::vector<PacienteEmergencia> pacientes;
    AlertaEmergencia alerta;
};
