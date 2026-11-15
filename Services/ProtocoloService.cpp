#include "Services/ProtocoloService.h"

#include <cctype>
#include <string>
#include <vector>

#include "Repositories/UsuarioRepository.h"

namespace {

std::string normalizarTipo(const std::string& valor) {
    const auto inicio = valor.find_first_not_of(" \t\r\n");
    if (inicio == std::string::npos) {
        return "";
    }
    const auto fin = valor.find_last_not_of(" \t\r\n");

    std::string resultado = valor.substr(inicio, fin - inicio + 1);
    for (char& caracter : resultado) {
        caracter = static_cast<char>(std::toupper(static_cast<unsigned char>(caracter)));
    }
    return resultado;
}

ResultadoOperacion autorizarLector(const std::string& correoUsuario, std::string& mensaje) {
    UsuarioRepository usuarios;

    if (usuarios.obtenerIdPorCorreo(correoUsuario) <= 0) {
        mensaje = "La cuenta que realiza la consulta ya no está activa";
        return ResultadoOperacion::Conflicto;
    }
    return ResultadoOperacion::Exito;
}

}  // namespace

ResultadoOperacion ProtocoloService::listar(const std::string& correoUsuario,
                                            std::vector<Protocolo>& protocolos,
                                            std::string& mensaje) const {
    const ResultadoOperacion autorizacion = autorizarLector(correoUsuario, mensaje);
    if (autorizacion != ResultadoOperacion::Exito) {
        return autorizacion;
    }

    ProtocoloRepository repository;
    return repository.listar(protocolos, mensaje);
}

ResultadoOperacion ProtocoloService::obtenerPorTipo(const std::string& tipo,
                                                    const std::string& correoUsuario,
                                                    Protocolo& protocolo,
                                                    std::string& mensaje) const {
    const ResultadoOperacion autorizacion = autorizarLector(correoUsuario, mensaje);
    if (autorizacion != ResultadoOperacion::Exito) {
        return autorizacion;
    }

    const std::string tipoNormalizado = normalizarTipo(tipo);
    if (tipoNormalizado.empty()) {
        mensaje = "El tipo de emergencia es obligatorio";
        return ResultadoOperacion::DatosInvalidos;
    }

    ProtocoloRepository repository;
    return repository.obtenerPorTipo(tipoNormalizado, protocolo, mensaje);
}