add_library(LibriFlowConverterConfiguration STATIC
    ConverterConfiguration.cpp
    ConverterConfiguration.hpp
)

target_include_directories(LibriFlowConverterConfiguration
    PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
)

target_link_libraries(LibriFlowConverterConfiguration
    PUBLIC
        LibriFlowConverterCommand
)

target_compile_features(LibriFlowConverterConfiguration PUBLIC cxx_std_20)
