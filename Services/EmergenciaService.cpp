#include "Services/EmergenciaService.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

#include "Repositories/UsuarioRepository.h"

namespace {

const std::size_t MAXIMO_TEXTO = 2000;
const std::size_t MAXIMO_PACIENTES = 50;
const char* NIVEL_POR_DEFECTO = "ALTA";

std::string limpiarTexto(const std::string& valor) {
    const auto inicio = valor.find_first_not_of(" \t\r\n");
    if (inicio == std::string::npos) {
        return "";
    }

    const auto fin = valor.find_last_not_of(" \t\r\n");
    return valor.substr(inicio, fin - inicio + 1);
}

std::string aMayusculas(const std::string& valor) {
    std::string resultado = valor;
    for (char& caracter : resultado) {
        caracter = static_cast<char>(std::toupper(static_cast<unsigned char>(caracter)));
    }
    return resultado;
}

}  // namespace

ResultadoOperacion EmergenciaService::registrar(const NuevaEmergencia& datos,
                                                const std::string& correoUsuario,
                                                Emergencia& creada,
                                                std::string& mensaje) const {
    UsuarioRepository usuarios;
    EmergenciaRepository repository;

    const long long idUsuario = usuarios.obtenerIdPorCorreo(correoUsuario);
    if (idUsuario <= 0) {
        mensaje = "La cuenta que registra la emergencia ya no está activa";
        return ResultadoOperacion::Conflicto;
    }

    // El permiso se mira en la base (rol_permiso), no contra el nombre del rol:
    // así Capitán y Cirujano Naval entran, y si mañana se le da el permiso a
    // otro rol, no hay que tocar el código.
    if (!repository.usuarioPuedeGestionarEmergencias(idUsuario)) {
        mensaje = "Acceso denegado: su rol no puede registrar emergencias";
        return ResultadoOperacion::SinPermiso;
    }

    // Tipo: solo los que tienen protocolo.
    std::vector<std::string> tiposValidos;
    std::string errorTipos;
    if (!repository.listarTipos(tiposValidos, errorTipos)) {
        mensaje = errorTipos;
        return ResultadoOperacion::ErrorInterno;
    }

    const std::string tipo = aMayusculas(limpiarTexto(datos.tipo));
    if (std::find(tiposValidos.begin(), tiposValidos.end(), tipo) == tiposValidos.end()) {
        std::string lista;
        for (const std::string& valido : tiposValidos) {
            lista += (lista.empty() ? "" : ", ") + valido;
        }
        mensaje = "El tipo de emergencia es obligatorio y debe ser uno de: " + lista;
        return ResultadoOperacion::DatosInvalidos;
    }

    const std::string sintomas = limpiarTexto(datos.sintomas);
    if (sintomas.empty() || sintomas.size() > MAXIMO_TEXTO) {
        mensaje = "Los síntomas son obligatorios y admiten máximo 2000 caracteres";
        return ResultadoOperacion::DatosInvalidos;
    }

    const std::string lesiones = limpiarTexto(datos.lesiones);
    if (lesiones.size() > MAXIMO_TEXTO) {
        mensaje = "Las lesiones admiten máximo 2000 caracteres";
        return ResultadoOperacion::DatosInvalidos;
    }

    const std::string diagnostico = limpiarTexto(datos.diagnosticoInicial);
    if (diagnostico.empty() || diagnostico.size() > MAXIMO_TEXTO) {
        mensaje = "El diagnóstico inicial es obligatorio y admite máximo 2000 caracteres";
        return ResultadoOperacion::DatosInvalidos;
    }

    // La base exige un nivel de urgencia y la historia de usuario no lo pide:
    // si no llega, se asume ALTA (por seguridad, es mejor sobrestimar que
    // dejar una emergencia nueva como BAJA sin que nadie lo haya decidido).
    std::string nivel = aMayusculas(limpiarTexto(datos.nivelUrgencia));
    if (nivel.empty()) {
        nivel = NIVEL_POR_DEFECTO;
    }
    if (nivel != "BAJA" && nivel != "MEDIA" && nivel != "ALTA" && nivel != "CRITICA") {
        mensaje = "El nivel de urgencia debe ser BAJA, MEDIA, ALTA o CRITICA";
        return ResultadoOperacion::DatosInvalidos;
    }

    // Pacientes: al menos uno, sin repetir y todos existentes con historia.
    if (datos.pacientes.empty()) {
        mensaje = "Debe asociar al menos un tripulante a la emergencia";
        return ResultadoOperacion::DatosInvalidos;
    }

    if (datos.pacientes.size() > MAXIMO_PACIENTES) {
        mensaje = "No se pueden asociar más de 50 tripulantes a una emergencia";
        return ResultadoOperacion::DatosInvalidos;
    }

    NuevaEmergencia limpia;
    limpia.tipo = tipo;
    limpia.sintomas = sintomas;
    limpia.lesiones = lesiones;
    limpia.diagnosticoInicial = diagnostico;
    limpia.nivelUrgencia = nivel;

    std::vector<long long> vistos;
    for (const PacienteEmergencia& entrada : datos.pacientes) {
        if (entrada.idPaciente <= 0) {
            mensaje = "Cada tripulante debe indicarse con un identificador de paciente válido";
            return ResultadoOperacion::DatosInvalidos;
        }

        if (std::find(vistos.begin(), vistos.end(), entrada.idPaciente) != vistos.end()) {
            mensaje = "El paciente " + std::to_string(entrada.idPaciente) + " viene repetido en la lista";
            return ResultadoOperacion::DatosInvalidos;
        }
        vistos.push_back(entrada.idPaciente);

        PacienteEmergencia paciente;
        if (!repository.buscarPaciente(entrada.idPaciente, paciente)) {
            mensaje = "No existe el paciente " + std::to_string(entrada.idPaciente);
            return ResultadoOperacion::NoEncontrado;
        }

        // Sin historia no hay dónde dejar el evento: se avisa en vez de
        // registrar una emergencia que quede fuera del historial.
        if (!repository.tieneHistoria(entrada.idPaciente)) {
            mensaje = "El paciente " + paciente.nombreCompleto + " no tiene historia clínica registrada";
            return ResultadoOperacion::Conflicto;
        }

        limpia.pacientes.push_back(paciente);
    }

    std::string error;
    if (!repository.crear(limpia, idUsuario, creada, error)) {
        mensaje = error.empty() ? "No se pudo registrar la emergencia" : error;
        return ResultadoOperacion::ErrorInterno;
    }

    mensaje = "Emergencia registrada";
    return ResultadoOperacion::Exito;
}
// ---------------------------------------------------------------------
// KAN-51: acciones, cambio de estado y finalización
// ---------------------------------------------------------------------
namespace {

// Identifica a quien firma la petición y comprueba su permiso en la base.
ResultadoOperacion autorizarGestor(const std::string& correoUsuario,
                                   const EmergenciaRepository& repository,
                                   long long& idUsuario,
                                   std::string& mensaje) {
    UsuarioRepository usuarios;

    idUsuario = usuarios.obtenerIdPorCorreo(correoUsuario);
    if (idUsuario <= 0) {
        mensaje = "La cuenta que realiza la operación ya no está activa";
        return ResultadoOperacion::Conflicto;
    }

    if (!repository.usuarioPuedeGestionarEmergencias(idUsuario)) {
        mensaje = "Acceso denegado: su rol no puede gestionar emergencias";
        return ResultadoOperacion::SinPermiso;
    }

    return ResultadoOperacion::Exito;
}

bool identificadorValido(long long idEmergencia, std::string& mensaje) {
    if (idEmergencia <= 0) {
        mensaje = "El identificador de la emergencia no es válido";
        return false;
    }
    return true;
}

}  // namespace

