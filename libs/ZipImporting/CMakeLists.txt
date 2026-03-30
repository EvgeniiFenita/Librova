add_library(LibrovaZipImporting STATIC
    ZipImportCoordinator.cpp
    ZipImportCoordinator.hpp
)

find_package(libzip CONFIG REQUIRED)

target_include_directories(LibrovaZipImporting
    PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
)

target_link_libraries(LibrovaZipImporting
    PUBLIC
        LibrovaDomain
        LibrovaImporting
        libzip::zip
)

target_compile_features(LibrovaZipImporting PUBLIC cxx_std_20)
