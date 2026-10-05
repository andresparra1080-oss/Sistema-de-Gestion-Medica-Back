#include "Cross/PasswordHash.h"

#include <openssl/sha.h>

#include <iomanip>
#include <sstream>
#include <string>

namespace {
std::string aHex(const unsigned char* datos, std::size_t longitud) {
    std::ostringstream flujo;
    flujo << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < longitud; ++i) {
        flujo << std::setw(2) << static_cast<int>(datos[i]);
    }
    return flujo.str();
}
}

std::string HashContrasena::crear(const std::string& contrasena) {
    unsigned char resumen[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(contrasena.c_str()), contrasena.size(), resumen);
    return aHex(resumen, SHA256_DIGEST_LENGTH);
}

bool HashContrasena::verificar(const std::string& contrasena, const std::string& hashAlmacenado) {
    return crear(contrasena) == hashAlmacenado;
}
