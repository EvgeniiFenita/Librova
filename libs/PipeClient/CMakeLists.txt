add_library(LibrovaPipeClient STATIC
    NamedPipeClient.cpp
    NamedPipeClient.hpp
)

target_include_directories(LibrovaPipeClient
    PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
)

target_link_libraries(LibrovaPipeClient
    PUBLIC
        LibrovaPipeTransport
        LibrovaProtoContracts
)

target_compile_features(LibrovaPipeClient PUBLIC cxx_std_20)
