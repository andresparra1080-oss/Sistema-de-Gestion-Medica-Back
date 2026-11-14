cmake_minimum_required(VERSION 3.16)

project(HeartMedBackend LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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

if(HEARTMED_CPP_SOURCES STREQUAL "")
    message(FATAL_ERROR "No se encontró ningún archivo .cpp en los módulos del proyecto. Revisa la estructura del repositorio.")
endif()

list(SORT HEARTMED_CPP_SOURCES)

add_executable(heartmed-backend
    ${HEARTMED_CPP_SOURCES}
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
