add_library(LibrovaApplicationClient STATIC
    ImportJobClient.cpp
    ImportJobClient.hpp
)

target_include_directories(LibrovaApplicationClient
    PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
)

target_link_libraries(LibrovaApplicationClient
    PUBLIC
        LibrovaApplicationJobs
        LibrovaTransport
        LibrovaRpc
)

target_compile_features(LibrovaApplicationClient PUBLIC cxx_std_20)
