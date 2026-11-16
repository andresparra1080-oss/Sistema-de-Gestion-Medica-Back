#pragma once

#include <string>
#include <vector>

#include "Cross/ResultadoOperacion.h"
#include "Models/RegistroAuditoria.h"

// Consulta de solo lectura sobre logs_auditoria (KAN-54).
class AuditoriaRepository {
public:
    // Todo lo que quedó registrado sobre un paciente: él, su historia clínica, sus
    // eventos, controles y tratamientos, y las emergencias en las que participó
    // (con sus acciones). Del más reciente al más antiguo, máximo 200.
    // Devuelve Exito, NoEncontrado (el paciente no existe) o ErrorInterno.
    ResultadoOperacion listarPorPaciente(long long idPaciente,
                                         std::vector<RegistroAuditoria>& registros,
                                         std::string& error) const;
};