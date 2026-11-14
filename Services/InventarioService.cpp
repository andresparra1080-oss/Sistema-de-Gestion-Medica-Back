#include "Services/InventarioService.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

#include "Repositories/InventarioRepository.h"

ResumenInventario::ResumenInventario()
    : total(0),
      bajoMinimo(0),
      porcentajeDisponibilidad(0),
      hayDatos(false) {
}

namespace {

std::string limpiarTexto(const std::string& valor) {
    std::string resultado = valor;
    const auto inicio = resultado.find_first_not_of(" \t\r\n");
    if (inicio == std::string::npos) {
        return "";
    }
    const auto fin = resultado.find_last_not_of(" \t\r\n");
    return resultado.substr(inicio, fin - inicio + 1);
}

bool esTipoInsumoValido(const std::string& valor) {
    return valor == "MEDICAMENTO" || valor == "INSUMO";
}

bool esUnidadMedidaValida(const std::string& valor) {
    static const std::vector<std::string> permitidas = {
        "UNIDAD", "TABLETA", "CAPSULA", "AMPOLLA", "FRASCO", "TUBO", "ROLLO", "ML"};
    return std::find(permitidas.begin(), permitidas.end(), valor) != permitidas.end();
}

ResultadoOperacion prepararMedicamento(const Medicamento& datos,
                                        Medicamento& limpio,
                                        std::string& mensaje) {
    const std::string nombre = limpiarTexto(datos.getNombre());
    const std::string tipo = limpiarTexto(datos.getTipoInsumo());
    const std::string unidad = limpiarTexto(datos.getUnidadMedida());
    const std::string descripcion = limpiarTexto(datos.getDescripcion());
    const long long umbralMinimo = datos.getUmbralMinimo();

    if (nombre.empty() || nombre.size() > 100) {
        mensaje = "El nombre del medicamento es obligatorio y admite máximo 100 caracteres";
        return ResultadoOperacion::DatosInvalidos;
    }

    if (!esTipoInsumoValido(tipo)) {
        mensaje = "El tipo de insumo debe ser MEDICAMENTO o INSUMO";
        return ResultadoOperacion::DatosInvalidos;
    }

    if (!esUnidadMedidaValida(unidad)) {
        mensaje = "La unidad de medida no es válida";
        return ResultadoOperacion::DatosInvalidos;
    }

    if (descripcion.size() > 500) {
        mensaje = "La descripción admite máximo 500 caracteres";
        return ResultadoOperacion::DatosInvalidos;
    }

    if (umbralMinimo < 0) {
        mensaje = "El umbral mínimo no puede ser negativo";
        return ResultadoOperacion::DatosInvalidos;
    }

    limpio = datos;
    limpio.setNombre(nombre);
    limpio.setTipoInsumo(tipo);
    limpio.setUnidadMedida(unidad);
    limpio.setDescripcion(descripcion);
    limpio.setCantidadTotalDisponible(0);
    limpio.setUmbralMinimo(umbralMinimo);
    mensaje.clear();
    return ResultadoOperacion::Exito;
}

}  // namespace

ResultadoOperacion InventarioService::crear(const Medicamento& datos,
                                            Medicamento& creado,
                                            std::string& mensaje) const {
    Medicamento limpio;
    const ResultadoOperacion validacion = prepararMedicamento(datos, limpio, mensaje);
    if (validacion != ResultadoOperacion::Exito) {
        return validacion;
    }

    InventarioRepository repository;
    long long idGenerado = 0;
    std::string error;
    if (!repository.crear(limpio, idGenerado, error)) {
        if (error.find("UNIQUE") != std::string::npos) {
            mensaje = "Ya existe un medicamento con ese nombre";
            return ResultadoOperacion::Duplicado;
        }
        mensaje = error.empty() ? "No se pudo registrar el medicamento" : error;
        return ResultadoOperacion::ErrorInterno;
    }

    limpio.setIdMedicamento(idGenerado);
    creado = limpio;
    mensaje = "Medicamento registrado";
    return ResultadoOperacion::Exito;
}

ResultadoOperacion InventarioService::actualizar(long long idMedicamento,
                                                const Medicamento& datos,
                                                std::string& mensaje) const {
    if (idMedicamento <= 0) {
        mensaje = "El identificador del medicamento no es válido";
        return ResultadoOperacion::DatosInvalidos;
    }

    Medicamento limpio;
    const ResultadoOperacion validacion = prepararMedicamento(datos, limpio, mensaje);
    if (validacion != ResultadoOperacion::Exito) {
        return validacion;
    }

    InventarioRepository repository;
    if (!repository.existeId(idMedicamento)) {
        mensaje = "No existe el medicamento indicado";
        return ResultadoOperacion::NoEncontrado;
    }

    std::string error;
    if (!repository.actualizar(idMedicamento, limpio, error)) {
        if (error.find("UNIQUE") != std::string::npos) {
            mensaje = "Ya existe un medicamento con ese nombre";
            return ResultadoOperacion::Duplicado;
        }
        mensaje = error.empty() ? "No se pudo actualizar el medicamento" : error;
        return ResultadoOperacion::ErrorInterno;
    }

    mensaje = "Medicamento actualizado";
    return ResultadoOperacion::Exito;
}

ResultadoOperacion InventarioService::listar(std::vector<Medicamento>& medicamentos,
                                             ResumenInventario& resumen,
                                             std::string& mensaje) const {
    InventarioRepository repository;
    std::string error;

    if (!repository.listar(medicamentos, error)) {
        mensaje = error.empty() ? "No se pudo consultar el inventario" : error;
        return ResultadoOperacion::ErrorInterno;
    }

    resumen = ResumenInventario();
    resumen.total = medicamentos.size();

    if (medicamentos.empty()) {
        mensaje = "Inventario vacío";
        return ResultadoOperacion::Exito;
    }

    for (const Medicamento& medicamento : medicamentos) {
        if (medicamento.estaBajoMinimo()) {
            ++resumen.bajoMinimo;
        }
    }

    // Disponibilidad general = qué porcentaje de los insumos tiene stock
    // suficiente. El "+ total / 2" redondea al entero más cercano sin usar
    // coma flotante.
    const std::size_t conStockSuficiente = resumen.total - resumen.bajoMinimo;
    resumen.porcentajeDisponibilidad =
        static_cast<int>((conStockSuficiente * 100 + resumen.total / 2) / resumen.total);
    resumen.hayDatos = true;

    mensaje = "Inventario consultado";
    return ResultadoOperacion::Exito;
}

ResultadoOperacion InventarioService::eliminar(long long idMedicamento,
                                                std::string& mensaje) const {
    if (idMedicamento <= 0) {
        mensaje = "El identificador del medicamento no es válido";
        return ResultadoOperacion::DatosInvalidos;
    }

    InventarioRepository repository;
    if (!repository.existeId(idMedicamento)) {
        mensaje = "No existe el medicamento indicado";
        return ResultadoOperacion::NoEncontrado;
    }

    std::string error;
    if (!repository.eliminar(idMedicamento, error)) {
        if (error.find("FOREIGN KEY") != std::string::npos ||
            error.find("fk_") != std::string::npos ||
            error.find("constraint") != std::string::npos) {
            mensaje = "No se puede eliminar el medicamento porque tiene lotes o movimientos asociados";
            return ResultadoOperacion::Conflicto;
        }

        mensaje = error.empty() ? "No se pudo eliminar el medicamento" : error;
        return ResultadoOperacion::ErrorInterno;
    }

    mensaje = "Medicamento eliminado";
    return ResultadoOperacion::Exito;
}
