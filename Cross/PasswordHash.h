#pragma once

#include <string>


class HashContrasena {
public:
    static std::string crear(const std::string& contrasena);
    static bool verificar(const std::string& contrasena, const std::string& hashAlmacenado);
};
