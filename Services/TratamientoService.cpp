#include "Services/TratamientoService.h"

#include <cctype>
#include <string>
#include <vector>

#include "Repositories/InventarioRepository.h"
#include "Repositories/PacienteRepository.h"
#include "Repositories/TratamientoRepository.h"
#include "Repositories/UsuarioRepository.h"

namespace {

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
        caracter = static_cast<char>(
            std::toupper(static_cast<unsigned char>(caracter))
        );
    }

    return resultado;
}

// Comprueba que la fecha tenga el formato YYYY-MM-DD.
// La base de datos también valida que sea una fecha real.
bool esFechaISO(const std::string& fecha) {
    if (fecha.size() != 10 || fecha[4] != '-' || fecha[7] != '-') {
        return false;
    }

    for (std::size_t i = 0; i < fecha.size(); ++i) {
        if (i == 4 || i == 7) {
            continue;
        }

        if (!std::isdigit(static_cast<unsigned char>(fecha[i]))) {
            return false;
        }
    }

    return true;
}

bool validarDatosTratamiento(const Tratamiento& datos,
                             const std::string& fechaInicio,
                             const std::string& fechaFin,
                             std::string& mensaje) {
    const std::string dosis = limpiarTexto(datos.getDosis());
    const std::string frecuencia = limpiarTexto(datos.getFrecuencia());

    if (dosis.empty() || dosis.size() > 100) {
        mensaje = "La dosis es obligatoria y admite máximo 100 caracteres";
        return false;
    }

    if (frecuencia.empty() || frecuencia.size() > 100) {
        mensaje = "La frecuencia es obligatoria y admite máximo 100 caracteres";
        return false;
    }

    if (datos.tieneIntervaloHoras() &&
        (datos.getIntervaloHoras() < 1 || datos.getIntervaloHoras() > 168)) {
        mensaje = "El intervalo debe estar entre 1 y 168 horas";
        return false;
    }

    if (!esFechaISO(fechaInicio)) {
        mensaje = "La fecha de inicio debe tener el formato YYYY-MM-DD";
        return false;
    }

    if (!fechaFin.empty()) {
        if (!esFechaISO(fechaFin)) {
            mensaje = "La fecha de finalización debe tener el formato YYYY-MM-DD";
            return false;
        }

        if (fechaFin < fechaInicio) {
            mensaje = "La fecha de finalización no puede ser anterior a la fecha de inicio";
            return false;
        }
    }

    return true;
}

}  // namespace

ResultadoOperacion TratamientoService::listar(
    std::vector<Tratamiento>& tratamientos,
    std::string& mensaje) const {

    TratamientoRepository repository;
    std::string error;

    if (!repository.listar(tratamientos, error)) {
        mensaje = error.empty()
            ? "No se pudieron consultar los tratamientos"
            : error;

        return ResultadoOperacion::ErrorInterno;
    }

    mensaje = "Tratamientos consultados";
    return ResultadoOperacion::Exito;
}

ResultadoOperacion TratamientoService::listarPorPaciente(
    long long idPaciente,
    std::vector<Tratamiento>& tratamientos,
    std::string& mensaje) const {

    if (idPaciente <= 0) {
        mensaje = "El identificador del paciente no es válido";
        return ResultadoOperacion::DatosInvalidos;
    }

    PacienteRepository pacientes;

    if (!pacientes.existeId(idPaciente)) {
        mensaje = "No existe el paciente indicado";
        return ResultadoOperacion::NoEncontrado;
    }

    TratamientoRepository repository;
    std::string error;

    if (!repository.listarPorPaciente(idPaciente, tratamientos, error)) {
        mensaje = error.empty()
            ? "No se pudieron consultar los tratamientos del paciente"
            : error;

        return ResultadoOperacion::ErrorInterno;
    }

    mensaje = "Tratamientos del paciente consultados";
    return ResultadoOperacion::Exito;
}

