add_library(LibriFlowPipeTransport STATIC
    NamedPipeChannel.cpp
    NamedPipeChannel.hpp
    PipeProtocol.cpp
    PipeProtocol.hpp
    PipeRequestDispatcher.cpp
    PipeRequestDispatcher.hpp
)

target_include_directories(LibriFlowPipeTransport
    PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
)

target_link_libraries(LibriFlowPipeTransport
    PUBLIC
        LibriFlowProtoContracts
        LibriFlowProtoServices
)

target_compile_features(LibriFlowPipeTransport PUBLIC cxx_std_20)
