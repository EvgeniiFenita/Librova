add_library(LibrovaImportConversion STATIC
    ImportConversionPolicy.cpp
    ImportConversionPolicy.hpp
)

target_include_directories(LibrovaImportConversion
    PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
)

target_link_libraries(LibrovaImportConversion
    PUBLIC
        LibrovaDomain
)

target_compile_features(LibrovaImportConversion PUBLIC cxx_std_20)
