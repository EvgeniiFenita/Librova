add_library(LibriFlowConverterCommand STATIC
    ConverterCommandBuilder.cpp
)

target_include_directories(LibriFlowConverterCommand PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(LibriFlowConverterCommand PUBLIC
    LibriFlowDomain
)

target_compile_features(LibriFlowConverterCommand PUBLIC cxx_std_20)
