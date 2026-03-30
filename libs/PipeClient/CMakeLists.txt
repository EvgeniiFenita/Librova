add_library(LibriFlowPipeClient STATIC
    NamedPipeClient.cpp
    NamedPipeClient.hpp
)

target_include_directories(LibriFlowPipeClient
    PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
)

target_link_libraries(LibriFlowPipeClient
    PUBLIC
        LibriFlowPipeTransport
        LibriFlowProtoContracts
)

target_compile_features(LibriFlowPipeClient PUBLIC cxx_std_20)
