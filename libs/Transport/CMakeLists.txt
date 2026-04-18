add_library(LibrovaTransport STATIC
    NamedPipeChannel.cpp
    NamedPipeClient.cpp
    NamedPipeHost.cpp
    PipeProtocol.cpp
    PipeRequestDispatcher.cpp
)

target_include_directories(LibrovaTransport PUBLIC "${CMAKE_SOURCE_DIR}/libs")

target_link_libraries(LibrovaTransport
    PUBLIC
        LibrovaProtoContracts
        LibrovaProtoServices
    PRIVATE
        LibrovaFoundation
)

target_compile_features(LibrovaTransport PUBLIC cxx_std_20)
