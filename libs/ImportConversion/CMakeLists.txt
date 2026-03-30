add_library(LibriFlowImportConversion STATIC
    ImportConversionPolicy.cpp
    ImportConversionPolicy.hpp
)

target_include_directories(LibriFlowImportConversion
    PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
)

target_link_libraries(LibriFlowImportConversion
    PUBLIC
        LibriFlowDomain
)

target_compile_features(LibriFlowImportConversion PUBLIC cxx_std_20)
