add_library(LibrovaProtoMapping STATIC
    ImportJobProtoMapper.cpp
    ImportJobProtoMapper.hpp
    LibraryCatalogProtoMapper.cpp
    LibraryCatalogProtoMapper.hpp
)

target_include_directories(LibrovaProtoMapping
    PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
)

target_link_libraries(LibrovaProtoMapping
    PUBLIC
        LibrovaApplication
        LibrovaApplicationJobs
        LibrovaDomain
        LibrovaProtoContracts
        LibrovaUnicode
)

target_compile_features(LibrovaProtoMapping PUBLIC cxx_std_20)
