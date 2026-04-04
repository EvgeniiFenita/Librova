find_package(ZLIB REQUIRED)

add_library(LibrovaManagedFileEncoding STATIC
    ManagedFileEncoding.cpp
    ManagedFileEncoding.hpp
)

target_include_directories(LibrovaManagedFileEncoding PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(LibrovaManagedFileEncoding PUBLIC
    LibrovaDomain
    LibrovaUnicode
    ZLIB::ZLIB
)

target_compile_features(LibrovaManagedFileEncoding PUBLIC cxx_std_20)
