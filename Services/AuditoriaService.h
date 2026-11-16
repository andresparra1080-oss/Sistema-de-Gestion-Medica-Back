#pragma once

#include <string>
#include <vector>

#include "Cross/ResultadoOperacion.h"
#include "Models/RegistroAuditoria.h"
#include "Repositories/AuditoriaRepository.h"

class AuditoriaService {
public:
    // Historial de auditoría de un paciente (KAN-54, RF-05). Del más reciente al más antiguo.
    ResultadoOperacion listarPorPaciente(long long idPaciente,
                                         std::vector<RegistroAuditoria>& registros,
                                         std::string& mensaje) const;
};