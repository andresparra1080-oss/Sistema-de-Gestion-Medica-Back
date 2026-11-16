#include "Services/AuditoriaService.h"

ResultadoOperacion AuditoriaService::listarPorPaciente(long long idPaciente,
                                                       std::vector<RegistroAuditoria>& registros,
                                                       std::string& mensaje) const {
    AuditoriaRepository repositorio;
    std::string error;
    const ResultadoOperacion resultado = repositorio.listarPorPaciente(idPaciente, registros, error);

    if (resultado == ResultadoOperacion::Exito) {
        mensaje = "Historial de auditoría del paciente";
        return resultado;
    }

    mensaje = error.empty() ? "No se pudo consultar el historial de auditoría" : error;
    return resultado;
}