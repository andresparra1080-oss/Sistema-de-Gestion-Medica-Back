cmake_minimum_required(VERSION 3.16)

project(HeartMedBackend LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

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

add_executable(heartmed-backend
    entry/main.cpp
)

target_link_libraries(heartmed-backend
    PRIVATE
        Crow::Crow
        nlohmann_json::nlohmann_json
        ws2_32
        wsock32
)
