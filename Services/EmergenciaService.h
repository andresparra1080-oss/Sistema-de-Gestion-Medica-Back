#pragma once

#include <string>
#include <vector>

#include "Cross/ResultadoOperacion.h"
#include "Models/AccionEmergencia.h"
#include "Models/CambioEmergencia.h"
#include "Models/Emergencia.h"
#include "Repositories/EmergenciaRepository.h"

class EmergenciaService {
public:
    // Registra una emergencia y la asocia a uno o varios tripulantes.
    // `correoUsuario` sale del token, no del cuerpo: quien registra es quien
    // firma la petición. En `datos.pacientes` solo se lee idPaciente; los
    // nombres los completa el servicio desde la base.
    ResultadoOperacion registrar(const NuevaEmergencia& datos,
                                 const std::string& correoUsuario,
                                 Emergencia& creada,
                                 std::string& mensaje) const;

    // ---- KAN-51 ----
    // En todas, `correoUsuario` sale del token: quien ejecuta es quien firma la
    // petición, nunca un dato del cuerpo. Exigen el permiso GESTIONAR_EMERGENCIAS.

    // Guarda una acción de atención en la emergencia. La descripción no puede
    // estar vacía; la fecha y hora las pone la base.
    ResultadoOperacion registrarAccion(long long idEmergencia,
                                       const std::string& descripcion,
                                       const std::string& correoUsuario,
                                       AccionEmergencia& creada,
                                       std::string& mensaje) const;

    // Historial de acciones de la emergencia.
    ResultadoOperacion listarAcciones(long long idEmergencia,
                                      const std::string& correoUsuario,
                                      std::vector<AccionEmergencia>& acciones,
                                      std::string& mensaje) const;

    // ACTIVA -> EN_ATENCION.
    ResultadoOperacion marcarEnAtencion(long long idEmergencia,
                                        const std::string& correoUsuario,
                                        Emergencia& actualizada,
                                        std::string& mensaje) const;

    // ACTIVA o EN_ATENCION -> FINALIZADA (con fecha_fin automática).
    ResultadoOperacion finalizar(long long idEmergencia,
                                 const std::string& correoUsuario,
                                 Emergencia& actualizada,
                                 std::string& mensaje) const;

    // Elimina una emergencia cerrada bajo permiso de gestión.
    ResultadoOperacion eliminar(long long idEmergencia,
                                 const std::string& correoUsuario,
                                 std::string& mensaje) const;

    // ---- Consultas (KAN-48, KAN-52, KAN-54) ----
    // `correoUsuario` sale del token. Basta una cuenta activa (ver
    // EXIGIR_PERMISO_PARA_LEER en EmergenciaService.cpp).

    // Detalle de una emergencia con sus pacientes. NoEncontrado si no existe.
    ResultadoOperacion obtener(long long idEmergencia,
                               const std::string& correoUsuario,
                               Emergencia& emergencia,
                               std::string& mensaje) const;

    // Lista de emergencias, las más urgentes y recientes primero.
    // soloActivas = true devuelve solo ACTIVA y EN_ATENCION.
    ResultadoOperacion listar(bool soloActivas,
                              const std::string& correoUsuario,
                              std::vector<Emergencia>& emergencias,
                              std::string& mensaje) const;

    // Cambios de la emergencia y de sus acciones, del más reciente al más antiguo.
    ResultadoOperacion listarHistorial(long long idEmergencia,
                                       const std::string& correoUsuario,
                                       std::vector<CambioEmergencia>& historial,
                                       std::string& mensaje) const;
};