#include "Services/InventarioService.h"

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
