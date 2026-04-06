add_library(LibrovaHashing STATIC
    Sha256.cpp
    Sha256.hpp
)

target_include_directories(LibrovaHashing
    PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
)

target_link_libraries(LibrovaHashing
    PRIVATE
        LibrovaUnicode
        bcrypt
)

target_compile_features(LibrovaHashing PUBLIC cxx_std_20)
