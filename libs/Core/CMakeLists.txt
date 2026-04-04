add_library(LibrovaCore STATIC
    Version.cpp
)

target_include_directories(LibrovaCore
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_compile_features(LibrovaCore PUBLIC cxx_std_20)
target_compile_definitions(LibrovaCore PUBLIC LIBROVA_PRODUCT_VERSION="${LIBROVA_PRODUCT_VERSION}")
