#pragma once

#include <string>
#include <vector>

// Un paso de un protocolo (fila de pasos_protocolo).
struct PasoProtocolo {
    long long idPaso = 0;
    int numero = 0;
    std::string descripcion;
};

// Protocolo de atención de un tipo de emergencia (fila de protocolos)
// con sus pasos ya ordenados por número.
struct Protocolo {
    long long idProtocolo = 0;
    std::string tipoEmergencia;
    std::string nombre;
    std::vector<PasoProtocolo> pasos;
};