ResultadoOperacion EmergenciaService::registrarAccion(long long idEmergencia,
                                                      const std::string& descripcion,
                                                      const std::string& correoUsuario,
                                                      AccionEmergencia& creada,
                                                      std::string& mensaje) const {
    EmergenciaRepository repository;

    long long idUsuario = 0;
    const ResultadoOperacion autorizacion = autorizarGestor(correoUsuario, repository, idUsuario, mensaje);
    if (autorizacion != ResultadoOperacion::Exito) {
        return autorizacion;
    }

    if (!identificadorValido(idEmergencia, mensaje)) {
        return ResultadoOperacion::DatosInvalidos;
    }

    const std::string texto = limpiarTexto(descripcion);
    if (texto.empty() || texto.size() > MAXIMO_TEXTO) {
        mensaje = "La descripción de la acción es obligatoria y admite máximo 2000 caracteres";
        return ResultadoOperacion::DatosInvalidos;
    }

    std::string error;
    const ResultadoOperacion resultado = repository.registrarAccion(idEmergencia, idUsuario, texto, creada, error);
    if (resultado != ResultadoOperacion::Exito) {
        mensaje = error.empty() ? "No se pudo registrar la acción" : error;
        return resultado;
    }

    mensaje = "Acción registrada";
    return ResultadoOperacion::Exito;
}

