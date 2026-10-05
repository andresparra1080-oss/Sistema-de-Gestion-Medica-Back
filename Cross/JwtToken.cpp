#include "Cross/JwtToken.h"

#include <openssl/sha.h>

#include <chrono>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include "Cross/Config.h"

namespace {
std::string aHex(const unsigned char* datos, std::size_t longitud) {
    std::ostringstream flujo;
    flujo << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < longitud; ++i) {
        flujo << std::setw(2) << static_cast<int>(datos[i]);
    }
    return flujo.str();
}

std::string firmaTokenPrueba(const std::string& secreto, const std::string& correo, const std::string& rol, long long expiracionUnix) {
    std::string entrada = secreto + "." + correo + "." + rol + "." + std::to_string(expiracionUnix);

    unsigned char resumen[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(entrada.c_str()), entrada.size(), resumen);
    return aHex(resumen, SHA256_DIGEST_LENGTH);
}

std::vector<std::string> dividir(const std::string& texto, char delimitador) {
    std::vector<std::string> partes;
    std::stringstream flujo(texto);
    std::string parte;
    while (std::getline(flujo, parte, delimitador)) {
        partes.push_back(parte);
    }
    return partes;
}
}

std::string TokenJwt::generar(const std::string& correo, const std::string& rol) {
    const std::string secreto = Configuracion::instancia().obtenerCadena("JWT_SECRET", "secret_de_prueba");
    const int minutosExpiracion = Configuracion::instancia().obtenerEntero("JWT_EXPIRATION_MINUTES", 60);

    const auto ahora = std::chrono::system_clock::now();
    const auto expiracionUnix = std::chrono::duration_cast<std::chrono::seconds>(
        ahora.time_since_epoch() + std::chrono::minutes(minutosExpiracion)
    ).count();

    const std::string firma = firmaTokenPrueba(secreto, correo, rol, expiracionUnix);
    return "heartmed." + correo + "." + rol + "." + std::to_string(expiracionUnix) + "." + firma;
}

bool TokenJwt::validar(const std::string& token, std::string& correo, std::string& rol) {
    if (token.empty()) {
        return false;
    }

    const std::vector<std::string> partes = dividir(token, '.');
    if (partes.size() != 5) {
        return false;
    }

    if (partes[0] != "heartmed") {
        return false;
    }

    const std::string secreto = Configuracion::instancia().obtenerCadena("JWT_SECRET", "secret_de_prueba");
    const long long expiracionUnix = std::stoll(partes[3]);
    const auto ahora = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    if (expiracionUnix < ahora) {
        return false;
    }

    const std::string firmaEsperada = firmaTokenPrueba(secreto, partes[1], partes[2], expiracionUnix);
    if (firmaEsperada != partes[4]) {
        return false;
    }

    correo = partes[1];
    rol = partes[2];
    return true;
}
