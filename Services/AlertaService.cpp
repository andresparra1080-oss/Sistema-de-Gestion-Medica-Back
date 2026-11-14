#include "Services/AlertaService.h"

#include <cctype>
#include "Repositories/AlertaRepository.h"
#include "Repositories/UsuarioRepository.h"

namespace {

std::string aMayusculas(const std::string& valor) {
    std::string resultado = valor;
    for (char& c : resultado) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return resultado;
}

ResultadoOperacion validarUsuarioActivo(const std::string& correoUsuario,
                                        long long& idUsuario,
                                        std::string& mensaje) {
    UsuarioRepository usuarios;
    idUsuario = usuarios.obtenerIdPorCorreo(correoUsuario);
    if (idUsuario <= 0) {
        mensaje = "La cuenta que realiza la operación no está activa";
        return ResultadoOperacion::Conflicto;
    }
    return ResultadoOperacion::Exito;
}

}  // namespace

ResultadoOperacion AlertaService::listar(const std::optional<int>& resueltaFiltro,
                                         const std::string& tipoFiltro,
                                         const std::string& correoUsuario,
                                         std::vector<Alerta>& alertas,
                                         std::string& mensaje) const {
    long long idUsuario = 0;
    const ResultadoOperacion auth = validarUsuarioActivo(correoUsuario, idUsuario, mensaje);
    if (auth != ResultadoOperacion::Exito) {
        return auth;
    }

    if (resueltaFiltro.has_value() && (*resueltaFiltro != 0 && *resueltaFiltro != 1)) {
        mensaje = "El parámetro resuelta debe ser 0 o 1";
        return ResultadoOperacion::DatosInvalidos;
    }

    const std::string tipoLimpio = aMayusculas(tipoFiltro);
    if (!tipoLimpio.empty() &&
        tipoLimpio != "STOCK_BAJO" &&
        tipoLimpio != "VENCIMIENTO_PROXIMO" &&
        tipoLimpio != "DOSIS_PENDIENTE" &&
        tipoLimpio != "EMERGENCIA") {
        mensaje = "El tipo de alerta debe ser STOCK_BAJO, VENCIMIENTO_PROXIMO, DOSIS_PENDIENTE o EMERGENCIA";
        return ResultadoOperacion::DatosInvalidos;
    }

    AlertaRepository repository;
    std::string error;
    if (!repository.listar(resueltaFiltro, tipoLimpio, alertas, error)) {
        mensaje = error.empty() ? "No se pudieron consultar las alertas" : error;
        return ResultadoOperacion::ErrorInterno;
    }

    mensaje = "Alertas consultadas";
    return ResultadoOperacion::Exito;
}

ResultadoOperacion AlertaService::marcarLeida(long long idAlerta,
                                              const std::string& correoUsuario,
                                              Alerta& actualizada,
                                              std::string& mensaje) const {
    long long idUsuario = 0;
    const ResultadoOperacion auth = validarUsuarioActivo(correoUsuario, idUsuario, mensaje);
    if (auth != ResultadoOperacion::Exito) {
        return auth;
    }

    if (idAlerta <= 0) {
        mensaje = "El identificador de la alerta no es válido";
        return ResultadoOperacion::DatosInvalidos;
    }

    AlertaRepository repository;
    std::string error;
    const ResultadoOperacion resultado = repository.marcarLeida(idAlerta, idUsuario, actualizada, error);
    if (resultado != ResultadoOperacion::Exito) {
        mensaje = error.empty() ? "No se pudo actualizar la alerta" : error;
        return resultado;
    }

    mensaje = "Alerta marcada como leída";
    return ResultadoOperacion::Exito;
}

ResultadoOperacion AlertaService::marcarTodasLeidas(const std::string& correoUsuario,
                                                    int& cantidadActualizada,
                                                    std::string& mensaje) const {
    long long idUsuario = 0;
    const ResultadoOperacion auth = validarUsuarioActivo(correoUsuario, idUsuario, mensaje);
    if (auth != ResultadoOperacion::Exito) {
        return auth;
    }

    AlertaRepository repository;
    std::string error;
    const ResultadoOperacion resultado = repository.marcarTodasLeidas(idUsuario, cantidadActualizada, error);
    if (resultado != ResultadoOperacion::Exito) {
        mensaje = error.empty() ? "No se pudieron actualizar las alertas" : error;
        return resultado;
    }

    mensaje = cantidadActualizada > 0
                  ? "Se marcaron " + std::to_string(cantidadActualizada) + " alertas como leídas"
                  : "No había alertas pendientes por leer";
    return ResultadoOperacion::Exito;
}