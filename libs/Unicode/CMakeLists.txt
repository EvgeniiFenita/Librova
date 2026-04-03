add_library(LibrovaUnicode STATIC
    UnicodeConversion.cpp
    UnicodeConversion.hpp
)

target_include_directories(LibrovaUnicode PUBLIC
    ${CMAKE_SOURCE_DIR}/libs
)

target_compile_features(LibrovaUnicode PUBLIC cxx_std_20)
