add_library(LibrovaPipeTransport STATIC
    NamedPipeChannel.cpp
    NamedPipeChannel.hpp
    PipeProtocol.cpp
    PipeProtocol.hpp
    PipeRequestDispatcher.cpp
    PipeRequestDispatcher.hpp
)

target_include_directories(LibrovaPipeTransport
    PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
)

target_link_libraries(LibrovaPipeTransport
    PUBLIC
        LibrovaProtoContracts
        LibrovaProtoServices
)

target_compile_features(LibrovaPipeTransport PUBLIC cxx_std_20)
