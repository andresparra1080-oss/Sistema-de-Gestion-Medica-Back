#include "Models/Usuarios.h"

#include <iostream>
#include <string>

Usuarios::Usuarios() : nombre_("") {
}

void Usuarios::setNombre(const std::string& nombre) {
    nombre_ = nombre;
}

std::string Usuarios::getNombre() const {
    return nombre_;
}