add_library(LibriFlowCoreHost STATIC
    HostOptions.cpp
    HostOptions.hpp
)

target_include_directories(LibriFlowCoreHost
    PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
)

target_link_libraries(LibriFlowCoreHost
    PUBLIC
        LibriFlowConverterConfiguration
)

target_compile_features(LibriFlowCoreHost PUBLIC cxx_std_20)
