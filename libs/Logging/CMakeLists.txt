find_package(spdlog CONFIG REQUIRED)

add_library(LibrovaLogging STATIC
    Logging.cpp
    Logging.hpp
)

target_include_directories(LibrovaLogging
    PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
)

target_link_libraries(LibrovaLogging
    PUBLIC
        spdlog::spdlog
)

target_compile_features(LibrovaLogging PUBLIC cxx_std_20)
