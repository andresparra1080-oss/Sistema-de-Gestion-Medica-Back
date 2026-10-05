#pragma once

#include <string>

class UsuarioRepository {
public:
    bool existeUsuario(const std::string& email) const;
    bool validarCredenciales(const std::string& email, const std::string& password) const;
    bool buscarPorCorreo(const std::string& email, std::string& nombre) const;
    bool buscarPorCorreoYPassword(const std::string& email, const std::string& password) const;
    std::string obtenerHashContrasena(const std::string& email) const;
    bool usuarioActivo(const std::string& email) const;
};
