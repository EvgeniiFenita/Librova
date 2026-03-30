add_library(LibriFlowPipeHost STATIC
    NamedPipeHost.cpp
    NamedPipeHost.hpp
)

target_include_directories(LibriFlowPipeHost
    PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
)

target_link_libraries(LibriFlowPipeHost
    PUBLIC
        LibriFlowPipeTransport
)

target_compile_features(LibriFlowPipeHost PUBLIC cxx_std_20)
