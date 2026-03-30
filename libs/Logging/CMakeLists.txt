find_package(spdlog CONFIG REQUIRED)

add_library(LibriFlowLogging STATIC
    Logging.cpp
    Logging.hpp
)

target_include_directories(LibriFlowLogging
    PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
)

target_link_libraries(LibriFlowLogging
    PUBLIC
        spdlog::spdlog
)

target_compile_features(LibriFlowLogging PUBLIC cxx_std_20)
