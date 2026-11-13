#pragma once

#include <string>
#include <vector>

#include "Cross/ResultadoOperacion.h"
#include "Models/AccionEmergencia.h"
#include "Models/Emergencia.h"

class EmergenciaRepository {
public:
    // El usuario tiene el permiso GESTIONAR_EMERGENCIAS por su rol.
    bool usuarioPuedeGestionarEmergencias(long long idUsuario) const;
    // Tipos válidos: los de la tabla protocolos (única lista del sistema).
    bool listarTipos(std::vector<std::string>& tipos, std::string& error) const;
    // false si el paciente no existe.
    bool buscarPaciente(long long idPaciente, PacienteEmergencia& paciente) const;
    bool tieneHistoria(long long idPaciente) const;

    // Registra TODO en una sola transacción, o nada:
    //   1. la emergencia (ACTIVA, con la fecha y hora de la base, hora de Colombia),
    //   2. su vínculo con cada paciente (emergencia_paciente),
    //   3. un evento DIAGNOSTICO en la historia de cada paciente (eventos_medicos),
    //   4. la alerta EMERGENCIA para los responsables,
    //   5. el registro en logs_auditoria.
    bool crear(const NuevaEmergencia& datos,
               long long idUsuario,
               Emergencia& creada,
               std::string& error) const;

    // ---- KAN-51: acciones, cambio de estado y finalización ----
    // Todas devuelven: Exito, NoEncontrado (la emergencia no existe),
    // Conflicto (el estado actual no lo permite) o ErrorInterno. Ante un
    // fallo, `error` trae el motivo.

    // Historial de acciones de la emergencia, de la más antigua a la más reciente.
    ResultadoOperacion listarAcciones(long long idEmergencia,
                                      std::vector<AccionEmergencia>& acciones,
                                      std::string& error) const;

    // Guarda la acción (la fecha y hora la pone la base) y su registro en
    // logs_auditoria, en una sola transacción. No admite emergencias FINALIZADAS.
    ResultadoOperacion registrarAccion(long long idEmergencia,
                                       long long idUsuario,
                                       const std::string& descripcion,
                                       AccionEmergencia& creada,
                                       std::string& error) const;

    // ACTIVA -> EN_ATENCION, con su registro en logs_auditoria.
    ResultadoOperacion marcarEnAtencion(long long idEmergencia,
                                        long long idUsuario,
                                        Emergencia& actualizada,
                                        std::string& error) const;

    // ACTIVA o EN_ATENCION -> FINALIZADA. fecha_fin la pone la base. Registra
    // el cambio en logs_auditoria.
    ResultadoOperacion finalizar(long long idEmergencia,
                                 long long idUsuario,
                                 Emergencia& actualizada,
                                 std::string& error) const;
};