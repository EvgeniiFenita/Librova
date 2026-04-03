add_library(LibrovaImporting STATIC
    SingleFileImportCoordinator.cpp
    SingleFileImportCoordinator.hpp
)

target_include_directories(LibrovaImporting
    PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
)

target_link_libraries(LibrovaImporting
    PUBLIC
        LibrovaDomain
        LibrovaImportConversion
        LibrovaManagedPaths
        LibrovaParserRegistry
)

target_compile_features(LibrovaImporting PUBLIC cxx_std_20)
