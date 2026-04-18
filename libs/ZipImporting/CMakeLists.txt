add_library(LibrovaZipImporting STATIC
    ZipImportCoordinator.cpp
    ZipImportCoordinator.hpp
)

find_package(libzip CONFIG REQUIRED)
find_path(BSHOSHANY_THREAD_POOL_INCLUDE_DIRS "BS_thread_pool.hpp")

target_include_directories(LibrovaZipImporting
    PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
    PRIVATE
        ${BSHOSHANY_THREAD_POOL_INCLUDE_DIRS}
)

target_link_libraries(LibrovaZipImporting
    PUBLIC
        LibrovaDomain
        LibrovaFoundation
        LibrovaImporting
        libzip::zip
)

target_compile_features(LibrovaZipImporting PUBLIC cxx_std_20)
