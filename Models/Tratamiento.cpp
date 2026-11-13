#include "Models/Tratamiento.h"

#include <string>

Tratamiento::Tratamiento()
    : idTratamiento_(0),
      idPaciente_(0),
      idMedicamento_(0),
      idUsuarioRegistra_(0),
      dosis_(""),
      frecuencia_(""),
      intervaloHoras_(0),
      tieneIntervaloHoras_(false),
      fechaInicio_(""),
      fechaFin_(""),
      fechaFinalizacionEfectiva_(""),
      estadoTratamiento_("ACTIVO") {
}

void Tratamiento::setIdTratamiento(long long id) {
    idTratamiento_ = id;
}

long long Tratamiento::getIdTratamiento() const {
    return idTratamiento_;
}

void Tratamiento::setIdPaciente(long long id) {
    idPaciente_ = id;
}

long long Tratamiento::getIdPaciente() const {
    return idPaciente_;
}

void Tratamiento::setIdMedicamento(long long id) {
    idMedicamento_ = id;
}

long long Tratamiento::getIdMedicamento() const {
    return idMedicamento_;
}

void Tratamiento::setIdUsuarioRegistra(long long id) {
    idUsuarioRegistra_ = id;
}

long long Tratamiento::getIdUsuarioRegistra() const {
    return idUsuarioRegistra_;
}

void Tratamiento::setDosis(const std::string& dosis) {
    dosis_ = dosis;
}

std::string Tratamiento::getDosis() const {
    return dosis_;
}

void Tratamiento::setFrecuencia(const std::string& frecuencia) {
    frecuencia_ = frecuencia;
}

std::string Tratamiento::getFrecuencia() const {
    return frecuencia_;
}

void Tratamiento::setIntervaloHoras(int intervaloHoras) {
    intervaloHoras_ = intervaloHoras;
    tieneIntervaloHoras_ = true;
}

void Tratamiento::quitarIntervaloHoras() {
    intervaloHoras_ = 0;
    tieneIntervaloHoras_ = false;
}

int Tratamiento::getIntervaloHoras() const {
    return intervaloHoras_;
}

bool Tratamiento::tieneIntervaloHoras() const {
    return tieneIntervaloHoras_;
}

void Tratamiento::setFechaInicio(const std::string& fechaInicio) {
    fechaInicio_ = fechaInicio;
}

std::string Tratamiento::getFechaInicio() const {
    return fechaInicio_;
}

void Tratamiento::setFechaFin(const std::string& fechaFin) {
    fechaFin_ = fechaFin;
}

std::string Tratamiento::getFechaFin() const {
    return fechaFin_;
}

void Tratamiento::setFechaFinalizacionEfectiva(const std::string& fecha) {
    fechaFinalizacionEfectiva_ = fecha;
}

std::string Tratamiento::getFechaFinalizacionEfectiva() const {
    return fechaFinalizacionEfectiva_;
}

void Tratamiento::setEstadoTratamiento(const std::string& estado) {
    estadoTratamiento_ = estado;
}

std::string Tratamiento::getEstadoTratamiento() const {
    return estadoTratamiento_;
}