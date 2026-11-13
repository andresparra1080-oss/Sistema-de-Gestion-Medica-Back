#pragma once

#include <string>

// Representa un registro de la tabla tratamientos.
class Tratamiento {
public:
    Tratamiento();

    void setIdTratamiento(long long idTratamiento);
    long long getIdTratamiento() const;

    void setIdPaciente(long long idPaciente);
    long long getIdPaciente() const;

    void setIdMedicamento(long long idMedicamento);
    long long getIdMedicamento() const;

    void setIdUsuarioRegistra(long long idUsuarioRegistra);
    long long getIdUsuarioRegistra() const;

    void setDosis(const std::string& dosis);
    std::string getDosis() const;

    void setFrecuencia(const std::string& frecuencia);
    std::string getFrecuencia() const;

    void setIntervaloHoras(int intervaloHoras);
    void quitarIntervaloHoras();
    int getIntervaloHoras() const;
    bool tieneIntervaloHoras() const;

    void setFechaInicio(const std::string& fechaInicio);
    std::string getFechaInicio() const;

    void setFechaFin(const std::string& fechaFin);
    std::string getFechaFin() const;

    void setFechaFinalizacionEfectiva(const std::string& fecha);
    std::string getFechaFinalizacionEfectiva() const;

    void setEstadoTratamiento(const std::string& estado);
    std::string getEstadoTratamiento() const;

private:
    long long idTratamiento_;
    long long idPaciente_;
    long long idMedicamento_;
    long long idUsuarioRegistra_;
    std::string dosis_;
    std::string frecuencia_;
    int intervaloHoras_;
    bool tieneIntervaloHoras_;
    std::string fechaInicio_;
    std::string fechaFin_;
    std::string fechaFinalizacionEfectiva_;
    std::string estadoTratamiento_;
};