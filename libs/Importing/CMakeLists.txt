add_library(LibriFlowImporting STATIC
    SingleFileImportCoordinator.cpp
    SingleFileImportCoordinator.hpp
)

target_include_directories(LibriFlowImporting
    PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
)

target_link_libraries(LibriFlowImporting
    PUBLIC
        LibriFlowDomain
        LibriFlowImportConversion
        LibriFlowParserRegistry
)

target_compile_features(LibriFlowImporting PUBLIC cxx_std_20)
