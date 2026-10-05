#pragma once

#include <string>
#include <unordered_map>

class Configuracion {
public:
    static Configuracion& instancia();

    // Lee un archivo de entorno en formato KEY=VALUE.
    void cargarDesdeArchivo(const std::string& ruta);

    // Devuelve el valor como texto, o un valor por defecto si no existe.
    std::string obtenerCadena(const std::string& clave, const std::string& valorPredeterminado = "") const;

    // Convierte un valor del archivo .env a entero.
    int obtenerEntero(const std::string& clave, int valorPredeterminado = 0) const;

    // Convierte valores como "true", "1", "yes" a booleano.
    bool obtenerBooleano(const std::string& clave, bool valorPredeterminado = false) const;

private:
    Configuracion() = default;

    std::unordered_map<std::string, std::string> valores_;

    static std::string recortar(const std::string& valor);
    static std::string normalizarClave(const std::string& clave);
};
