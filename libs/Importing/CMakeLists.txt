add_library(LibrovaImporting STATIC
    ImportPerfTracker.cpp
    ImportPerfTracker.hpp
    SingleFileImportCoordinator.cpp
    SingleFileImportCoordinator.hpp
    WriterDispatchingRepository.cpp
    WriterDispatchingRepository.hpp
)

target_include_directories(LibrovaImporting
    PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
)

target_link_libraries(LibrovaImporting
    PUBLIC
        LibrovaDomain
        LibrovaImportConversion
        LibrovaParserRegistry
        LibrovaUnicode
    PRIVATE
        LibrovaBookDatabase
        LibrovaHashing
        LibrovaLogging
)

target_compile_features(LibrovaImporting PUBLIC cxx_std_20)
