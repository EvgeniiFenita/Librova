add_library(LibriFlowConverterRuntime STATIC
    ExternalBookConverter.cpp
)

target_include_directories(LibriFlowConverterRuntime PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(LibriFlowConverterRuntime PUBLIC
    LibriFlowConverterCommand
    LibriFlowDomain
)

target_compile_features(LibriFlowConverterRuntime PUBLIC cxx_std_20)
