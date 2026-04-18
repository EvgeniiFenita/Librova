add_library(LibrovaImporting STATIC
    ImportDiagnosticText.cpp
    ImportDiagnosticText.hpp
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
        LibrovaFoundation
        LibrovaImportConversion
        LibrovaParsing
    PRIVATE
        LibrovaBookDatabase
)

target_compile_features(LibrovaImporting PUBLIC cxx_std_20)
