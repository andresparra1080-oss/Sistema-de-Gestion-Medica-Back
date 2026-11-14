#include "Services/GestionUsuarioService.h"

#include <string>
#include <vector>

#include "Cross/PasswordHash.h"
#include "Repositories/GestionUsuarioRepository.h"

namespace {

std::string limpiarTexto(const std::string& valor) {
    const auto inicio = valor.find_first_not_of(" \t\r\n");
    if (inicio == std::string::npos) {
        return "";
    }

    const auto fin = valor.find_last_not_of(" \t\r\n");
    return valor.substr(inicio, fin - inicio + 1);
}

bool tieneFormatoEmailValido(const std::string& email) {
    if (email.empty() || email.size() > 150 || email.find(' ') != std::string::npos) {
        return false;
    }

    const auto arroba = email.find('@');
    if (arroba == std::string::npos || arroba == 0 || arroba + 1 >= email.size()) {
        return false;
    }

    const auto punto = email.find('.', arroba + 1);
    return punto != std::string::npos && punto > arroba + 1 && punto + 1 < email.size();
}

}  // namespace

ResultadoOperacion GestionUsuarioService::listar(long long idUsuarioSolicitante,
                                                  std::vector<Usuario>& usuarios,
                                                  std::string& mensaje) const {
    if (idUsuarioSolicitante < 1 || idUsuarioSolicitante > 4) {
        mensaje = "Acceso denegado: solo los usuarios 1, 2, 3 y 4 pueden listar usuarios";
        return ResultadoOperacion::SinPermiso;
    }

    GestionUsuarioRepository repository;
    std::string error;
    if (!repository.listar(usuarios, error)) {
        mensaje = error.empty() ? "No se pudo consultar la lista de usuarios" : error;
        return ResultadoOperacion::ErrorInterno;
    }

    mensaje = "Listado de usuarios";
    return ResultadoOperacion::Exito;
}

ResultadoOperacion GestionUsuarioService::crear(long long idUsuarioSolicitante,
                                                 const Usuario& datos,
                                                 const std::string& contrasena,
                                                 Usuario& creado,
                                                 std::string& mensaje) const {
    if (idUsuarioSolicitante != 1) {
        mensaje = "Acceso denegado: solo el usuario 1 puede crear usuarios";
        return ResultadoOperacion::SinPermiso;
    }

    Usuario usuario = datos;
    usuario.nombre = limpiarTexto(datos.nombre);
    usuario.correo = limpiarTexto(datos.correo);
    const std::string clave = limpiarTexto(contrasena);

    if (usuario.nombre.empty() || usuario.nombre.size() > 100) {
        mensaje = "El nombre es obligatorio y admite máximo 100 caracteres";
        return ResultadoOperacion::DatosInvalidos;
    }

    if (!tieneFormatoEmailValido(usuario.correo)) {
        mensaje = "El correo debe ser válido, no contener espacios y admitir máximo 150 caracteres";
        return ResultadoOperacion::DatosInvalidos;
    }

    if (clave.size() < 6) {
        mensaje = "La contraseña debe tener al menos 6 caracteres";
        return ResultadoOperacion::DatosInvalidos;
    }

    if (usuario.idRol <= 0) {
        mensaje = "El id_rol debe ser un entero positivo";
        return ResultadoOperacion::DatosInvalidos;
    }

    GestionUsuarioRepository repository;
    std::string error;
    if (!repository.existeRol(usuario.idRol, error)) {
        if (!error.empty()) {
            mensaje = error;
            return ResultadoOperacion::ErrorInterno;
        }
        mensaje = "El rol indicado no existe";
        return ResultadoOperacion::DatosInvalidos;
    }

    const ResultadoCreacionUsuario resultado =
        repository.crear(usuario, HashContrasena::crear(clave), idUsuarioSolicitante, creado, error);
    if (resultado == ResultadoCreacionUsuario::CorreoDuplicado) {
        mensaje = "Ya existe un usuario con ese correo";
        return ResultadoOperacion::Duplicado;
    }
    if (resultado == ResultadoCreacionUsuario::Error) {
        mensaje = error.empty() ? "No se pudo crear el usuario" : error;
        return ResultadoOperacion::ErrorInterno;
    }

    mensaje = "Usuario creado correctamente";
    return ResultadoOperacion::Exito;
}