ResultadoOperacion EmergenciaService::listarAcciones(long long idEmergencia,
                                                     const std::string& correoUsuario,
                                                     std::vector<AccionEmergencia>& acciones,
                                                     std::string& mensaje) const {
    EmergenciaRepository repository;

    long long idUsuario = 0;
    const ResultadoOperacion autorizacion = autorizarGestor(correoUsuario, repository, idUsuario, mensaje);
    if (autorizacion != ResultadoOperacion::Exito) {
        return autorizacion;
    }

    if (!identificadorValido(idEmergencia, mensaje)) {
        return ResultadoOperacion::DatosInvalidos;
    }

    std::string error;
    const ResultadoOperacion resultado = repository.listarAcciones(idEmergencia, acciones, error);
    if (resultado != ResultadoOperacion::Exito) {
        mensaje = error.empty() ? "No se pudo consultar el historial de acciones" : error;
        return resultado;
    }

    mensaje = "Historial de acciones";
    return ResultadoOperacion::Exito;
}

ResultadoOperacion EmergenciaService::marcarEnAtencion(long long idEmergencia,
                                                       const std::string& correoUsuario,
                                                       Emergencia& actualizada,
                                                       std::string& mensaje) const {
    EmergenciaRepository repository;

    long long idUsuario = 0;
    const ResultadoOperacion autorizacion = autorizarGestor(correoUsuario, repository, idUsuario, mensaje);
    if (autorizacion != ResultadoOperacion::Exito) {
        return autorizacion;
    }

    if (!identificadorValido(idEmergencia, mensaje)) {
        return ResultadoOperacion::DatosInvalidos;
    }

    std::string error;
    const ResultadoOperacion resultado = repository.marcarEnAtencion(idEmergencia, idUsuario, actualizada, error);
    if (resultado != ResultadoOperacion::Exito) {
        mensaje = error.empty() ? "No se pudo cambiar el estado de la emergencia" : error;
        return resultado;
    }

    mensaje = "La emergencia pasó a EN_ATENCION";
    return ResultadoOperacion::Exito;
}

ResultadoOperacion EmergenciaService::finalizar(long long idEmergencia,
                                                const std::string& correoUsuario,
                                                Emergencia& actualizada,
                                                std::string& mensaje) const {
    EmergenciaRepository repository;

    long long idUsuario = 0;
    const ResultadoOperacion autorizacion = autorizarGestor(correoUsuario, repository, idUsuario, mensaje);
    if (autorizacion != ResultadoOperacion::Exito) {
        return autorizacion;
    }

    if (!identificadorValido(idEmergencia, mensaje)) {
        return ResultadoOperacion::DatosInvalidos;
    }

    std::string error;
    const ResultadoOperacion resultado = repository.finalizar(idEmergencia, idUsuario, actualizada, error);
    if (resultado != ResultadoOperacion::Exito) {
        mensaje = error.empty() ? "No se pudo finalizar la emergencia" : error;
        return resultado;
    }

    mensaje = "Emergencia finalizada";
    return ResultadoOperacion::Exito;
}