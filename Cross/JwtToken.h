#pragma once

#include <string>


class TokenJwt {
public:
    static std::string generar(const std::string& correo, const std::string& rol);
    static bool validar(const std::string& token, std::string& correo, std::string& rol);
};
