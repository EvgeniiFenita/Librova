add_library(LibrovaApplication STATIC
    LibraryImportFacade.cpp
    LibraryImportFacade.hpp
)

target_include_directories(LibrovaApplication
    PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
)

target_link_libraries(LibrovaApplication
    PUBLIC
        LibrovaDomain
        LibrovaImporting
        LibrovaZipImporting
)

target_compile_features(LibrovaApplication PUBLIC cxx_std_20)
