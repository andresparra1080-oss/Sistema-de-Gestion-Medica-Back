#pragma once

#include <string>
#include <vector>

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
};
