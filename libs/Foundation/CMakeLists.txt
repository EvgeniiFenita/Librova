find_package(spdlog CONFIG REQUIRED)

add_library(LibrovaFoundation STATIC
    FileSystemUtils.cpp
    FileSystemUtils.hpp
    Logging.cpp
    Logging.hpp
    Sha256.cpp
    Sha256.hpp
    StringUtils.cpp
    StringUtils.hpp
    UnicodeConversion.cpp
    UnicodeConversion.hpp
    Version.cpp
    Version.hpp
)

target_include_directories(LibrovaFoundation PUBLIC
    ${CMAKE_SOURCE_DIR}/libs
)

target_compile_features(LibrovaFoundation PUBLIC cxx_std_20)

target_compile_definitions(LibrovaFoundation PUBLIC
    LIBROVA_PRODUCT_VERSION="${LIBROVA_PRODUCT_VERSION}"
)

target_link_libraries(LibrovaFoundation
    PUBLIC
        spdlog::spdlog_header_only
    PRIVATE
        bcrypt
)

if(WIN32)
    target_compile_definitions(LibrovaFoundation PUBLIC SPDLOG_WCHAR_FILENAMES)
endif()