ResultadoOperacion TratamientoService::registrar(
    const Tratamiento& datos,
    const std::string& correoUsuario,
    Tratamiento& registrado,
    std::string& mensaje) const {

    if (datos.getIdPaciente() <= 0 || datos.getIdMedicamento() <= 0) {
        mensaje = "Debe seleccionar un paciente y un medicamento válidos";
        return ResultadoOperacion::DatosInvalidos;
    }

    const std::string fechaInicio = limpiarTexto(datos.getFechaInicio());
    const std::string fechaFin = limpiarTexto(datos.getFechaFin());

    if (!validarDatosTratamiento(datos, fechaInicio, fechaFin, mensaje)) {
        return ResultadoOperacion::DatosInvalidos;
    }

    PacienteRepository pacientes;

    if (!pacientes.existeId(datos.getIdPaciente())) {
        mensaje = "No existe el paciente indicado";
        return ResultadoOperacion::NoEncontrado;
    }

    // Se comprueba que el medicamento exista y que no sea un insumo.
    InventarioRepository inventario;
    std::vector<Medicamento> medicamentos;
    std::string errorInventario;

    if (!inventario.listar(medicamentos, errorInventario)) {
        mensaje = errorInventario.empty()
            ? "No se pudo consultar el inventario"
            : errorInventario;

        return ResultadoOperacion::ErrorInterno;
    }

    bool medicamentoValido = false;

    for (const Medicamento& medicamento : medicamentos) {
        if (medicamento.getIdMedicamento() == datos.getIdMedicamento() &&
            medicamento.getTipoInsumo() == "MEDICAMENTO") {
            medicamentoValido = true;
            break;
        }
    }

    if (!medicamentoValido) {
        mensaje = "El medicamento indicado no existe o corresponde a un insumo";
        return ResultadoOperacion::NoEncontrado;
    }

    // El usuario que registra se obtiene de la sesión, no del cuerpo
    // de la petición.
    UsuarioRepository usuarios;
    const long long idUsuario = usuarios.obtenerIdPorCorreo(correoUsuario);

    if (idUsuario <= 0) {
        mensaje = "La cuenta que registra el tratamiento ya no está activa";
        return ResultadoOperacion::Conflicto;
    }

    Tratamiento nuevo;
    nuevo.setIdPaciente(datos.getIdPaciente());
    nuevo.setIdMedicamento(datos.getIdMedicamento());
    nuevo.setIdUsuarioRegistra(idUsuario);
    nuevo.setDosis(limpiarTexto(datos.getDosis()));
    nuevo.setFrecuencia(limpiarTexto(datos.getFrecuencia()));
    nuevo.setFechaInicio(fechaInicio);
    nuevo.setFechaFin(fechaFin);
    nuevo.setEstadoTratamiento("ACTIVO");

    if (datos.tieneIntervaloHoras()) {
        nuevo.setIntervaloHoras(datos.getIntervaloHoras());
    } else {
        nuevo.quitarIntervaloHoras();
    }

    long long idGenerado = 0;
    std::string error;
    TratamientoRepository repository;

    if (!repository.crear(nuevo, idGenerado, error)) {
        if (error.find("alerg") != std::string::npos ||
            error.find("Alerg") != std::string::npos) {
            mensaje = error;
            return ResultadoOperacion::Conflicto;
        }

        mensaje = error.empty()
            ? "No se pudo registrar el tratamiento"
            : error;

        return ResultadoOperacion::ErrorInterno;
    }

    if (!repository.buscarPorId(idGenerado, registrado, error)) {
        mensaje = "El tratamiento se registró, pero no se pudo recuperar la información";
        return ResultadoOperacion::ErrorInterno;
    }

    mensaje = "Tratamiento registrado correctamente";
    return ResultadoOperacion::Exito;
}

ResultadoOperacion TratamientoService::actualizar(
    const Tratamiento& datos,
    std::string& mensaje) const {

    if (datos.getIdTratamiento() <= 0) {
        mensaje = "El identificador del tratamiento no es válido";
        return ResultadoOperacion::DatosInvalidos;
    }

    TratamientoRepository repository;
    Tratamiento actual;
    std::string error;

    if (!repository.buscarPorId(datos.getIdTratamiento(), actual, error)) {
        mensaje = "No existe el tratamiento indicado";
        return ResultadoOperacion::NoEncontrado;
    }

    if (actual.getEstadoTratamiento() != "ACTIVO") {
        mensaje = "Solo se pueden modificar tratamientos activos";
        return ResultadoOperacion::Conflicto;
    }

    const std::string fechaFin = limpiarTexto(datos.getFechaFin());

    if (!validarDatosTratamiento(
            datos, actual.getFechaInicio(), fechaFin, mensaje)) {
        return ResultadoOperacion::DatosInvalidos;
    }

    Tratamiento actualizado;
    actualizado.setIdTratamiento(datos.getIdTratamiento());
    actualizado.setDosis(limpiarTexto(datos.getDosis()));
    actualizado.setFrecuencia(limpiarTexto(datos.getFrecuencia()));
    actualizado.setFechaFin(fechaFin);

    if (datos.tieneIntervaloHoras()) {
        actualizado.setIntervaloHoras(datos.getIntervaloHoras());
    } else {
        actualizado.quitarIntervaloHoras();
    }

    if (!repository.actualizar(actualizado, error)) {
        mensaje = error.empty()
            ? "No se pudo actualizar el tratamiento"
            : error;

        return ResultadoOperacion::ErrorInterno;
    }

    mensaje = "Tratamiento actualizado correctamente";
    return ResultadoOperacion::Exito;
}

