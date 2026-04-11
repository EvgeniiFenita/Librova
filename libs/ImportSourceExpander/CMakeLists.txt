add_library(LibrovaImportSourceExpander STATIC
    ImportDiagnostics.hpp
    ImportSourceExpander.cpp
    ImportSourceExpander.hpp
)

target_include_directories(LibrovaImportSourceExpander
    PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
)

target_link_libraries(LibrovaImportSourceExpander
    PRIVATE
        LibrovaLogging
        LibrovaUnicode
)

target_compile_features(LibrovaImportSourceExpander PUBLIC cxx_std_20)
