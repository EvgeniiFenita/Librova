add_library(LibrovaApplication STATIC
    LibraryCatalogFacade.cpp
    LibraryCatalogFacade.hpp
    LibraryExportFacade.cpp
    LibraryExportFacade.hpp
    LibraryImportFacade.cpp
    LibraryImportFacade.hpp
    LibraryTrashFacade.cpp
    LibraryTrashFacade.hpp
)

target_include_directories(LibrovaApplication
    PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
)

target_link_libraries(LibrovaApplication
    PUBLIC
        LibrovaDomain
        LibrovaImporting
        LibrovaManagedTrash
        LibrovaZipImporting
    PRIVATE
        LibrovaLogging
)

target_compile_features(LibrovaApplication PUBLIC cxx_std_20)
