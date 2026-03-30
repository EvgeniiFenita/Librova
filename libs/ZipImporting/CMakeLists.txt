add_library(LibriFlowZipImporting STATIC
    ZipImportCoordinator.cpp
    ZipImportCoordinator.hpp
)

find_package(libzip CONFIG REQUIRED)

target_include_directories(LibriFlowZipImporting
    PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
)

target_link_libraries(LibriFlowZipImporting
    PUBLIC
        LibriFlowDomain
        LibriFlowImporting
        libzip::zip
)

target_compile_features(LibriFlowZipImporting PUBLIC cxx_std_20)
