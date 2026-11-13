#pragma once
#include <string>

// Acción de atención registrada en una emergencia (tabla acciones_emergencia).
struct AccionEmergencia {
    long long idAccion = 0;
    long long idEmergencia = 0;
    long long idUsuarioEjecutor = 0;
    std::string nombreEjecutor; // usuarios.nombre, resuelto al leer
    std::string descripcion;
    std::string fechaHora;      // la pone la base: AAAA-MM-DD HH:MM:SS (hora de Colombia)
};
