add_library(LibrovaCore STATIC
    Version.cpp
)

target_include_directories(LibrovaCore
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_compile_features(LibrovaCore PUBLIC cxx_std_20)
