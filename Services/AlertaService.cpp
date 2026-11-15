#include "Services/AlertaService.h"

#include <string>
#include <vector>

#include "Repositories/UsuarioRepository.h"

namespace {

// Devuelve el id del usuario del token, o un resultado de error si su cuenta
// ya no está activa.
ResultadoOperacion identificarUsuario(const std::string& correoUsuario,
                                      long long& idUsuario,
                                      std::string& mensaje) {
    UsuarioRepository usuarios;

    idUsuario = usuarios.obtenerIdPorCorreo(correoUsuario);
    if (idUsuario <= 0) {
        mensaje = "La cuenta que realiza la operación ya no está activa";
        return ResultadoOperacion::Conflicto;
    }
    return ResultadoOperacion::Exito;
}

}  // namespace

ResultadoOperacion AlertaService::listar(bool soloSinResolver,
                                         const std::string& correoUsuario,
                                         std::vector<AlertaSistema>& alertas,
                                         std::string& mensaje) const {
    long long idUsuario = 0;
    const ResultadoOperacion identificado = identificarUsuario(correoUsuario, idUsuario, mensaje);
    if (identificado != ResultadoOperacion::Exito) {
        return identificado;
    }

    AlertaRepository repository;
    return repository.listar(soloSinResolver, alertas, mensaje);
}

ResultadoOperacion AlertaService::marcarLeida(long long idAlerta,
                                              const std::string& correoUsuario,
                                              std::string& mensaje) const {
    long long idUsuario = 0;
    const ResultadoOperacion identificado = identificarUsuario(correoUsuario, idUsuario, mensaje);
    if (identificado != ResultadoOperacion::Exito) {
        return identificado;
    }

    if (idAlerta <= 0) {
        mensaje = "El identificador de la alerta no es válido";
        return ResultadoOperacion::DatosInvalidos;
    }

    AlertaRepository repository;
    const ResultadoOperacion resultado = repository.marcarLeida(idAlerta, idUsuario, mensaje);
    if (resultado == ResultadoOperacion::Exito) {
        mensaje = "Alerta marcada como leída";
    }
    return resultado;
}

ResultadoOperacion AlertaService::marcarTodasLeidas(const std::string& correoUsuario,
                                                    int& cantidad,
                                                    std::string& mensaje) const {
    cantidad = 0;

    long long idUsuario = 0;
    const ResultadoOperacion identificado = identificarUsuario(correoUsuario, idUsuario, mensaje);
    if (identificado != ResultadoOperacion::Exito) {
        return identificado;
    }

    AlertaRepository repository;
    const ResultadoOperacion resultado = repository.marcarTodasLeidas(idUsuario, cantidad, mensaje);
    if (resultado == ResultadoOperacion::Exito) {
        mensaje = cantidad == 0 ? "No había alertas sin leer"
                                : std::to_string(cantidad) + " alertas marcadas como leídas";
    }
    return resultado;
}