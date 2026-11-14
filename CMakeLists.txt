cmake_minimum_required(VERSION 3.16)

project(HeartMedBackend LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

if (WIN32 AND EXISTS "C:/msys64/mingw64")
    set(OPENSSL_ROOT_DIR "C:/msys64/mingw64" CACHE PATH "OpenSSL root directory" FORCE)
    list(APPEND CMAKE_PREFIX_PATH "C:/msys64/mingw64")
endif()

include(FetchContent)
find_package(SQLite3 REQUIRED)
find_package(OpenSSL REQUIRED COMPONENTS Crypto)

FetchContent_Declare(
    crow
    GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
    GIT_TAG master
)
FetchContent_MakeAvailable(crow)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

if (MSVC)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

add_executable(heartmed-backend
    entry/main.cpp
    Models/Usuarios.cpp
    Models/Paciente.cpp
    Models/Lote.cpp
    Models/Medicamento.cpp
    Models/MovimientoInventario.cpp
    Models/Alergia.cpp
    Models/HistoriaClinica.cpp
    Models/Tratamiento.cpp
    Repositories/UsuarioRepository.cpp
    Repositories/GestionUsuarioRepository.cpp
    Repositories/PacienteRepository.cpp
    Repositories/InventarioRepository.cpp
    Repositories/MovimientoRepository.cpp
    Repositories/HistoriaRepository.cpp
    Repositories/TratamientoRepository.cpp
    Services/AuthService.cpp
    Services/GestionUsuarioService.cpp
    Services/PacienteService.cpp
    Services/InventarioService.cpp
    Services/MovimientoService.cpp
    Services/HistoriaService.cpp
    Services/TratamientoService.cpp
    Routes/AuthRoutes.cpp
    Routes/GestionUsuarioRoutes.cpp
    Routes/PacienteRoutes.cpp
    Routes/InventarioRoutes.cpp
    Routes/MovimientoRoutes.cpp
    Routes/HistoriaRoutes.cpp
    Routes/TratamientoRoutes.cpp
    Database/SQLite.cpp
    Cross/Config.cpp
    Cross/PasswordHash.cpp
    Cross/JwtToken.cpp
    Cross/ApiHttp.cpp
    Models/Paciente.h
    Models/Lote.h
    Models/Medicamento.h
    Models/MovimientoInventario.h
    Models/Alergia.h
    Models/HistoriaClinica.h
    Models/Tratamiento.h
    Repositories/UsuarioRepository.h
    Repositories/GestionUsuarioRepository.h
    Repositories/PacienteRepository.h
    Repositories/InventarioRepository.h
    Repositories/MovimientoRepository.h
    Repositories/HistoriaRepository.h
    Repositories/TratamientoRepository.h
    Routes/AuthRoutes.h
    Routes/GestionUsuarioRoutes.h
    Routes/PacienteRoutes.h
    Routes/InventarioRoutes.h
    Routes/MovimientoRoutes.h
    Routes/HistoriaRoutes.h
    Routes/TratamientoRoutes.h
    Services/PacienteService.h
    Services/GestionUsuarioService.h
    Models/Usuario.h
    Services/InventarioService.h
    Services/MovimientoService.h
    Services/HistoriaService.h
    Services/TratamientoService.h
    Database/SQLite.h
    Cross/Config.h
    Cross/PasswordHash.h
    Cross/JwtToken.h
    Cross/ApiHttp.h
    Cross/ResultadoOperacion.h
    Repositories/AlertaRepository.cpp
    Services/AlertaService.cpp
    Routes/AlertaRoutes.cpp
    Models/Alerta.h
    Repositories/AlertaRepository.h
    Services/AlertaService.h
    Routes/AlertaRoutes.h
)

target_include_directories(heartmed-backend
    PRIVATE
        ${CMAKE_SOURCE_DIR}
)

target_link_libraries(heartmed-backend
    PRIVATE
        Crow::Crow
        nlohmann_json::nlohmann_json
        SQLite3::SQLite3
        OpenSSL::Crypto
        ws2_32
        wsock32
)

if (BUILD_TESTING)
    add_executable(gestion-usuario-tests
        tests/GestionUsuarioTests.cpp
        Repositories/GestionUsuarioRepository.cpp
        Services/GestionUsuarioService.cpp
        Database/SQLite.cpp
        Cross/PasswordHash.cpp
    )

    target_include_directories(gestion-usuario-tests
        PRIVATE
            ${CMAKE_SOURCE_DIR}
    )

    target_link_libraries(gestion-usuario-tests
        PRIVATE
            SQLite::SQLite3
            OpenSSL::Crypto
    )

    add_test(NAME gestion-usuario-tests COMMAND gestion-usuario-tests)
endif()
