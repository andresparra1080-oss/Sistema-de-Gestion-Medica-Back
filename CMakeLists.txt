cmake_minimum_required(VERSION 3.16)

if(WIN32)
    set(MSYS2_MINGW64 "C:/msys64/mingw64/bin")
    if(EXISTS "${MSYS2_MINGW64}/gcc.exe")
        set(CMAKE_C_COMPILER "${MSYS2_MINGW64}/gcc.exe" CACHE FILEPATH "MSYS2 MinGW C compiler" FORCE)
    endif()
    if(EXISTS "${MSYS2_MINGW64}/g++.exe")
        set(CMAKE_CXX_COMPILER "${MSYS2_MINGW64}/g++.exe" CACHE FILEPATH "MSYS2 MinGW C++ compiler" FORCE)
    endif()
    if(EXISTS "${MSYS2_MINGW64}/mingw32-make.exe")
        set(CMAKE_MAKE_PROGRAM "${MSYS2_MINGW64}/mingw32-make.exe" CACHE FILEPATH "MSYS2 MinGW make" FORCE)
    endif()
endif()

project(HeartMedBackend LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Redirigir la salida del ejecutable dentro de la carpeta build del proyecto (build/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin")

include(FetchContent)

# --- SQLite3 ---
# 1) SQLite instalado en el sistema (CMake lo expone como SQLite::SQLite3)
# 2) Si el sistema no lo tiene, se compila el de Database/sqlite3/ (fallback local)
# En todos los casos el proyecto enlaza contra SQLite3::SQLite3.
if(NOT TARGET SQLite3::SQLite3)
    find_package(SQLite3 QUIET)

    if(TARGET SQLite::SQLite3)
        add_library(SQLite3::SQLite3 ALIAS SQLite::SQLite3)
    elseif(SQLite3_FOUND)
        find_path(SQLite3_INCLUDE_DIR NAMES sqlite3.h REQUIRED)
        find_library(SQLite3_LIBRARY NAMES sqlite3 REQUIRED)

        add_library(SQLite3::SQLite3 UNKNOWN IMPORTED)
        set_target_properties(SQLite3::SQLite3 PROPERTIES
            IMPORTED_LOCATION "${SQLite3_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${SQLite3_INCLUDE_DIR}"
        )
    elseif(EXISTS "${CMAKE_SOURCE_DIR}/Database/sqlite3/sqlite3.c")
        add_library(sqlite3_static STATIC Database/sqlite3/sqlite3.c)
        target_include_directories(sqlite3_static PUBLIC Database/sqlite3)
        add_library(SQLite3::SQLite3 ALIAS sqlite3_static)
    else()
        message(FATAL_ERROR "No se encontró SQLite3 en el sistema ni los archivos locales en Database/sqlite3/")
    endif()
endif()

# --- OpenSSL Configuration ---
# Si se pasa -DOPENSSL_ROOT_DIR=... o existe la variable de entorno, se respeta.
# Si no, se prueban rutas comunes de instalación en Windows.
if (WIN32 AND NOT OPENSSL_ROOT_DIR AND NOT DEFINED ENV{OPENSSL_ROOT_DIR})
    set(_OPENSSL_CANDIDATES
        "C:/Program Files/OpenSSL-Win64"
        "C:/Program Files/OpenSSL"
        "C:/OpenSSL-Win64"
        "C:/Program Files/FireDaemon OpenSSL 3"
        "C:/ProgramData/chocolatey/lib/openssl/tools"
    )
    foreach(_candidate IN LISTS _OPENSSL_CANDIDATES)
        if(EXISTS "${_candidate}/include/openssl/ssl.h")
            set(OPENSSL_ROOT_DIR "${_candidate}" CACHE PATH "OpenSSL root directory")
            message(STATUS "OpenSSL encontrado en: ${_candidate}")
            break()
        endif()
    endforeach()
endif()

find_package(OpenSSL REQUIRED COMPONENTS Crypto)

# --- Asio (Dependencia requerida por Crow) ---
FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG asio-1-30-2
)
FetchContent_MakeAvailable(asio)

if(asio_SOURCE_DIR)
    set(ASIO_INCLUDE_DIR "${asio_SOURCE_DIR}/asio/include" CACHE PATH "Path to Asio headers" FORCE)
    include_directories(${ASIO_INCLUDE_DIR})
endif()

# --- Crow Framework ---
FetchContent_Declare(
    crow
    GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
    GIT_TAG master
)
FetchContent_MakeAvailable(crow)

# --- nlohmann_json ---
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

if (MSVC)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS _WIN32_WINNT=0x0A00)
endif()

# Recopilación automática de fuentes por carpetas
set(HEARTMED_SOURCE_DIRS
    ${CMAKE_SOURCE_DIR}/entry
    ${CMAKE_SOURCE_DIR}/Cross
    ${CMAKE_SOURCE_DIR}/Database
    ${CMAKE_SOURCE_DIR}/Models
    ${CMAKE_SOURCE_DIR}/Repositories
    ${CMAKE_SOURCE_DIR}/Routes
    ${CMAKE_SOURCE_DIR}/Services
)

set(HEARTMED_CPP_SOURCES)
foreach(dir IN LISTS HEARTMED_SOURCE_DIRS)
    file(GLOB dir_sources CONFIGURE_DEPENDS "${dir}/*.cpp")
    list(APPEND HEARTMED_CPP_SOURCES ${dir_sources})
endforeach()

# Módulo de Auditoría (explícito)
list(APPEND HEARTMED_CPP_SOURCES
    ${CMAKE_SOURCE_DIR}/Repositories/AuditoriaRepository.cpp
    ${CMAKE_SOURCE_DIR}/Services/AuditoriaService.cpp
    ${CMAKE_SOURCE_DIR}/Routes/AuditoriaRoutes.cpp
)
list(REMOVE_DUPLICATES HEARTMED_CPP_SOURCES)

if(HEARTMED_CPP_SOURCES STREQUAL "")
    message(FATAL_ERROR "No se encontró ningún archivo .cpp en los módulos del proyecto. Revisa la estructura del repositorio.")
endif()

list(SORT HEARTMED_CPP_SOURCES)
list(FILTER HEARTMED_CPP_SOURCES EXCLUDE REGEX ".*/tmp_test\\.cpp$")

add_executable(heartmed-backend
    ${HEARTMED_CPP_SOURCES}
)

target_include_directories(heartmed-backend
    PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/Routes
        ${CMAKE_SOURCE_DIR}/Models
        ${CMAKE_SOURCE_DIR}/Repositories
        ${CMAKE_SOURCE_DIR}/Services
        ${CMAKE_SOURCE_DIR}/Database
        ${CMAKE_SOURCE_DIR}/Cross
        ${CMAKE_SOURCE_DIR}/entry
)

target_link_libraries(heartmed-backend
    PRIVATE
        Crow::Crow
        nlohmann_json::nlohmann_json
        SQLite3::SQLite3
        OpenSSL::Crypto
        $<$<PLATFORM_ID:Windows>:ws2_32>
        $<$<PLATFORM_ID:Windows>:wsock32>
        $<$<PLATFORM_ID:Windows>:bcrypt>
)