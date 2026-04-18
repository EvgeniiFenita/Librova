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

target_include_directories(LibrovaApplication
    PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
)

target_link_libraries(LibrovaApplication
    PUBLIC
        LibrovaDomain
        LibrovaImport
        LibrovaStorage
    PRIVATE
        LibrovaFoundation
)

target_compile_features(LibrovaApplication PUBLIC cxx_std_20)
