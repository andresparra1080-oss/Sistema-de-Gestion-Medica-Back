#include "Services/LoteService.h"

#include <cctype>

#include "Repositories/LoteRepository.h"
#include "Repositories/UsuarioRepository.h"

namespace {

std::string limpiarTexto(const std::string& texto) {
    const auto inicio = texto.find_first_not_of(" \t\r\n");

    if (inicio == std::string::npos) {
        return "";
    }

    const auto fin = texto.find_last_not_of(" \t\r\n");
    return texto.substr(inicio, fin - inicio + 1);
}

bool fechaValida(const std::string& fecha) {
    if (fecha.size() != 10 ||
        fecha[4] != '-' ||
        fecha[7] != '-') {
        return false;
    }

    for (std::size_t i = 0; i < fecha.size(); ++i) {
        if (i == 4 || i == 7) {
            continue;
        }

        if (!std::isdigit(
                static_cast<unsigned char>(fecha[i]))) {
            return false;
        }
    }

    const int anio = std::stoi(fecha.substr(0, 4));
    const int mes = std::stoi(fecha.substr(5, 2));
    const int dia = std::stoi(fecha.substr(8, 2));

    if (anio < 1 || mes < 1 || mes > 12 || dia < 1) {
        return false;
    }

    const bool bisiesto =
        anio % 400 == 0 ||
        (anio % 4 == 0 && anio % 100 != 0);

    const int diasPorMes[] = {
        31, bisiesto ? 29 : 28, 31, 30, 31, 30,
        31, 31, 30, 31, 30, 31
    };

    return dia <= diasPorMes[mes - 1];
}

bool textoValido(const std::string& texto, std::size_t limite) {
    if (texto.empty() || texto.size() > limite) {
        return false;
    }

    for (unsigned char caracter : texto) {
        if (caracter < 32 || caracter == 127) {
            return false;
        }
    }

    return true;
}

bool usuarioActivo(const std::string& correo) {
    UsuarioRepository usuarios;
    return usuarios.obtenerIdPorCorreo(correo) > 0;
}

}  // namespace

ResultadoOperacion LoteService::listar(
    const std::string& correoUsuario,
    std::vector<Lote>& lotes,
    std::string& mensaje) const {

    if (!usuarioActivo(correoUsuario)) {
        mensaje = "La cuenta ya no esta activa";
        return ResultadoOperacion::Conflicto;
    }

    LoteRepository repository;

    if (!repository.listar(lotes, mensaje)) {
        return ResultadoOperacion::ErrorInterno;
    }

    mensaje = "Lotes consultados";
    return ResultadoOperacion::Exito;
}

ResultadoOperacion LoteService::crear(
    const Lote& datos,
    const std::string& correoUsuario,
    Lote& creado,
    std::string& mensaje) const {

    if (!usuarioActivo(correoUsuario)) {
        mensaje = "La cuenta ya no esta activa";
        return ResultadoOperacion::Conflicto;
    }

    if (datos.getIdMedicamento() <= 0) {
        mensaje = "El identificador del medicamento debe ser positivo";
        return ResultadoOperacion::DatosInvalidos;
    }

    const std::string numero = limpiarTexto(datos.getNumLote());
    const std::string ubicacion = limpiarTexto(datos.getUbicacion());
    const std::string fecha = limpiarTexto(
        datos.getFechaVencimiento());

    if (!textoValido(numero, 50)) {
        mensaje =
            "El numero de lote es obligatorio, admite hasta 50 bytes "
            "y no permite caracteres de control";
        return ResultadoOperacion::DatosInvalidos;
    }

    if (!textoValido(ubicacion, 100)) {
        mensaje =
            "La ubicacion es obligatoria, admite hasta 100 bytes "
            "y no permite caracteres de control";
        return ResultadoOperacion::DatosInvalidos;
    }

    if (!fechaValida(fecha)) {
        mensaje =
            "La fecha debe ser una fecha real con formato AAAA-MM-DD";
        return ResultadoOperacion::DatosInvalidos;
    }

    if (datos.getCantidad() != 0) {
        mensaje =
            "El lote se crea en cero; registre un movimiento ENTRADA "
            "para agregar existencias";
        return ResultadoOperacion::DatosInvalidos;
    }

    Lote normalizado;
    normalizado.setIdMedicamento(datos.getIdMedicamento());
    normalizado.setNumLote(numero);
    normalizado.setUbicacion(ubicacion);
    normalizado.setFechaVencimiento(fecha);
    normalizado.setCantidad(0);

    LoteRepository repository;
    long long idGenerado = 0;

    if (!repository.crear(normalizado, idGenerado, mensaje)) {
        if (mensaje ==
            "Ya existe ese numero de lote para el medicamento") {
            return ResultadoOperacion::Conflicto;
        }

        if (mensaje == "No existe el medicamento indicado") {
            return ResultadoOperacion::NoEncontrado;
        }

        return ResultadoOperacion::ErrorInterno;
    }

    normalizado.setIdLote(idGenerado);
    creado = normalizado;

    mensaje = "Lote creado con cantidad cero";
    return ResultadoOperacion::Exito;
}