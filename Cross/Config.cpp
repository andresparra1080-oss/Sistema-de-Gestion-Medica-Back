#include "Cross/Config.h"

#include <cctype>
#include <fstream>
#include <sstream>



Configuracion& Configuracion::instancia() {
    static Configuracion configuracion;
    return configuracion;
}

std::string Configuracion::recortar(const std::string& valor) {
    const std::string espacios = " \t\r\n";

    const std::size_t inicio = valor.find_first_not_of(espacios);
    if (inicio == std::string::npos) {
        return "";
    }

    const std::size_t fin = valor.find_last_not_of(espacios);
    return valor.substr(inicio, fin - inicio + 1);
}

std::string Configuracion::normalizarClave(const std::string& clave) {
    std::string claveNormalizada = clave;
    for (char& caracter : claveNormalizada) {
        if (caracter == '-') {
            caracter = '_';
        }
    }
    return claveNormalizada;
}

void Configuracion::cargarDesdeArchivo(const std::string& ruta) {
    std::ifstream archivo(ruta);
    if (!archivo.is_open()) {
        return;
    }

    std::string linea;
    while (std::getline(archivo, linea)) {
        const std::string limpia = recortar(linea);
        if (limpia.empty() || limpia[0] == '#') {
            continue;
        }

        const std::size_t posicionIgual = limpia.find('=');
        if (posicionIgual == std::string::npos) {
            continue;
        }

        std::string clave = recortar(limpia.substr(0, posicionIgual));
        std::string valor = recortar(limpia.substr(posicionIgual + 1));

        if (valor.size() >= 2 && valor.front() == '"' && valor.back() == '"') {
            valor = valor.substr(1, valor.size() - 2);
        }

        if (valor.size() >= 2 && valor.front() == '\'' && valor.back() == '\'') {
            valor = valor.substr(1, valor.size() - 2);
        }

        valores_[normalizarClave(clave)] = valor;
        valores_[clave] = valor;
    }
}

std::string Configuracion::obtenerCadena(const std::string& clave, const std::string& valorPredeterminado) const {
    const std::string claveNormalizada = normalizarClave(clave);

    const auto it = valores_.find(claveNormalizada);
    if (it != valores_.end()) {
        return it->second;
    }

    const auto alternativa = valores_.find(clave);
    if (alternativa != valores_.end()) {
        return alternativa->second;
    }

    return valorPredeterminado;
}

int Configuracion::obtenerEntero(const std::string& clave, int valorPredeterminado) const {
    const std::string valor = obtenerCadena(clave, "");
    if (valor.empty()) {
        return valorPredeterminado;
    }

    try {
        return std::stoi(valor);
    } catch (...) {
        return valorPredeterminado;
    }
}

bool Configuracion::obtenerBooleano(const std::string& clave, bool valorPredeterminado) const {
    const std::string valor = obtenerCadena(clave, "");
    if (valor.empty()) {
        return valorPredeterminado;
    }

    std::string valorNormalizado = valor;
    for (char& caracter : valorNormalizado) {
        caracter = static_cast<char>(std::tolower(static_cast<unsigned char>(caracter)));
    }

    if (valorNormalizado == "true" || valorNormalizado == "1" || valorNormalizado == "yes") {
        return true;
    }

    if (valorNormalizado == "false" || valorNormalizado == "0" || valorNormalizado == "no") {
        return false;
    }

    return valorPredeterminado;
}
