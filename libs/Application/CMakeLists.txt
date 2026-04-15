add_library(LibrovaApplication STATIC
    ImportRollbackService.cpp
    ImportRollbackService.hpp
    ImportWorkloadPlanner.cpp
    ImportWorkloadPlanner.hpp
    LibraryCatalogFacade.cpp
    LibraryCatalogFacade.hpp
    LibraryExportFacade.cpp
    LibraryExportFacade.hpp
    LibraryImportFacade.cpp
    LibraryImportFacade.hpp
    LibraryTrashFacade.cpp
    LibraryTrashFacade.hpp
    StructuredProgressMapper.cpp
    StructuredProgressMapper.hpp
)

find_path(BSHOSHANY_THREAD_POOL_INCLUDE_DIRS "BS_thread_pool.hpp")

target_include_directories(LibrovaApplication
    PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
    PRIVATE
        ${BSHOSHANY_THREAD_POOL_INCLUDE_DIRS}
)

target_link_libraries(LibrovaApplication
    PUBLIC
        LibrovaDomain
        LibrovaImporting
        LibrovaImportSourceExpander
        LibrovaManagedTrash
        LibrovaZipImporting
    PRIVATE
        LibrovaLogging
        LibrovaManagedFileEncoding
        LibrovaManagedPaths
        LibrovaUnicode
)

target_compile_features(LibrovaApplication PUBLIC cxx_std_20)
