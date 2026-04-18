find_package(libzip CONFIG REQUIRED)
find_path(BSHOSHANY_THREAD_POOL_INCLUDE_DIRS "BS_thread_pool.hpp")

add_library(LibrovaImport STATIC
    ImportConversionPolicy.cpp
    ImportDiagnosticText.cpp
    ImportPerfTracker.cpp
    ImportSourceExpander.cpp
    SingleFileImportCoordinator.cpp
    WriterDispatchingRepository.cpp
    ZipImportCoordinator.cpp
)

target_include_directories(LibrovaImport PUBLIC "${CMAKE_SOURCE_DIR}/libs")

target_link_libraries(LibrovaImport
    PUBLIC
        LibrovaDomain
        LibrovaFoundation
        LibrovaParsing
    PRIVATE
        libzip::zip
)

target_include_directories(LibrovaImport PRIVATE ${BSHOSHANY_THREAD_POOL_INCLUDE_DIRS})