ResultadoOperacion TratamientoService::cambiarEstado(
    long long idTratamiento,
    const std::string& estado,
    const std::string& fechaFinalizacion,
    std::string& mensaje) const {

    if (idTratamiento <= 0) {
        mensaje = "El identificador del tratamiento no es válido";
        return ResultadoOperacion::DatosInvalidos;
    }

    const std::string nuevoEstado =
        aMayusculas(limpiarTexto(estado));

    if (nuevoEstado != "ACTIVO" &&
        nuevoEstado != "FINALIZADO" &&
        nuevoEstado != "SUSPENDIDO") {
        mensaje = "El estado debe ser ACTIVO, FINALIZADO o SUSPENDIDO";
        return ResultadoOperacion::DatosInvalidos;
    }

    TratamientoRepository repository;
    Tratamiento actual;
    std::string error;

    if (!repository.buscarPorId(idTratamiento, actual, error)) {
        mensaje = "No existe el tratamiento indicado";
        return ResultadoOperacion::NoEncontrado;
    }

    const std::string estadoActual = actual.getEstadoTratamiento();

    if (estadoActual == "FINALIZADO") {
        mensaje = "Un tratamiento finalizado no puede cambiar de estado";
        return ResultadoOperacion::Conflicto;
    }

    if (nuevoEstado == "ACTIVO" && estadoActual != "SUSPENDIDO") {
        mensaje = "Solo se puede reactivar un tratamiento suspendido";
        return ResultadoOperacion::Conflicto;
    }

    if (nuevoEstado == "SUSPENDIDO" && estadoActual != "ACTIVO") {
        mensaje = "Solo se puede suspender un tratamiento activo";
        return ResultadoOperacion::Conflicto;
    }

    std::string fechaEfectiva;

    if (nuevoEstado == "FINALIZADO") {
        fechaEfectiva = limpiarTexto(fechaFinalizacion);

        if (!esFechaISO(fechaEfectiva)) {
            mensaje = "Para finalizar el tratamiento debe indicar una fecha válida YYYY-MM-DD";
            return ResultadoOperacion::DatosInvalidos;
        }

        if (fechaEfectiva < actual.getFechaInicio()) {
            mensaje = "La fecha de finalización no puede ser anterior al inicio";
            return ResultadoOperacion::DatosInvalidos;
        }
    }

    if (nuevoEstado == "SUSPENDIDO") {
        fechaEfectiva = limpiarTexto(fechaFinalizacion);

        if (!fechaEfectiva.empty() &&
            (!esFechaISO(fechaEfectiva) ||
             fechaEfectiva < actual.getFechaInicio())) {
            mensaje = "La fecha de suspensión debe ser válida y no anterior al inicio";
            return ResultadoOperacion::DatosInvalidos;
        }
    }

    // Al reactivar, se limpia la fecha efectiva para cumplir la regla
    // de la base de datos para tratamientos ACTIVO.
    if (nuevoEstado == "ACTIVO") {
        fechaEfectiva = "";
    }

    if (!repository.cambiarEstado(
            idTratamiento, nuevoEstado, fechaEfectiva, error)) {
        mensaje = error.empty()
            ? "No se pudo cambiar el estado del tratamiento"
            : error;

        return ResultadoOperacion::ErrorInterno;
    }

    mensaje = "Estado del tratamiento actualizado correctamente";
    return ResultadoOperacion::Exito;
}