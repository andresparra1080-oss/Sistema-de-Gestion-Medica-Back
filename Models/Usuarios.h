#pragma once

#include <string>

class Usuarios {
public:
    Usuarios();

    void setNombre(const std::string& nombre);
    std::string getNombre() const;

private:
    std::string nombre_;
};
